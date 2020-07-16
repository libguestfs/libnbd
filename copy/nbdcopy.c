/* NBD client library in userspace
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
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnbd.h>

/* XXX For future work, see TODO file. */

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

static bool progress;

static void upload (const char *filename, int fd,
                    struct stat *filestat, off_t filesize,
                    struct nbd_handle *nbd);
static void download (struct nbd_handle *nbd,
                      const char *filename, int fd,
                      struct stat *filestat, off_t filesize);
static void progress_bar (off_t pos, int64_t size);

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

static int
open_local (const char *prog,
            const char *filename, bool writing,
            struct stat *statbuf, off_t *size_rtn)
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

  if (fstat (fd, statbuf) == -1) {
    perror (filename);
    exit (EXIT_FAILURE);
  }
  if (S_ISBLK (statbuf->st_mode)) {
    /* Block device. */
    *size_rtn = lseek (fd, 0, SEEK_END);
    if (*size_rtn == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
    if (lseek (fd, 0, SEEK_SET) == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
  }
  else if (S_ISREG (statbuf->st_mode)) {
    /* Reguar file. */
    *size_rtn = statbuf->st_size;
    if (writing) {
      /* Truncate the file since we might not have done that above. */
      if (ftruncate (fd, 0) == -1) {
        perror ("truncate");
        exit (EXIT_FAILURE);
      }
    }
  }
  else {
    /* Probably stdin/stdout, a pipe or a socket.  Set *size_rtn == -1
     * which means don't know.
     */
    *size_rtn = -1;
  }

  return fd;
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
  int c, fd;
  size_t i;
  const char *src, *dst;
  bool src_is_uri, dst_is_uri;
  struct nbd_handle *nbd;
  struct stat filestat;
  off_t filesize;

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

  src = argv[optind];
  dst = argv[optind+1];

  /* Currently you cannot use this tool to copy from NBD to NBD
   * although we may add this in future.
   */
  src_is_uri = is_nbd_uri (src);
  dst_is_uri = is_nbd_uri (dst);
  if (src_is_uri && dst_is_uri) {
    fprintf (stderr,
             "%s: currently this tool does not allow you to copy between\n"
             "NBD servers.  Use: nbdcopy URI - | nbdcopy - URI instead.  This\n"
             "restriction may be removed in a future version.\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Prevent copying between local files or devices.  There are
   * better ways to do that.
   */
  if (!src_is_uri && !dst_is_uri) {
    fprintf (stderr,
             "%s: this tool does not let you copy between local files, use\n"
             "cp(1) or dd(1) instead.\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_uri (nbd, src_is_uri ? src : dst) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Open the local file side. */
  fd = open_local (argv[0], src_is_uri ? dst : src,
                   src_is_uri /* writing */,
                   &filestat, &filesize);

  /* Begin the operation. */
  if (dst_is_uri)
    upload (src, fd, &filestat, filesize, nbd);
  else
    download (nbd, dst, fd, &filestat, filesize);

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_close (nbd);

  if (close (fd) == -1) {
    perror ("close");
    exit (EXIT_FAILURE);
  }

  exit (EXIT_SUCCESS);
}

static char buf[MAX_REQUEST_SIZE];

static void
upload (const char *filename, int fd, struct stat *filestat, off_t filesize,
        struct nbd_handle *nbd)
{
  off_t pos = 0;
  ssize_t r;

  while ((r = read (fd, buf, sizeof buf)) > 0) {
    if (nbd_pwrite (nbd, buf, r, pos, 0) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    pos += r;
    if (progress)
      progress_bar (pos, (int64_t) filesize);
  }
  if (r == -1) {
    perror (filename);
    exit (EXIT_FAILURE);
  }

  if (progress)
    progress_bar (1, 1);
}

static void
download (struct nbd_handle *nbd,
          const char *filename, int fd, struct stat *filestat, off_t filesize)
{
  int64_t size;
  off_t pos = 0;
  size_t n;
  char *p;
  ssize_t r;

  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (S_ISBLK (filestat->st_mode) && filesize != -1 && size > filesize) {
    fprintf (stderr,
             "nbdcopy: block device is smaller than NBD source device\n");
    exit (EXIT_FAILURE);
  }

  while (pos < size) {
    p = buf;
    n = sizeof buf;
    if (n > size-pos) n = size-pos;
    if (nbd_pread (nbd, p, n, pos, 0) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    while (n > 0) {
      r = write (fd, p, n);
      if (r == -1) {
        perror (filename);
        exit (EXIT_FAILURE);
      }
      p += r;
      n -= r;
      pos += r;
      if (progress)
        progress_bar (pos, size);
    }
  }

  if (progress)
    progress_bar (1, 1);
}

/* Display the progress bar. */
static void
progress_bar (off_t pos, int64_t size)
{
  static const char *spinner[] = { "◐", "◓", "◑", "◒" };
  static int tty = -1;
  double frac = (double) pos / size;
  char msg[80];
  size_t n, i;

  if (tty == -1) {
    tty = open ("/dev/tty", O_WRONLY);
    if (tty == -1)
      return;
  }

  if (frac < 0) frac = 0; else if (frac > 1) frac = 1;

  if (frac == 1) {
    snprintf (msg, sizeof msg, "● 100%% [********************]\n");
    progress = false; /* Don't print any more progress bar messages. */
  } else {
    snprintf (msg, sizeof msg, "%s %3d%% [--------------------]\r",
              spinner[(int)(4*frac)], (int)(100*frac));
    n = strcspn (msg, "-");
    for (i = 0; i < 20*frac; ++i)
      msg[n+i] = '*';
  }

  n = write (tty, msg, strlen (msg));
}
