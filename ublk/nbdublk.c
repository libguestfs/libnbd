/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
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

/* ublk support. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>

#include <ublksrv.h>

#include <libnbd.h>

#include "nbdublk.h"

#include "ispowerof2.h"
#include "vector.h"
#include "version.h"

#define DEVICE_PREFIX "/dev/ublkb"
#define DEVICE_PREFIX_LEN 10

handles nbd = empty_vector;
unsigned connections = 4;
bool readonly = false;
bool rotational;
bool can_fua;
uint64_t size;
uint64_t min_block_size;
uint64_t pref_block_size;
bool verbose = false;

/* The single control device.  This is a global so the signal handler
 * can attempt to stop the device.
 */
static struct ublksrv_ctrl_dev *dev;

enum mode {
  MODE_URI,                  /* URI */
  MODE_COMMAND,              /* --command */
  MODE_FD,                   /* --fd */
  MODE_SQUARE_BRACKET,       /* [ CMD ], same as --socket-activation*/
  MODE_SOCKET_ACTIVATION,    /* --socket-activation */
  MODE_TCP,                  /* --tcp */
  MODE_UNIX,                 /* --unix */
  MODE_VSOCK,                /* --vsock */
};

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Mount NBD server as a virtual device:\n"
"\n"
#ifdef HAVE_LIBXML2
"    nbdublk [-C N|--connections N] [-r] [-v|--verbose]\n"
"            " DEVICE_PREFIX "<N> URI\n"
"\n"
"Other modes:\n"
"\n"
#endif
"    nbdublk " DEVICE_PREFIX "<N> [ CMD [ARGS ...] ]\n"
"    nbdublk " DEVICE_PREFIX "<N> --command CMD [ARGS ...]\n"
"    nbdublk " DEVICE_PREFIX "<N> --fd N\n"
"    nbdublk " DEVICE_PREFIX "<N> --tcp HOST PORT\n"
"    nbdublk " DEVICE_PREFIX "<N> --unix SOCKET\n"
"    nbdublk " DEVICE_PREFIX "<N> --vsock CID PORT\n"
"\n"
"You can also use just the device number or '-' to allocate one:\n"
"\n"
"    nbdublk <N> ...\n"
"    nbdublk - ...\n"
"\n"
"To unmount:\n"
"\n"
"    ublk del -n <N>\n"
"\n"
"Other options:\n"
"\n"
"    nbdublk --help\n"
"    nbdublk -V|--version\n"
"\n"
"Please read the nbdublk(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

/* Which modes support multi-conn?  We cannot connect multiple times
 * to subprocesses (since we'd have to launch multiple subprocesses).
 */
static bool
mode_is_multi_conn_compatible (enum mode mode)
{
  switch (mode) {
  case MODE_COMMAND:
  case MODE_SQUARE_BRACKET:
  case MODE_SOCKET_ACTIVATION:
  case MODE_FD:
    return false;
  case MODE_URI:
  case MODE_TCP:
  case MODE_UNIX:
  case MODE_VSOCK:
    return true;
  default:
    abort ();
  }
}

static struct nbd_handle *create_and_connect (enum mode mode,
                                              int argc, char **argv);
static void signal_handler (int sig);

