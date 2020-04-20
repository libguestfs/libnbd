/* NBD client library in userspace
 * Copyright (C) 2013-2020 Red Hat Inc.
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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define FUSE_USE_VERSION 26

#include <fuse.h>
#ifdef HAVE_FUSE_LOWLEVEL_H
#include <fuse_lowlevel.h>
#endif

#include <libnbd.h>

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

static struct nbd_handle *nbd;
static bool readonly;
static char *mountpoint, *filename;
static const char *pidfile;
static char *fuse_options;
static struct fuse_chan *ch;
static struct fuse *fuse;
static struct timespec start_t;
static uint64_t size;

static int nbdfuse_getattr (const char *path, struct stat *stbuf);
static int nbdfuse_readdir (const char *path, void *buf,
                            fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi);
static int nbdfuse_open (const char *path, struct fuse_file_info *fi);
static int nbdfuse_read (const char *path, char *buf,
                         size_t count, off_t offset,
                         struct fuse_file_info *fi);
static int nbdfuse_write (const char *path, const char *buf,
                          size_t count, off_t offset,
                          struct fuse_file_info *fi);
static int nbdfuse_fsync (const char *path, int datasync,
                          struct fuse_file_info *fi);
static int nbdfuse_release (const char *path, struct fuse_file_info *fi);

static struct fuse_operations fuse_operations = {
  .getattr           = nbdfuse_getattr,
  .readdir           = nbdfuse_readdir,
  .open              = nbdfuse_open,
  .read              = nbdfuse_read,
  .write             = nbdfuse_write,
  .fsync             = nbdfuse_fsync,
  .release           = nbdfuse_release,
};

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Mount NBD server as a virtual file:\n"
"\n"
#ifdef HAVE_LIBXML2
"    nbdfuse [-o FUSE-OPTION] [-P PIDFILE] [-r] MOUNTPOINT[/FILENAME] URI\n"
"\n"
"Other modes:\n"
"\n"
#endif
"    nbdfuse MOUNTPOINT[/FILENAME] --command CMD [ARGS ...]\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --socket-activation CMD [ARGS ...]\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --fd N\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --tcp HOST PORT\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --unix SOCKET\n"
"    nbdfuse MOUNTPOINT[/FILENAME] --vsock CID PORT\n"
"\n"
"Please read the nbdfuse(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

static void
display_version (void)
{
  printf ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
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

int
main (int argc, char *argv[])
{
  enum {
    MODE_URI,
    MODE_COMMAND,
    MODE_FD,
    MODE_SOCKET_ACTIVATION,
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
  const char *short_options = "+o:P:rV";
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
  int64_t ssize;
  const char *s;
  struct fuse_args fuse_args = FUSE_ARGS_INIT (0, NULL);
  struct sigaction sa;
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

    case 'V':
      display_version ();
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

  /* Parse and check the mountpoint.  It might be MOUNTPOINT or
   * MOUNTPOINT/FILENAME.  In either case MOUNTPOINT must be an
   * existing directory.
   */
  s = argv[optind++];
  if (is_directory (s)) {
    mountpoint = strdup (s);
    filename = strdup ("nbd");
    if (mountpoint == NULL || filename == NULL) {
    strdup_error:
      perror ("strdup");
      exit (EXIT_FAILURE);
    }
  }
  else {
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
  ch = fuse_mount (mountpoint, &fuse_args);
  if (ch == NULL) {
    fprintf (stderr,
             "%s: fuse_mount failed: see error messages above", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Set FD_CLOEXEC on the channel.  Some versions of libfuse don't do
   * this.
   */
  fd = fuse_chan_fd (ch);
  if (fd >= 0) {
    int flags = fcntl (fd, F_GETFD, 0);
    if (flags >= 0)
      fcntl (fd, F_SETFD, flags | FD_CLOEXEC);
  }

  /* Create the FUSE handle. */
  fuse = fuse_new (ch, &fuse_args,
                   &fuse_operations, sizeof fuse_operations, NULL);
  if (!fuse) {
    perror ("fuse_new");
    exit (EXIT_FAILURE);
  }
  fuse_opt_free_args (&fuse_args);

  /* Catch signals since they can leave the mountpoint in a funny
   * state.  To exit the program callers must use ‘fusermount -u’.  We
   * also must be careful not to call exit(2) in this program until we
   * have unmounted the filesystem below.
   */
  memset (&sa, 0, sizeof sa);
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = SA_RESTART;
  sigaction (SIGPIPE, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGQUIT, &sa, NULL);

  /* Ready to serve, write pidfile. */
  if (pidfile) {
    fp = fopen (pidfile, "w");
    if (fp) {
      fprintf (fp, "%ld", (long) getpid ());
      fclose (fp);
    }
  }

  /* Enter the main loop. */
  r = fuse_loop (fuse);
  if (r != 0)
    perror ("fuse_loop");

  /* Close FUSE. */
  fuse_unmount (mountpoint, ch);
  fuse_destroy (fuse);

  /* Close NBD handle. */
  nbd_close (nbd);

  free (mountpoint);
  free (filename);
  free (fuse_options);

  exit (r == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* Wraps calls to libnbd functions and automatically checks for error,
 * returning errors in the format required by FUSE.  It also prints
 * out the full error message on stderr, so that we don't lose it.
 */
#define CHECK_NBD_ERROR(CALL)                                   \
  do { if ((CALL) == -1) return check_nbd_error (); } while (0)
static int
check_nbd_error (void)
{
  int err;

  fprintf (stderr, "%s\n", nbd_get_error ());
  err = nbd_get_errno ();
  if (err != 0)
    return -err;
  else
    return -EIO;
}

static int
nbdfuse_getattr (const char *path, struct stat *statbuf)
{
  const int mode = readonly ? 0444 : 0666;

  memset (statbuf, 0, sizeof (struct stat));

  /* We're probably making some Linux-specific assumptions here, but
   * this file is not usually compiled on non-Linux systems (perhaps
   * on OpenBSD?).  XXX
   */
  statbuf->st_atim = start_t;
  statbuf->st_mtim = start_t;
  statbuf->st_ctim = start_t;
  statbuf->st_uid = geteuid ();
  statbuf->st_gid = getegid ();

  if (strcmp (path, "/") == 0) {
    /* getattr "/" */
    statbuf->st_mode = S_IFDIR | (mode & 0111);
    statbuf->st_nlink = 2;
  }
  else if (path[0] == '/' && strcmp (path+1, filename) == 0) {
    /* getattr "/filename" */
    statbuf->st_mode = S_IFREG | mode;
    statbuf->st_nlink = 1;
    statbuf->st_size = size;
  }
  else
    return -ENOENT;

  return 0;
}

static int
nbdfuse_readdir (const char *path, void *buf,
                 fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi)
{
  if (strcmp (path, "/") != 0)
    return -ENOENT;

  filler (buf, ".", NULL, 0);
  filler (buf, "..", NULL, 0);
  filler (buf, filename, NULL, 0);

  return 0;
}

/* This function checks the O_RDONLY/O_RDWR flags passed to the
 * open(2) call, so we have to check the open mode is compatible with
 * the readonly flag.
 */
static int
nbdfuse_open (const char *path, struct fuse_file_info *fi)
{
  if (path[0] != '/' || strcmp (path+1, filename) != 0)
    return -ENOENT;

  if (readonly && (fi->flags & O_ACCMODE) != O_RDONLY)
    return -EACCES;

  return 0;
}

static int
nbdfuse_read (const char *path, char *buf,
              size_t count, off_t offset,
              struct fuse_file_info *fi)
{
  if (path[0] != '/' || strcmp (path+1, filename) != 0)
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ERROR (nbd_pread (nbd, buf, count, offset, 0));

  return (int) count;
}

static int
nbdfuse_write (const char *path, const char *buf,
               size_t count, off_t offset,
               struct fuse_file_info *fi)
{
  /* Probably shouldn't happen because of nbdfuse_open check. */
  if (readonly)
    return -EACCES;

  if (path[0] != '/' || strcmp (path+1, filename) != 0)
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ERROR (nbd_pwrite (nbd, buf, count, offset, 0));

  return (int) count;
}

static int
nbdfuse_fsync (const char *path, int datasync, struct fuse_file_info *fi)
{
  if (readonly)
    return 0;

  /* If the server doesn't support flush then the operation is
   * silently ignored.
   */
  if (nbd_can_flush (nbd))
    CHECK_NBD_ERROR (nbd_flush (nbd, 0));

  return 0;
}

/* This is called on the last close of a file.  We do a flush here to
 * be on the safe side, but it's not strictly necessary.
 */
static int
nbdfuse_release (const char *path, struct fuse_file_info *fi)
{
  if (readonly)
    return 0;

  return nbdfuse_fsync (path, 0, fi);
}
