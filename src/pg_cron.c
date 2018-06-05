
#include <unistd.h>
#include <errno.h>

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "access/xact.h"
#include "executor/spi.h"
#include "executor/executor.h"
#include "lib/stringinfo.h"
#include "utils/snapmgr.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/uuid.h"
#include "catalog/pg_type.h"

#include "./pg_cron.h"
#include "./signals.h"

void _PG_init(void)
{
    BackgroundWorker worker;

    if (!process_shared_preload_libraries_in_progress)
        return;

    elog(LOG, "pg_cron: Init started!");

    // worker definition
    memset(&worker, 0, sizeof(worker));
    sprintf(worker.bgw_library_name, "pg_cron");
    snprintf(worker.bgw_name, BGW_MAXLEN, "pg_cron");
    // things start here
    // called dynamically
    sprintf(worker.bgw_function_name, "pg_cron_worker_main");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
    worker.bgw_notify_pid = 0;

    RegisterBackgroundWorker(&worker);
}

void pg_cron_worker_main(Datum main_arg)
{
    elog(LOG, "%s: pg_cron_worker_main()", MyBgworkerEntry->bgw_name);

    pqsignal(SIGTERM, handle_sigterm);
    pqsignal(SIGHUP, handle_sighup);

    /* We're now ready to receive signals */
    BackgroundWorkerUnblockSignals();

    /* Connect to our database */
    BackgroundWorkerInitializeConnection("postgres", NULL);

    while (!got_sigterm)
    {
        int ret;
        int rc;
        char *queries_ids[PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT];
        char *queries[PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT];

        /*
		* Background workers mustn't call usleep() or any direct equivalent:
		* instead, they may wait on their process latch, which sleeps as
		* necessary, but is awakened if postmaster dies.  That way the
		* background process goes away immediately in an emergency.
		*/
        rc = WaitLatch(&MyProc->procLatch,
                       WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
                       TASK_EXECUTION_INTERVAL * 1000L,
                       PG_WAIT_EXTENSION);
        ResetLatch(&MyProc->procLatch);

        /* emergency bailout if postmaster has died */
        if (rc & WL_POSTMASTER_DEATH)
            proc_exit(1);

        CHECK_FOR_INTERRUPTS();

        if (got_sighup)
        {
            got_sighup = false;
            ProcessConfigFile(PGC_SIGHUP);
        }

        /*
		 * Start a transaction on which we can run queries.  Note that each
		 * StartTransactionCommand() call should be preceded by a
		 * SetCurrentStatementStartTimestamp() call, which sets both the time
		 * for the statement we're about the run, and also the transaction
		 * start time.  Also, each other query sent to SPI should probably be
		 * preceded by SetCurrentStatementStartTimestamp(), so that statement
		 * start time is always up to date.
		 *
		 * The SPI_connect() call lets us run queries through the SPI manager,
		 * and the PushActiveSnapshot() call creates an "active" snapshot
		 * which is necessary for queries to have MVCC data to work on.
		 *
		 * The pgstat_report_activity() call makes our activity visible
		 * through the pgstat views.
		 */
        SetCurrentStatementStartTimestamp();
        StartTransactionCommand();
        SPI_connect();
        PushActiveSnapshot(GetTransactionSnapshot());

        /* We can now execute queries via SPI */
        ret = SPI_execute(PG_CRON_QUERY_GET_NEXT_TASKS, true, PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT);

        if (ret != SPI_OK_SELECT)
            elog(FATAL, "cannot select : error code %d", ret);

        memset(queries_ids, 0, sizeof(char *) * PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT);
        memset(queries, 0, sizeof(char *) * PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT);
        for (unsigned int i = 0; i < SPI_processed; i++)
        {
            bool isnull1;
            bool isnull2;
            Datum d1;
            Datum d2;

            d1 = GetAttributeByName(SPI_returntuple(SPI_tuptable->vals[i], SPI_tuptable->tupdesc),
                                    "id",
                                    &isnull1);
            d2 = GetAttributeByName(SPI_returntuple(SPI_tuptable->vals[i], SPI_tuptable->tupdesc),
                                    "query",
                                    &isnull2);

            // this should never be null
            if (isnull1 || isnull2)
            {
                elog(LOG, "%s: got null 'query' column", MyBgworkerEntry->bgw_name);
                queries_ids[i] = NULL;
                queries[i] = NULL;
            }
            else
            {
                // I should be using my_strdup (based on palloc) here, but SPI_finish frees them
                // @TODO: RTFM
                queries_ids[i] = strdup(uuid_to_cstring(DatumGetUUIDP(d1)));
                queries[i] = strdup(TextDatumGetCString(d2));
            }
        }

        /*
		 * And finish our transaction.
		 */
        SPI_finish();
        PopActiveSnapshot();
        CommitTransactionCommand();

        pg_cron_exec_tasks(queries_ids, queries);
    }

    proc_exit(0);
}

