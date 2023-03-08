/* NBD client library in userspace
 * Copyright Red Hat
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
#include <errno.h>

#include <pthread.h>

#include "internal.h"

struct last_error {
  const char *context;         /* Current function context. */
  char *error;                 /* Error message string. */
  int errnum;                  /* errno value (0 if not available). */
};

/* Thread-local storage of the last error. */
static pthread_key_t errors_key;

static void free_errors_key (void *vp) LIBNBD_ATTRIBUTE_NONNULL (1);

/* Create the thread-local key when the library is loaded. */
static void errors_key_create (void) __attribute__ ((constructor));

static void
errors_key_create (void)
{
  int err;

  err = pthread_key_create (&errors_key, free_errors_key);
  if (err != 0) {
    fprintf (stderr, "%s: %s: %s\n", "libnbd", "pthread_key_create",
             strerror (err));
    abort ();
  }
}

/* Destroy the thread-local key when the library is unloaded. */
static void errors_key_destroy (void) __attribute__ ((destructor));

static void
errors_key_destroy (void)
{
  struct last_error *last_error = pthread_getspecific (errors_key);

  /* "No destructor functions shall be invoked by
   * pthread_key_delete().  Any destructor function that may have been
   * associated with key shall no longer be called upon thread exit."
   */
  if (last_error != NULL) {
    free (last_error->error);
    free (last_error);
  }

  /* We could do this, but that causes a race condition described here:
   * https://listman.redhat.com/archives/libguestfs/2023-March/031002.html
   */
  //pthread_key_delete (errors_key);
}

/* This is called when a thread exits, to free the thread-local data
 * for that thread.  Note that vp != NULL, guaranteed by pthread.
 */
static void
free_errors_key (void *vp)
{
  struct last_error *last_error = vp;

  free (last_error->error);
  free (last_error);
}

static struct last_error *
allocate_last_error_on_demand (void)
{
  struct last_error *last_error = pthread_getspecific (errors_key);

  if (!last_error) {
    last_error = calloc (1, sizeof *last_error);
    if (last_error) {
      int err = pthread_setspecific (errors_key, last_error);
      if (err != 0) {
        /* This is not supposed to happen (XXX). */
        fprintf (stderr, "%s: %s: %s\n", "libnbd", "pthread_setspecific",
                 strerror (err));
      }
    }
  }
  return last_error;
}

/* Called on entry to any API function that can call an error function
 * (see generator "may_set_error") to reset the error context.  The
 * 'context' parameter is the name of the function.
 */
void
nbd_internal_set_error_context (const char *context)
{
  struct last_error *last_error = allocate_last_error_on_demand ();

  if (!last_error)
    return;
  last_error->context = context;
}

void
nbd_internal_set_last_error (int errnum, char *error)
{
  struct last_error *last_error = allocate_last_error_on_demand ();

  if (!last_error) {
    /* At least we shouldn't lose the error. */
    perror ("nbd_internal_set_last_error: calloc");
    fprintf (stderr, "nbd_internal_set_last_error: lost error: %s (%d)\n",
             error, errnum);
    return;
  }

  free (last_error->error);
  last_error->error = error;
  last_error->errnum = errnum;
}

const char *
nbd_internal_get_error_context (void)
{
  struct last_error *last_error = allocate_last_error_on_demand ();

  return last_error ? last_error->context : NULL;
}

const char *
nbd_get_error (void)
{
  struct last_error *last_error = pthread_getspecific (errors_key);

  if (!last_error)
    return NULL;
  return last_error->error;
}

int
nbd_get_errno (void)
{
  struct last_error *last_error = pthread_getspecific (errors_key);

  if (!last_error)
    return 0;
  return last_error->errnum;
}
