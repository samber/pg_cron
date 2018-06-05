EXTENSION = pg_cron
EXTVERSION = 0.0.1

DATA = $(wildcard $(EXTENSION)--*.sql)
MODULE_big = $(EXTENSION)
REGRESS =

# compilation configuration
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c)) $(patsubst %.c,%.o,$(wildcard libs/*/*.c))
PG_CPPFLAGS = -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-maybe-uninitialized -I$(libpq_srcdir)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