// @TODO: execution should be in multiple worker for non-blocking jobs
void pg_cron_exec_tasks(char **queries_ids, char **queries)
{
    for (unsigned int i = 0; i < PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT; i++)
    {
        if (queries_ids[i] == NULL || queries[i] == NULL)
            continue;

        pg_cron_exec_task(queries_ids[i], queries[i]);
        pg_cron_update_task_next_exec_time(queries_ids[i]);

        free(queries_ids[i]);
        free(queries[i]);
    }
}

void pg_cron_exec_task(char *query_id, char *query)
{
    int ret;

    elog(LOG, "%s: executing query %s: '%s'",
         MyBgworkerEntry->bgw_name, query_id, query);

    /*
    * Start a transaction on which we can run queries.  Note that each
    * StartTransactionCommand() call should be preceded by a
    * SetCurrentStatementStartTimestamp() call, which sets both the time
    * for the statement we're about the run, and also the transaction
    * start time.  Also, each other query sent to SPI should probably be
    * preceded by SetCurrentStatementStartTimestamp(), so that statement
    * start time is always up to date.
    *
    * The SPI_connect() call lets us run queries through the SPI manager,
    * and the PushActiveSnapshot() call creates an "active" snapshot
    * which is necessary for queries to have MVCC data to work on.
    *
    * The pgstat_report_activity() call makes our activity visible
    * through the pgstat views.
    */
    SetCurrentStatementStartTimestamp();
    StartTransactionCommand();
    SPI_connect();
    PushActiveSnapshot(GetTransactionSnapshot());

    /* We can now execute queries via SPI */
    ret = SPI_execute(query, false, 0);

    if (ret < 0)
        elog(FATAL, "failed to exec query %s, %s: error code %d",
             query_id, query, ret);

    /*
    * And finish our transaction.
    */
    SPI_finish();
    PopActiveSnapshot();
    CommitTransactionCommand();
}

void pg_cron_update_task_next_exec_time(char *query_id)
{
    int ret;
    char query[100];

    /*
    * Start a transaction on which we can run queries.  Note that each
    * StartTransactionCommand() call should be preceded by a
    * SetCurrentStatementStartTimestamp() call, which sets both the time
    * for the statement we're about the run, and also the transaction
    * start time.  Also, each other query sent to SPI should probably be
    * preceded by SetCurrentStatementStartTimestamp(), so that statement
    * start time is always up to date.
    *
    * The SPI_connect() call lets us run queries through the SPI manager,
    * and the PushActiveSnapshot() call creates an "active" snapshot
    * which is necessary for queries to have MVCC data to work on.
    *
    * The pgstat_report_activity() call makes our activity visible
    * through the pgstat views.
    */
    SetCurrentStatementStartTimestamp();
    StartTransactionCommand();
    SPI_connect();
    PushActiveSnapshot(GetTransactionSnapshot());

    sprintf(query, "SELECT * FROM pg_cron_update_task_next_exec_time('%s');", query_id);
    /* We can now execute queries via SPI */
    ret = SPI_execute(query, false, 0);

    if (ret < 0)
        elog(FATAL, "failed to exec update next exec_time for task '%s': error code %d", query_id, ret);

    /*
    * And finish our transaction.
    */
    SPI_finish();
    PopActiveSnapshot();
    CommitTransactionCommand();
}
