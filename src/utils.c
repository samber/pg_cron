
#include <string.h>

#include "postgres.h"
#include "fmgr.h"
#include "utils/uuid.h"

#include "./pg_cron.h"

/**
 * Like strdup but with `pmalloc`
 */
char *my_strdup(const char *in)
{
  char *out;
  size_t len;

  len = strlen(in) + 1;
  // @TODO: palloc may return NULL
  out = palloc(len);
  strncpy(out, in, len);
  return out;
}

char *my_strdup_text(const text *in)
{
  char *out;
  size_t len;

  len = VARSIZE(in) - VARHDRSZ;
  // @TODO: palloc may return NULL
  out = palloc(len + 1);
  memcpy(out, VARDATA(in), len);
  out[len] = 0;
  return out;
}

char *uuid_to_cstring(pg_uuid_t *uuid)
{
  static const int CSTRING_UUID_LEN = 36;
  static const char hex_chars[] = "0123456789abcdef";
  char *out;

  // @TODO: palloc may return NULL
  out = palloc(CSTRING_UUID_LEN + 1);
  memset(out, 0, CSTRING_UUID_LEN + 1);

  for (int i = 0; i < UUID_LEN; i++)
  {
    int hi;
    int lo;

    /*
		 * We print uuid values as a string of 8, 4, 4, 4, and then 12
		 * hexadecimal characters, with each group is separated by a hyphen
		 * ("-"). Therefore, add the hyphens at the appropriate places here.
		 */
    if (i == 4 || i == 6 || i == 8 || i == 10)
      out[strlen(out)] = '-';

    hi = uuid->data[i] >> 4;
    lo = uuid->data[i] & 0x0F;

    out[strlen(out)] = hex_chars[hi];
    out[strlen(out)] = hex_chars[lo];
  }
  return out;
}
