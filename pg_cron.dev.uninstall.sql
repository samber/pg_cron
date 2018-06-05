
DROP FUNCTION pg_cron_get_next_exec_time;

DROP FUNCTION pg_cron_get_task_list;
DROP FUNCTION pg_cron_get_task;
DROP FUNCTION pg_cron_create_task;
DROP FUNCTION pg_cron_drop_task;
DROP FUNCTION pg_cron_update_task_next_exec_time;

DROP FUNCTION pg_cron_get_task_history_list;
DROP FUNCTION pg_cron_get_task_history;

DROP TABLE _cron_tasks_history;
DROP TABLE _cron_tasks;

DROP EXTENSION pg_cron;
