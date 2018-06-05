
#include <errno.h>

#include "postgres.h"
#include "postmaster/bgworker.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "utils/builtins.h"
#include "utils/uuid.h"

#include "./pg_cron.h"

#ifndef __PG_CRON_SIGNALS_H
#define __PG_CRON_SIGNALS_H

static bool got_sigterm = false; // Used as a flag for SIGTERM activation
static bool got_sighup = false;  // Used as a flag for SIGHUP activation

static void handle_sigterm(SIGNAL_ARGS)
{
  int save_errno = errno;

  elog(LOG, "%s shutting down", MyBgworkerEntry->bgw_name);

  got_sigterm = true;
  if (MyProc)
    SetLatch(&MyProc->procLatch);

  errno = save_errno;
}

static void handle_sighup(SIGNAL_ARGS)
{
  int save_errno = errno;

  // reload config here
  got_sighup = true;

  if (MyProc)
    SetLatch(&MyProc->procLatch);

  errno = save_errno;
}

#endif
