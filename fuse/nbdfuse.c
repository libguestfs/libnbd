/* NBD client library in userspace
 * Copyright (C) 2013-2021 Red Hat Inc.
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

/* FUSE support. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnbd.h>

#include "version.h"
#include "nbdfuse.h"

struct nbd_handle *nbd;
bool readonly;
bool file_mode = false;
struct timespec start_t;
char *filename;
uint64_t size;

static char *mountpoint;
static const char *pidfile;
static char *fuse_options;
static struct fuse *fuse;
static struct fuse_session *fuse_session;

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Mount NBD server as a virtual file:\n"
"\n"
#ifdef HAVE_LIBXML2
"    nbdfuse [-o FUSE-OPTION] [-P PIDFILE] [-r] [-s] MOUNTPOINT[/FILENAME] URI\n"
"\n"
"Other modes:\n"
"\n"
#endif
"    nbdfuse MOUNTPOINT[/FILENAME] [ CMD [ARGS ...] ]\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --command CMD [ARGS ...]\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --fd N\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --tcp HOST PORT\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --unix SOCKET\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --vsock CID PORT\n"
"\n"
"To unmount:\n"
"\n"
"    fusermount3 -u MOUNTPOINT\n"
"\n"
"Other options:\n"
"\n"
"    nbdfuse --help\n"
"    nbdfuse --fuse-help\n"
"    nbdfuse -V|--version\n"
"\n"
"Please read the nbdfuse(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

static void
fuse_help (const char *prog)
{
  static struct fuse_operations null_operations;
  const char *tmp_argv[] = { prog, "--help", NULL };
  fuse_main (2, (char **) tmp_argv, &null_operations, NULL);
  exit (EXIT_SUCCESS);
}

static bool
is_directory (const char *path)
{
  struct stat statbuf;

  if (stat (path, &statbuf) == -1)
    return false;
  return S_ISDIR (statbuf.st_mode);
}

static bool
is_regular_file (const char *path)
{
  struct stat statbuf;

  if (stat (path, &statbuf) == -1)
    return false;
  return S_ISREG (statbuf.st_mode);
}

int
main (int argc, char *argv[])
{
  enum {
    MODE_URI,
    MODE_COMMAND,
    MODE_FD,
    MODE_SQUARE_BRACKET, /* [ CMD ], same as --socket-activation*/
    MODE_SOCKET_ACTIVATION, /* --socket-activation */
    MODE_TCP,
    MODE_UNIX,
    MODE_VSOCK,
  } mode = MODE_URI;
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    FUSE_HELP_OPTION,
    LONG_OPTIONS,
    SHORT_OPTIONS,
  };
  /* Note the "+" means we stop processing as soon as we get to the
   * first non-option argument (the mountpoint) and then we parse the
   * rest of the command line without getopt.
   */
  const char *short_options = "+o:P:rsV";
  const struct option long_options[] = {
    { "fuse-help",          no_argument,       NULL, FUSE_HELP_OPTION },
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "pidfile",            required_argument, NULL, 'P' },
    { "pid-file",           required_argument, NULL, 'P' },
    { "readonly",           no_argument,       NULL, 'r' },
    { "read-only",          no_argument,       NULL, 'r' },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "version",            no_argument,       NULL, 'V' },

    { NULL }
  };
  int c, fd, r;
  size_t i;
  uint32_t cid, port;
  bool singlethread = false;
  int64_t ssize;
  const char *s;
  struct fuse_args fuse_args = FUSE_ARGS_INIT (0, NULL);
  struct fuse_loop_config fuse_loop_config;
  FILE *fp;

  for (;;) {
    c = getopt_long (argc, argv, short_options, long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
    case HELP_OPTION:
      usage (stdout, EXIT_SUCCESS);

    case FUSE_HELP_OPTION:
      fuse_help (argv[0]);
      exit (EXIT_SUCCESS);

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

    case 'o':
      fuse_opt_add_opt_escaped (&fuse_options, optarg);
      break;

    case 'P':
      pidfile = optarg;
      break;

    case 'r':
      readonly = true;
      break;

    case 's':
      singlethread = true;
      break;

    case 'V':
      display_version ("nbdfuse");
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* There must be at least 2 parameters (mountpoint and
   * URI/--command/etc).
   */
  if (argc - optind < 2)
    usage (stderr, EXIT_FAILURE);

  /* Parse and check the mountpoint.  It might be MOUNTPOINT (file),
   * MOUNTPOINT (directory), or MOUNTPOINT/FILENAME.
   */
  s = argv[optind++];
  if (is_regular_file (s)) {    /* MOUNTPOINT (file) */
    const char *p;

    file_mode = true;
    mountpoint = strdup (s);
    if (mountpoint == NULL) {
    strdup_error:
      perror ("strdup");
      exit (EXIT_FAILURE);
    }
    p = strrchr (s, '/');
    if (!p)
      filename = strdup (s);
    else {
      if (strlen (p+1) == 0) goto mp_error; /* probably can't happen */
      filename = strdup (p+1);
    }
    if (!filename) goto strdup_error;
  }
  else if (is_directory (s)) {  /* MOUNTPOINT/nbd */
    mountpoint = strdup (s);
    filename = strdup ("nbd");
    if (mountpoint == NULL || filename == NULL)
      goto strdup_error;
  }
  else {                        /* MOUNTPOINT/FILENAME */
    const char *p = strrchr (s, '/');

    if (p == NULL) {
    mp_error:
      fprintf (stderr, "%s: %s: "
               "mountpoint must be \"directory\" or \"directory/filename\"\n",
               argv[0], s);
      exit (EXIT_FAILURE);
    }
    mountpoint = strndup (s, p-s);
    if (mountpoint == NULL) goto strdup_error;
    if (! is_directory (mountpoint)) goto mp_error;
    if (strlen (p+1) == 0) goto mp_error;
    filename = strdup (p+1);
    if (filename == NULL) goto strdup_error;
  }

  /* The next parameter is either a URI or a mode switch. */
  if (strcmp (argv[optind], "--command") == 0 ||
      strcmp (argv[optind], "--cmd") == 0) {
    mode = MODE_COMMAND;
    optind++;
  }
  else if (strcmp (argv[optind], "[") == 0) {
    mode = MODE_SQUARE_BRACKET;
    optind++;
  }
  else if (strcmp (argv[optind], "--socket-activation") == 0 ||
           strcmp (argv[optind], "--systemd-socket-activation") == 0) {
    mode = MODE_SOCKET_ACTIVATION;
    optind++;
  }
  else if (strcmp (argv[optind], "--fd") == 0) {
    mode = MODE_FD;
    optind++;
  }
  else if (strcmp (argv[optind], "--tcp") == 0) {
    mode = MODE_TCP;
    optind++;
  }
  else if (strcmp (argv[optind], "--unix") == 0) {
    mode = MODE_UNIX;
    optind++;
  }
  else if (strcmp (argv[optind], "--vsock") == 0) {
    mode = MODE_VSOCK;
    optind++;
  }
  /* This is undocumented, but allow either URI or --uri URI. */
  else if (strcmp (argv[optind], "--uri") == 0) {
    mode = MODE_URI;
    optind++;
  }
  else if (argv[optind][0] == '-') {
    fprintf (stderr, "%s: unknown mode: %s\n", argv[0], argv[optind]);
    usage (stderr, EXIT_FAILURE);
  }

#ifndef HAVE_LIBXML2
  if (mode == MODE_URI) {
    fprintf (stderr, "%s: URIs are not supported in this build of libnbd\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
#endif

  /* Check there are enough parameters following given the mode. */
  switch (mode) {
  case MODE_URI:
  case MODE_FD:
  case MODE_UNIX:
    if (argc - optind != 1)
      usage (stderr, EXIT_FAILURE);
    break;
  case MODE_TCP:
  case MODE_VSOCK:
    if (argc - optind != 2)
      usage (stderr, EXIT_FAILURE);
    break;
  case MODE_COMMAND:
  case MODE_SOCKET_ACTIVATION:
    if (argc - optind < 1)
      usage (stderr, EXIT_FAILURE);
    break;
  case MODE_SQUARE_BRACKET:
    if (argc - optind < 2 || strcmp (argv[argc-1], "]") != 0)
      usage (stderr, EXIT_FAILURE);
    break;
  }
  /* At this point we know the command line is valid, and so can start
   * opening FUSE and libnbd.
   */

  /* Create the libnbd handle. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to the NBD server synchronously. */
  switch (mode) {
  case MODE_URI:
    if (nbd_connect_uri (nbd, argv[optind]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_COMMAND:
    if (nbd_connect_command (nbd, &argv[optind]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_SQUARE_BRACKET:
    /* This is the same as MODE_SOCKET_ACTIVATION but we must eat the
     * closing square bracket on the command line.
     */
    assert (strcmp (argv[argc-1], "]") == 0); /* checked above */
    argv[argc-1] = NULL;
    /*FALLTHROUGH */
  case MODE_SOCKET_ACTIVATION:
    if (nbd_connect_systemd_socket_activation (nbd, &argv[optind]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_FD:
    if (sscanf (argv[optind], "%d", &fd) != 1) {
      fprintf (stderr, "%s: could not parse file descriptor: %s\n\n",
               argv[0], argv[optind]);
      exit (EXIT_FAILURE);
    }
    if (nbd_connect_socket (nbd, fd) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_TCP:
    if (nbd_connect_tcp (nbd, argv[optind], argv[optind+1]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_UNIX:
    if (nbd_connect_unix (nbd, argv[optind]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_VSOCK:
    if (sscanf (argv[optind], "%" SCNu32, &cid) != 1) {
      fprintf (stderr, "%s: could not parse vsock cid: %s\n\n",
               argv[0], argv[optind]);
      exit (EXIT_FAILURE);
    }
    if (sscanf (argv[optind+1], "%" SCNu32, &port) != 1) {
      fprintf (stderr, "%s: could not parse vsock port: %s\n\n",
               argv[0], argv[optind]);
      exit (EXIT_FAILURE);
    }
    if (nbd_connect_vsock (nbd, cid, port) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;
  }

  ssize = nbd_get_size (nbd);
  if (ssize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  size = (uint64_t) ssize;

  /* If the remote NBD server is readonly, then act as if the '-r'
   * flag was given on the nbdfuse command line.
   */
  if (nbd_is_read_only (nbd) > 0)
    readonly = true;

  /* Create the background thread which is used to dispatch NBD
   * operations.
   */
  start_operations_thread ();

  /* This is just used to give an unchanging time when they stat in
   * the mountpoint.
   */
  clock_gettime (CLOCK_REALTIME, &start_t);

  /* Create the FUSE args. */
  if (fuse_opt_add_arg (&fuse_args, argv[0]) == -1) {
  fuse_opt_error:
    perror ("fuse_opt_add_arg");
    exit (EXIT_FAILURE);
  }

  if (fuse_options) {
    if (fuse_opt_add_arg (&fuse_args, "-o") == -1 ||
        fuse_opt_add_arg (&fuse_args, fuse_options) == -1)
      goto fuse_opt_error;
  }

  /* Create the FUSE mountpoint. */
  fuse = fuse_new (&fuse_args,
                   &nbdfuse_operations, sizeof nbdfuse_operations, NULL);
  if (!fuse) {
    perror ("fuse_new");
    exit (EXIT_FAILURE);
  }
  fuse_opt_free_args (&fuse_args);

  fuse_session = fuse_get_session (fuse);

  fuse_set_signal_handlers (fuse_session);

  /* After successful fuse_mount we must be careful not to call
   * exit(2) in this program until we have unmounted the filesystem
   * below.
   */
  if (fuse_mount (fuse, mountpoint) == -1) {
    perror ("fuse_mount");
    exit (EXIT_FAILURE);
  }

  /* Ready to serve, write pidfile. */
  if (pidfile) {
    fp = fopen (pidfile, "w");
    if (fp) {
      fprintf (fp, "%ld", (long) getpid ());
      fclose (fp);
    }
  }

  /* Enter the main loop. */
  if (!singlethread) {
    memset (&fuse_loop_config, 0, sizeof fuse_loop_config);
    fuse_loop_config.clone_fd = 0;
    fuse_loop_config.max_idle_threads = 10;
    r = fuse_loop_mt (fuse, &fuse_loop_config);
  }
  else {
    r = fuse_loop (fuse);
  }
  if (r < 0) {
    errno = -r;
    perror ("fuse_loop");
  }
  else if (r > 0) {
    fprintf (stderr, "%s: fuse_loop: fuse loop terminated by signal %d\n",
             argv[0], r);
  }

  /* Close FUSE. */
  fuse_remove_signal_handlers (fuse_session);
  fuse_unmount (fuse);
  fuse_destroy (fuse);

  /* Close NBD handle. */
  nbd_close (nbd);

  free (mountpoint);
  free (filename);
  free (fuse_options);

  exit (r == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
