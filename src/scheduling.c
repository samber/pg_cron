
#include <unistd.h>

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/uuid.h"

#include "../libs/ccronexpr/ccronexpr.h"
#include "./pg_cron.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pg_cron_get_next_exec_time);

time_t _get_next_exec_time(char *schedule)
{
  cron_expr expr;
  const char *err = NULL;
  time_t cur;
  time_t next;

  memset(&expr, 0, sizeof(expr));
  cron_parse_expr(schedule, &expr, &err);

  if (err)
  {
    pfree(schedule);
    ereport(ERROR,
            (
                errcode(ERRCODE_INTERNAL_ERROR),
                errmsg("%s", "invalid scheduling expression"),
                errdetail("%s", err),
                errhint("%s", "example: '0 */2 1-4 * * *'")));
  }

  cur = time(NULL);
  next = cron_next(&expr, cur);

  return next;
}

Datum pg_cron_get_next_exec_time(PG_FUNCTION_ARGS)
{
  text *arg1;
  char *schedule;
  time_t next;

  arg1 = PG_GETARG_TEXT_P(0);
  schedule = text_to_cstring(arg1);

  next = _get_next_exec_time(schedule);

  pfree(schedule);
  PG_RETURN_TIMESTAMPTZ(time_t_to_timestamptz(next));
}
