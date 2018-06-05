
# Postgresql extension: adding cron-like features

## Why ?

Just for fun ¯\_(ツ)_/¯

I wanted to discover PostgreSQL extension API and pg background workers.

## What ?

A simple PostgreSQL extension for scheduling SQL queries.

Supports cron expressions with `second` field.

Scheduling is based on UTC time.

## How ?

- `pg_cron_create_task(schedule VARCHAR, sql_query VARCHAR)`: schedules a new task
- `pg_cron_get_task_list()`: lists existing tasks
- `pg_cron_get_task(task_id UUID)`: gets a task
- `pg_cron_drop_task(task_id UUID)`: deletes a task

Insert a new row into table `foo` every weekday at 7am:

```
SELECT * FROM pg_cron_create_task('0 0 7 ? * MON-FRI', 'INSERT INTO foo (bar) VALUES (42);');
                  id                  |     schedule      |               query                |       next_exec
--------------------------------------+-------------------+------------------------------------+------------------------
 a63b67fd-f739-48de-bcea-86e67f112c41 | 0 0 7 ? * MON-FRI | INSERT INTO foo (bar) VALUES (42); | 2018-06-06 07:00:00+00
(1 row)
```

List tasks:

```
SELECT * FROM pg_cron_get_task_list();
                  id                  |     schedule      |               query               |       next_exec
--------------------------------------+-------------------+-----------------------------------+------------------------
 a63b67fd-f739-48de-bcea-86e67f112c41 | 0 0 7 ? * MON-FRI | INSERT INTO foo (bar) VALUES (42); | 2018-06-06 07:00:00+00
(1 row)
```

Drop task:

```
SELECT * FROM pg_cron_drop_task('a63b67fd-f739-48de-bcea-86e67f112c41');
 pg_cron_drop_task
-------------------
                 1
(1 row)
```

## Install

```
cd /usr/lib/postgresql/10/lib
git clone git@github.com:samber/pg_cron.git
cd pg_cron
make install
```

Append `shared_preload_libraries = 'pg_cron'` to your postgresql.conf and restart PG.

```
psql --command 'CREATE EXTENSION pg_cron CASCADE'
```

```
$ pg_ctl restart
```

## Demo

```
$ docker-compose build
$ docker-compose up -d
````

```
$ docker-compose exec postgres psql -U postgres --command 'CREATE EXTENSION pg_cron CASCADE'
NOTICE:  installing required extension "pgcrypto"
CREATE EXTENSION

$ docker-compose restart
```

```
$ docker-compose exec postgres psql -U postgres

psql (10.4 (Debian 10.4-2.pgdg90+1))
Type "help" for help.

postgres=# CREATE TABLE test (id UUID PRIMARY KEY DEFAULT gen_random_uuid(), a INT DEFAULT NULL);
CREATE TABLE

postgres=# SELECT * FROM pg_cron_create_task('*/10 * * * * *', 'INSERT INTO test (a) values (42);');

postgres=# SELECT COUNT(*) FROM test; \watch
```

## Warning

- :warning: This is *NOT* production ready. And it will never be ;)
- Do NOT pay attention to code quality 🤮

## Todo

- filling _cron_tasks_history with execution outputs and errors
- async task execution (today tasks are executed in sequence) - pg threading is quite tricky :warning:
- 1 cron manager bgworker and a pool of bgworker
- safe memory management (cf @TODO + sed 's/malloc/palloc/g' + sed 's/free/pfree/g')

## Contributing

Test command:

```
$ docker-compose build \
    && docker-compose down -v \
    && docker-compose up -d \
    && sleep 2 \
    && docker-compose exec postgres psql -U postgres --command 'CREATE EXTENSION pg_cron CASCADE' \
    && docker-compose stop
    && docker-compose up

$ docker-compose exec postgres psql -U postgres --command "SELECT * FROM pg_cron_create_task('* * * * * *', 'SELECT 1;');"
```

## Useful resources for building extensions

- https://www.postgresql.org/docs/10/static/bgworker.html
- https://github.com/postgres/postgres/blob/master/src/test/modules/worker_spi/worker_spi.c
- https://www.postgresql.org/docs/9.4/static/xfunc-c.html#AEN55804
- http://big-elephants.com/2015-10/writing-postgres-extensions-part-i/
- PG headers: https://doxygen.postgresql.org/
