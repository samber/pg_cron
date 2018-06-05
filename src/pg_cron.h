
#ifndef __PG_CRON_H

#define __PG_CRON_H

#include "postgres.h"
#include "utils/builtins.h"

#define PG_CRON_QUERY_GET_NEXT_TASKS_LIMIT 10
#define PG_CRON_QUERY_GET_NEXT_TASKS "SELECT * FROM _cron_tasks \
WHERE next_exec < NOW() \
ORDER BY next_exec ASC \
LIMIT 10;"
#define TASK_EXECUTION_INTERVAL 1 // Duration between each task execution (in seconds)

void _PG_init(void);
void pg_cron_worker_main(Datum);
void pg_cron_exec_tasks(char **, char **);
void pg_cron_exec_task(char *, char *);
void pg_cron_update_task_next_exec_time(char *);

time_t _get_next_exec_time(char *schedule);
Datum pg_cron_get_next_exec_time(PG_FUNCTION_ARGS);

char *my_strdup(const char *);
char *my_strdup_text(const text *);
char *uuid_to_cstring(pg_uuid_t *);

#endif
