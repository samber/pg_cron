-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_cron" to load this file. \quit


--
-- SCHEMA
--

CREATE TABLE _cron_tasks (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  schedule character varying NOT NULL,
  query character varying NOT NULL,
  next_exec timestamp with time zone NOT NULL
);
CREATE INDEX idx_cron_tasks_next_exec ON _cron_tasks (next_exec DESC);

CREATE TABLE _cron_tasks_history (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  cron_task_id UUID REFERENCES _cron_tasks(id) NOT NULL,
  output JSONB NOT NULL DEFAULT '[]'::jsonb,
  executed_at timestamp with time zone NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_cron_tasks_history_executed_at ON _cron_tasks_history (executed_at DESC);


--
-- INTERFACE BINDING
--

CREATE FUNCTION pg_cron_get_next_exec_time(schedule character varying) RETURNS timestamp with time zone
    AS '$libdir/pg_cron', 'pg_cron_get_next_exec_time'
    LANGUAGE C IMMUTABLE;


--
-- FUNCTIONS
--

-- tasks manipulation
CREATE OR REPLACE FUNCTION pg_cron_get_task_list() RETURNS TABLE(id UUID, schedule text, query text, next_exec timestamp with time zone) AS $$
    SELECT * FROM _cron_tasks;
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION pg_cron_get_task(tid UUID) RETURNS TABLE(id UUID, schedule text, query text, next_exec timestamp with time zone) AS $$
    SELECT * FROM _cron_tasks
        WHERE id = tid;
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION pg_cron_create_task(schedule character varying, query character varying) RETURNS TABLE(id UUID, schedule text, query text, next_exec timestamp with time zone) AS $$
    INSERT INTO _cron_tasks (schedule, query, next_exec)
        VALUES (schedule, query, (SELECT * FROM pg_cron_get_next_exec_time(schedule)))
        RETURNING *;
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION pg_cron_drop_task(tid UUID) RETURNS INT AS $$
    WITH rows AS (
        DELETE FROM _cron_tasks
            WHERE id = tid
            RETURNING 1
    )
    SELECT COUNT(*)::INT FROM rows;
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION pg_cron_update_task_next_exec_time(tid UUID) RETURNS INT AS $$
    WITH rows AS (
        UPDATE _cron_tasks
            SET next_exec = pg_cron_get_next_exec_time(schedule)
            WHERE id = tid
            RETURNING 1
    )
    SELECT COUNT(*)::INT FROM rows;
$$ LANGUAGE sql;

-- tasks history manipulation
CREATE OR REPLACE FUNCTION pg_cron_get_task_history_list() RETURNS TABLE(id UUID, cron_task_id UUID, output JSONB, executed_at timestamp with time zone) AS $$
    SELECT * FROM _cron_tasks_history;
$$ LANGUAGE sql;

CREATE OR REPLACE FUNCTION pg_cron_get_task_history(tid UUID) RETURNS TABLE(id UUID, cron_task_id UUID, output JSONB, executed_at timestamp with time zone) AS
$$
    SELECT * FROM _cron_tasks_history
        WHERE cron_task_id = tid;
$$ LANGUAGE sql;