int
main (int argc, char *argv[])
{
  enum mode mode = MODE_URI;
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    LONG_OPTIONS,
    SHORT_OPTIONS,
  };
  /* Note the "+" means we stop processing as soon as we get to the
   * first non-option argument (the device) and then we parse the rest
   * of the command line without getopt.
   */
  const char *short_options = "+C:rvV";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "connections",        required_argument, NULL, 'C' },
    { "readonly",           no_argument,       NULL, 'r' },
    { "read-only",          no_argument,       NULL, 'r' },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "verbose",            no_argument,       NULL, 'v' },
    { "version",            no_argument,       NULL, 'V' },

    { NULL }
  };
  int c, r;
  size_t i;
  struct nbd_handle *h;
  int64_t rs;
  uint64_t max_block_size;
  const char *s;
  struct ublksrv_dev_data data = { .dev_id = -1 };
  const struct ublksrv_ctrl_dev_info *dinfo;
  struct sigaction sa = { 0 };

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

    case 'C':
      if (sscanf (optarg, "%u", &connections) != 1 ||
          connections < 1 || connections > 1024) {
        fprintf (stderr, "%s: --connections parameter must be an unsigned integer >= 1\n",
                 argv[0]);
        exit (EXIT_FAILURE);
      }
      break;

    case 'r':
      readonly = true;
      break;

    case 'v':
      verbose = true;
      break;

    case 'V':
      display_version ("nbdublk");
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* There must be at least 2 parameters (device and
   * URI/--command/etc).
   */
  if (argc - optind < 2)
    usage (stderr, EXIT_FAILURE);

  /* Parse and check the device name. */
  s = argv[optind++];
  /* /dev/ublkc<N> */
  if (strncmp (s, DEVICE_PREFIX, DEVICE_PREFIX_LEN) == 0) {
    if (sscanf (&s[DEVICE_PREFIX_LEN], "%u", &data.dev_id) != 1) {
      fprintf (stderr, "%s: could not parse ublk device name: %s\n",
               argv[0], s);
      exit (EXIT_FAILURE);
    }
  }
  else if (s[0] >= '0' && s[0] <= '9') {
    if (sscanf (s, "%u", &data.dev_id) != 1) {
      fprintf (stderr, "%s: could not parse ublk device name: %s\n",
               argv[0], s);
      exit (EXIT_FAILURE);
    }
  }
  else if (s[0] == '-') {
    data.dev_id = -1;           /* autoallocate */
  }
  else {
    fprintf (stderr, "%s: expecting device name %s<N>\n",
             argv[0], DEVICE_PREFIX);
    exit (EXIT_FAILURE);
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
  /* At this point we know the command line is valid. */

  /* Create the libnbd handle and connect to it. */
  h = create_and_connect (mode, argc, argv);
  if (handles_append (&nbd, h) == -1) {
    perror ("realloc");
    exit (EXIT_FAILURE);
  }

  /* If the server supports multi-conn, and we are able to, try to
   * open more handles.
   */
  if (connections > 1 &&
      mode_is_multi_conn_compatible (mode) &&
      nbd_can_multi_conn (nbd.ptr[0]) >= 1) {
    if (handles_reserve (&nbd, connections-1) == -1) {
      perror ("realloc");
      exit (EXIT_FAILURE);
    }
    for (i = 2; i <= connections; ++i) {
      h = create_and_connect (mode, argc, argv);
      handles_append (&nbd, h); /* reserved above, so can't fail */
    }
  }
  connections = (unsigned) nbd.len;

  /* Get the size and preferred block sizes. */
  rs = nbd_get_size (nbd.ptr[0]);
  if (rs == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  size = (uint64_t) rs;

  rs = nbd_get_block_size (nbd.ptr[0], LIBNBD_SIZE_MAXIMUM);
  if (rs <= 0 || rs > 64 * 1024 * 1024)
    max_block_size = 64 * 1024 * 1024;
  else
    max_block_size = rs;
  if (!is_power_of_2 (max_block_size)) {
    fprintf (stderr,
             "%s: %s block size is not a power of two: %" PRIu64 "\n",
             argv[0], "maximum", max_block_size);
    exit (EXIT_FAILURE);
  }

  rs = nbd_get_block_size (nbd.ptr[0], LIBNBD_SIZE_PREFERRED);
  if (rs <= 0)
    pref_block_size = 4096;
  else
    pref_block_size = rs;
  if (!is_power_of_2 (pref_block_size)) {
    fprintf (stderr,
             "%s: %s block size is not a power of two: %" PRIu64 "\n",
             argv[0], "preferred", pref_block_size);
    exit (EXIT_FAILURE);
  }

  rs = nbd_get_block_size (nbd.ptr[0], LIBNBD_SIZE_MINIMUM);
  if (rs <= 0)
    min_block_size = 512; /* minimum that the kernel supports */
  else
    min_block_size = rs;
  if (!is_power_of_2 (min_block_size)) {
    fprintf (stderr,
             "%s: %s block size is not a power of two: %" PRIu64 "\n",
             argv[0], "minimum", min_block_size);
    exit (EXIT_FAILURE);
  }

  /* If the remote NBD server is readonly, then act as if the '-r'
   * flag was given on the nbdublk command line.
   */
  if (nbd_is_read_only (nbd.ptr[0]) > 0)
    readonly = true;

  rotational = nbd_is_rotational (nbd.ptr[0]) > 0;
  can_fua = nbd_can_fua (nbd.ptr[0]) > 0;

  if (verbose)
    fprintf (stderr, "%s: size: %" PRIu64 " connections: %u%s\n",
             argv[0], size, connections, readonly ? " readonly" : "");

  /* Fill in other fields in 'data' struct. */
  data.max_io_buf_bytes = max_block_size;
  data.nr_hw_queues = connections;
  data.queue_depth = 64;
  data.tgt_type = "nbd";
  data.tgt_ops = &tgt_type;
  data.flags = 0;
  data.ublksrv_flags = UBLKSRV_F_NEED_EVENTFD;

  dev = ublksrv_ctrl_init (&data);
  if (!dev) {
    fprintf (stderr, "%s: ublksrv_ctrl_init: %m\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  dinfo = ublksrv_ctrl_get_dev_info(dev);

  /* Register signal handlers to try to stop the device. */
  sa.sa_handler = signal_handler;
  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  sa.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &sa, NULL);

  r = ublksrv_ctrl_add_dev (dev);
  if (r < 0) {
    errno = -r;
    fprintf (stderr, "%s: ublksrv_ctrl_add_dev: "DEVICE_PREFIX "%d: %m\n",
             argv[0], dinfo->dev_id);
    ublksrv_ctrl_deinit (dev);
    exit (EXIT_FAILURE);
  }

  if (verbose)
    fprintf (stderr, "%s: created %s%d\n",
             argv[0], DEVICE_PREFIX, dinfo->dev_id);

  /* XXX nbdfuse creates a pid file.  However I reason that you can
   * tell if the service is available when the block device is created
   * so a pid file is not necessary.  May need to revisit this.
   */

  if (start_daemon (dev) == -1) {
    ublksrv_ctrl_del_dev (dev);
    ublksrv_ctrl_deinit (dev);
    for (i = 0; i < nbd.len; ++i)
      nbd_close (nbd.ptr[i]);
    exit (EXIT_FAILURE);
  }

  /* Close ublk device. */
  ublksrv_ctrl_del_dev (dev);
  ublksrv_ctrl_deinit (dev);

  /* Close NBD handle(s). */
  for (i = 0; i < nbd.len; ++i)
    nbd_close (nbd.ptr[i]);

  exit (EXIT_SUCCESS);
}

/* Called from main() above to create an NBD handle and connect to it.
 * For multi-conn, this may be called several times.
 */
static struct nbd_handle *
create_and_connect (enum mode mode, int argc, char **argv)
{
  int fd;
  uint32_t cid, port;
  struct nbd_handle *h;

  /* Create the libnbd handle. */
  h = nbd_create ();
  if (h == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_debug (h, verbose);

  /* Connect to the NBD server synchronously. */
  switch (mode) {
  case MODE_URI:
    if (nbd_connect_uri (h, argv[optind]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_COMMAND:
    if (nbd_connect_command (h, &argv[optind]) == -1) {
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
    /*FALLTHROUGH*/
  case MODE_SOCKET_ACTIVATION:
    if (nbd_connect_systemd_socket_activation (h, &argv[optind]) == -1) {
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
    if (nbd_connect_socket (h, fd) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_TCP:
    if (nbd_connect_tcp (h, argv[optind], argv[optind+1]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case MODE_UNIX:
    if (nbd_connect_unix (h, argv[optind]) == -1) {
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
    if (nbd_connect_vsock (h, cid, port) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;
  }

  return h;
}

static void
signal_handler (int sig)
{
  /* XXX Racy, but not much else we can do. */
  ublksrv_ctrl_stop_dev (dev);
}
