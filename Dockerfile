
FROM postgres:10

RUN apt-get update \
    && apt-get install -y gcc make libpq-dev postgresql-server-dev-all postgresql-contrib vim

RUN echo "shared_preload_libraries = 'pg_cron'" >> /var/lib/postgresql/data/postgresql.conf \
    && echo "shared_preload_libraries = 'pg_cron'" >> /usr/share/postgresql/postgresql.conf.sample

COPY . /usr/lib/postgresql/10/lib/pg_cron
RUN cd /usr/lib/postgresql/10/lib/pg_cron \
    && make clean \
    && make install
