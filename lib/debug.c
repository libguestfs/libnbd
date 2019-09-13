/* NBD client library in userspace
 * Copyright (C) 2013-2019 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "internal.h"

int
nbd_unlocked_set_debug (struct nbd_handle *h, bool fl)
{
  h->debug = fl;
  return 0;
}

/* NB: may_set_error = false. */
int
nbd_unlocked_get_debug (struct nbd_handle *h)
{
  return h->debug;
}

int
nbd_unlocked_clear_debug_callback (struct nbd_handle *h)
{
  FREE_CALLBACK (h->debug_callback);
  return 0;
}

int
nbd_unlocked_set_debug_callback (struct nbd_handle *h,
                                 nbd_debug_callback debug_callback)
{
  /* This can't fail at the moment - see implementation above. */
  nbd_unlocked_clear_debug_callback (h);

  h->debug_callback = debug_callback;
  return 0;
}

/* Note this preserves the value of errno, making it safe to use in
 * all situations.
 */
void
nbd_internal_debug (struct nbd_handle *h, const char *fs, ...)
{
  int err, r;
  va_list args;
  char *msg = NULL;
  const char *context;

  /* The debug() wrapper checks this, but check it again in case
   * something calls nbd_internal_debug directly.
   */
  if (!h->debug) return;

  err = errno;

  context = nbd_internal_get_error_context ();

  va_start (args, fs);
  r = vasprintf (&msg, fs, args);
  va_end (args);
  if (r == -1)
    goto out;

  if (CALLBACK_IS_NOT_NULL (h->debug_callback))
    /* ignore return value */
    CALL_CALLBACK (h->debug_callback, context, msg);
  else
    fprintf (stderr, "libnbd: debug: %s: %s: %s\n",
             h->hname, context ? : "unknown", msg);
 out:
  free (msg);
  errno = err;
  return;
}
