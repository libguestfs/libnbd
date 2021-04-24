/* NBD client library in userspace
 * Copyright (C) 2020-2021 Red Hat Inc.
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

/* Test nbd_set_list_exports against an NBD server. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <libnbd.h>

#include "../tests/requires.h"

#ifdef NEEDS_TMPFILE
#define TMPFILE tmp
static char tmp[] = "/tmp/nbdXXXXXX";

static void
unlink_tmpfile (void)
{
  unlink (TMPFILE);
}
#endif /* NEEDS_TMPFILE */

static const char *exports[] = { EXPORTS };
#define nr_exports  (sizeof exports / sizeof exports[0])
static const char *descriptions[nr_exports] = { DESCRIPTIONS };
static char *actual[nr_exports][2]; /* (name, description)'s received */

static char *progname;

static int
append (void *opaque, const char *name, const char *description)
{
  size_t *ip = opaque;
  size_t i = *ip;

  if (i >= nr_exports) {
    fprintf (stderr, "%s: server returned more exports than expected",
             progname);
    exit (EXIT_FAILURE);
  }

  printf ("append: i=%zu name=\"%s\" description=\"%s\"\n",
          i, name, description);
  fflush (stdout);

  actual[i][0] = strdup (name);
  actual[i][1] = strdup (description);
  if (!actual[i][0] || !actual[i][1]) abort ();

  (*ip)++;
  return 0;
}

static int
compare_actuals (const void *vp1, const void *vp2)
{
  return strcmp (* (char * const *) vp1, * (char * const *) vp2);
}

static void
free_actuals (void)
{
  size_t i;

  for (i = 0; i < nr_exports; ++i) {
    free (actual[i][0]);
    free (actual[i][1]);
  }
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  size_t i = 0;

  progname = argv[0];

  /* Check requirements or skip the test. */
#ifdef REQUIRES
  REQUIRES
#endif

#ifdef NEEDS_TMPFILE
  /* Create a sparse temporary file. */
  int fd = mkstemp (TMPFILE);
  if (fd == -1 ||
      ftruncate (fd, 1024) == -1 ||
      close (fd) == -1) {
    perror (TMPFILE);
    exit (EXIT_FAILURE);
  }
  atexit (unlink_tmpfile);
#endif

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Set option mode in the handle. */
  nbd_set_opt_mode (nbd, true);

  /* Run the NBD server. */
  char *args[] = { SERVER, SERVER_PARAMS, NULL };
#if SOCKET_ACTIVATION
#define NBD_CONNECT nbd_connect_systemd_socket_activation
#else
#define NBD_CONNECT nbd_connect_command
#endif
  if (NBD_CONNECT (nbd, args) == -1 ||
      nbd_opt_list (nbd, (nbd_list_callback) {
          .callback = append, .user_data = &i}) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Check for expected number of exports. */
  if (i != nr_exports) {
    fprintf (stderr, "%s: expected %zu export, but got %zu\n",
             argv[0], nr_exports, i);
    exit (EXIT_FAILURE);
  }

  /* Servers won't always return the list of exports in a particular
   * order.  In particular nbdkit-file-plugin returns them in the
   * order they are read from the directory by readdir.  Sort before
   * comparing.
   */
  qsort (actual, nr_exports, sizeof actual[0], compare_actuals);

  for (i = 0; i < nr_exports; ++i) {
    if (strcmp (actual[i][0], exports[i]) != 0) {
      fprintf (stderr, "%s: expected export \"%s\", but got \"%s\"\n",
               progname, exports[i], actual[i][0]);
      exit (EXIT_FAILURE);
    }
    if (strcmp (actual[i][1], descriptions[i]) != 0) {
      fprintf (stderr, "%s: expected description \"%s\", but got \"%s\"\n",
               progname, descriptions[i], actual[i][1]);
      exit (EXIT_FAILURE);
    }
  }

  nbd_opt_abort (nbd);
  nbd_close (nbd);
  free_actuals ();
  exit (EXIT_SUCCESS);
}
