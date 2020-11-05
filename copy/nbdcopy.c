/* NBD client library in userspace.
 * Copyright (C) 2020 Red Hat Inc.
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnbd.h>

#include "nbdcopy.h"

bool progress;                  /* -p flag */
struct rw src, dst;             /* The source and destination. */

static bool is_nbd_uri (const char *s);
static int open_local (const char *prog,
                       const char *filename, bool writing, struct rw *rw);

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Copy to and from an NBD server:\n"
"\n"
"    nbdcopy nbd://example.com local.img\n"
"    nbdcopy nbd://example.com - | file -\n"
"    nbdcopy local.img nbd://example.com\n"
"    cat disk1 disk2 | nbdcopy - nbd://example.com\n"
"\n"
"Please read the nbdcopy(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

static void
display_version (void)
{
  printf ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

int
main (int argc, char *argv[])
{
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    LONG_OPTIONS,
    SHORT_OPTIONS,
  };
  const char *short_options = "pV";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "progress",           no_argument,       NULL, 'p' },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "version",            no_argument,       NULL, 'V' },
    { NULL }
  };
  int c;
  size_t i;
  const char *src_arg, *dst_arg;

  for (;;) {
    c = getopt_long (argc, argv, short_options, long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
    case HELP_OPTION:
      usage (stdout, EXIT_SUCCESS);

    case LONG_OPTIONS:
      for (i = 0; long_options[i].name != NULL; ++i) {
        if (strcmp (long_options[i].name, "long-options") != 0 &&
            strcmp (long_options[i].name, "short-options") != 0)
          printf ("--%s\n", long_options[i].name);
      }
      exit (EXIT_SUCCESS);

    case SHORT_OPTIONS:
      for (i = 0; short_options[i]; ++i) {
        if (short_options[i] != ':' && short_options[i] != '+')
          printf ("-%c\n", short_options[i]);
      }
      exit (EXIT_SUCCESS);

    case 'p':
      progress = true;
      break;

    case 'V':
      display_version ();
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* There must be exactly 2 parameters following. */
  if (argc - optind != 2)
    usage (stderr, EXIT_FAILURE);

  src_arg = argv[optind];
  dst_arg = argv[optind+1];
  src.t = is_nbd_uri (src_arg) ? NBD : LOCAL;
  dst.t = is_nbd_uri (dst_arg) ? NBD : LOCAL;

  /* Prevent copying between local files or devices.  It's unlikely
   * this program will ever be better than highly tuned utilities like
   * cp.
   */
  if (src.t == LOCAL && dst.t == LOCAL) {
    fprintf (stderr,
             "%s: this tool does not let you copy between local files, use\n"
             "cp(1) or dd(1) instead.\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Set up the source side. */
  src.name = src_arg;
  if (src.t == LOCAL) {
    src.u.local.fd = open_local (argv[0], src_arg, false, &src);
  }
  else {
    src.u.nbd = nbd_create ();
    if (src.u.nbd == NULL) {
      fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_set_uri_allow_local_file (src.u.nbd, true); /* Allow ?tls-psk-file. */
    if (nbd_connect_uri (src.u.nbd, src_arg) == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], src_arg, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Set up the destination side. */
  dst.name = dst_arg;
  if (dst.t == LOCAL) {
    dst.u.local.fd = open_local (argv[0], dst_arg, true /* writing */, &dst);
  }
  else {
    dst.u.nbd = nbd_create ();
    if (dst.u.nbd == NULL) {
      fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_set_uri_allow_local_file (dst.u.nbd, true); /* Allow ?tls-psk-file. */
    if (nbd_connect_uri (dst.u.nbd, dst_arg) == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], dst_arg, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    /* Obviously this is not going to work if the server is
     * advertising read-only, so fail early with a nice error message.
     */
    if (nbd_is_read_only (dst.u.nbd)) {
      fprintf (stderr, "%s: %s: "
               "this NBD server is read-only, cannot write to it\n",
               argv[0], dst_arg);
      exit (EXIT_FAILURE);
    }
  }

  /* Calculate the source and destination sizes.  We set these to -1
   * if the size is not known (because it's a stream).  Note that for
   * local types, open_local set something in *.size already.
   */
  if (src.t == NBD) {
    src.size = nbd_get_size (src.u.nbd);
    if (src.size == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], src_arg, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
  if (dst.t == LOCAL && S_ISREG (dst.u.local.stat.st_mode)) {
    /* If the destination is an ordinary file then the original file
     * size doesn't matter.  Truncate it to the source size.  But
     * truncate it to zero first so the file is completely empty and
     * sparse.
     */
    dst.size = src.size;
    if (ftruncate (dst.u.local.fd, 0) == -1 ||
        ftruncate (dst.u.local.fd, dst.size) == -1) {
      perror ("truncate");
      exit (EXIT_FAILURE);
    }
  }
  else if (dst.t == NBD) {
    dst.size = nbd_get_size (dst.u.nbd);
    if (dst.size == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], dst_arg, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Check if the source is bigger than the destination, since that
   * would truncate (ie. lose) data.  Copying from smaller to larger
   * is OK.
   */
  if (src.size >= 0 && dst.size >= 0 && src.size > dst.size) {
    fprintf (stderr,
             "nbdcopy: error: destination size is smaller than source size\n");
    exit (EXIT_FAILURE);
  }

  /* Start copying. */
  synch_copying ();

  /* Shut down the source side. */
  if (src.t == LOCAL) {
    if (close (src.u.local.fd) == -1) {
      fprintf (stderr, "%s: %s: close: %m\n", argv[0], src_arg);
      exit (EXIT_FAILURE);
    }
  }
  else {
    if (nbd_shutdown (src.u.nbd, 0) == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], src_arg, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_close (src.u.nbd);
  }

  /* Shut down the destination side. */
  if (dst.t == LOCAL) {
    if (close (dst.u.local.fd) == -1) {
      fprintf (stderr, "%s: %s: close: %m\n", argv[0], dst_arg);
      exit (EXIT_FAILURE);
    }
  }
  else {
    if (nbd_shutdown (dst.u.nbd, 0) == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], dst_arg, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_close (dst.u.nbd);
  }

  exit (EXIT_SUCCESS);
}

/* Return true if the parameter is an NBD URI. */
static bool
is_nbd_uri (const char *s)
{
  return
    strncmp (s, "nbd:", 4) == 0 ||
    strncmp (s, "nbds:", 5) == 0 ||
    strncmp (s, "nbd+unix:", 9) == 0 ||
    strncmp (s, "nbds+unix:", 10) == 0 ||
    strncmp (s, "nbd+vsock:", 10) == 0 ||
    strncmp (s, "nbds+vsock:", 11) == 0;
}

/* Open a local (non-NBD) file, ie. a file, device, or "-" for stdio.
 * Returns the open file descriptor which the caller must close.
 *
 * “writing” is true if this is the destination parameter.
 * “rw->u.local.stat” and “rw->size” return the file stat and size,
 * but size can be returned as -1 if we don't know the size (if it's a
 * pipe or stdio).
 */
static int
open_local (const char *prog,
            const char *filename, bool writing, struct rw *rw)
{
  int flags, fd;

  if (strcmp (filename, "-") == 0) {
    fd = writing ? STDOUT_FILENO : STDIN_FILENO;
    if (writing && isatty (fd)) {
      fprintf (stderr, "%s: refusing to write to tty\n", prog);
      exit (EXIT_FAILURE);
    }
  }
  else {
    /* If it's a block device and we're writing we don't want to turn
     * it into a truncated regular file by accident, so try to open
     * without O_CREAT first.
     */
    flags = writing ? O_WRONLY : O_RDONLY;
    fd = open (filename, flags);
    if (fd == -1) {
      if (writing) {
        /* Try again, with more flags. */
        flags |= O_TRUNC|O_CREAT|O_EXCL;
        fd = open (filename, flags, 0644);
      }
      if (fd == -1) {
        perror (filename);
        exit (EXIT_FAILURE);
      }
    }
  }

  if (fstat (fd, &rw->u.local.stat) == -1) {
    perror (filename);
    exit (EXIT_FAILURE);
  }
  if (S_ISBLK (rw->u.local.stat.st_mode)) {
    /* Block device. */
    rw->size = lseek (fd, 0, SEEK_END);
    if (rw->size == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
    if (lseek (fd, 0, SEEK_SET) == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
  }
  else if (S_ISREG (rw->u.local.stat.st_mode)) {
    /* Regular file. */
    rw->size = rw->u.local.stat.st_size;
  }
  else {
    /* Probably stdin/stdout, a pipe or a socket.  Set size == -1
     * which means don't know.
     */
    rw->size = -1;
  }

  return fd;
}
