/* NBD client library in userspace.
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
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <pthread.h>

#include <libnbd.h>

#include "ispowerof2.h"
#include "version.h"
#include "nbdcopy.h"

bool allocated;                 /* --allocated flag */
unsigned connections = 4;       /* --connections */
bool destination_is_zero;       /* --destination-is-zero flag */
bool extents = true;            /* ! --no-extents flag */
bool flush;                     /* --flush flag */
unsigned max_requests = 64;     /* --requests */
bool progress;                  /* -p flag */
int progress_fd = -1;           /* --progress=FD */
unsigned sparse_size = 4096;    /* --sparse */
bool synchronous;               /* --synchronous flag */
unsigned threads;               /* --threads */
struct rw *src, *dst;           /* The source and destination. */
bool verbose;                   /* --verbose flag */

const char *prog;               /* program name (== basename argv[0]) */

static bool is_nbd_uri (const char *s);
static struct rw *open_local (const char *filename, bool writing);
static void print_rw (struct rw *rw, const char *prefix, FILE *fp);

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

int
main (int argc, char *argv[])
{
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    LONG_OPTIONS,
    SHORT_OPTIONS,
    ALLOCATED_OPTION,
    DESTINATION_IS_ZERO_OPTION,
    FLUSH_OPTION,
    NO_EXTENTS_OPTION,
    SYNCHRONOUS_OPTION,
  };
  const char *short_options = "C:pR:S:T:vV";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "allocated",          no_argument,       NULL, ALLOCATED_OPTION },
    { "connections",        required_argument, NULL, 'C' },
    { "destination-is-zero",no_argument,       NULL, DESTINATION_IS_ZERO_OPTION },
    { "flush",              no_argument,       NULL, FLUSH_OPTION },
    { "no-extents",         no_argument,       NULL, NO_EXTENTS_OPTION },
    { "progress",           optional_argument, NULL, 'p' },
    { "requests",           required_argument, NULL, 'R' },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "sparse",             required_argument, NULL, 'S' },
    { "synchronous",        no_argument,       NULL, SYNCHRONOUS_OPTION },
    { "target-is-zero",     no_argument,       NULL, DESTINATION_IS_ZERO_OPTION },
    { "threads",            required_argument, NULL, 'T' },
    { "verbose",            no_argument,       NULL, 'v' },
    { "version",            no_argument,       NULL, 'V' },
    { NULL }
  };
  int c;
  size_t i;

  /* Set prog to basename argv[0]. */
  prog = strrchr (argv[0], '/');
  if (prog == NULL) prog = argv[0]; else prog++;

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

    case ALLOCATED_OPTION:
      allocated = true;
      break;

    case DESTINATION_IS_ZERO_OPTION:
      destination_is_zero = true;
      break;

    case FLUSH_OPTION:
      flush = true;
      break;

    case NO_EXTENTS_OPTION:
      extents = false;
      break;

    case SYNCHRONOUS_OPTION:
      synchronous = true;
      break;

    case 'C':
      if (sscanf (optarg, "%u", &connections) != 1 || connections == 0) {
        fprintf (stderr, "%s: --connections: could not parse: %s\n",
                 prog, optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'p':
      progress = true;
      if (optarg) {
        if (sscanf (optarg, "%d", &progress_fd) != 1 || progress_fd < 0) {
          fprintf (stderr, "%s: --progress: could not parse: %s\n",
                   prog, optarg);
          exit (EXIT_FAILURE);
        }
      }
      break;

    case 'R':
      if (sscanf (optarg, "%u", &max_requests) != 1 || max_requests == 0) {
        fprintf (stderr, "%s: --requests: could not parse: %s\n",
                 prog, optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'S':
      if (sscanf (optarg, "%u", &sparse_size) != 1) {
        fprintf (stderr, "%s: --sparse: could not parse: %s\n",
                 prog, optarg);
        exit (EXIT_FAILURE);
      }
      if (sparse_size != 0 &&
          (sparse_size < 512 || !is_power_of_2 (sparse_size))) {
        fprintf (stderr, "%s: --sparse: must be a power of 2 and >= 512\n",
                 prog);
        exit (EXIT_FAILURE);
      }
      break;

    case 'T':
      if (sscanf (optarg, "%u", &threads) != 1) {
        fprintf (stderr, "%s: --threads: could not parse: %s\n",
                 prog, optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'v':
      verbose = true;
      break;

    case 'V':
      display_version ("nbdcopy");
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* The remaining parameters describe the SOURCE and DESTINATION and
   * may either be -, filenames, NBD URIs, null: or [ ... ] sequences.
   */
  if (optind > argc - 2)
    usage (stderr, EXIT_FAILURE);

  if (strcmp (argv[optind], "[") == 0) { /* Source is [...] */
    for (i = optind+1; i < argc; ++i)
      if (strcmp (argv[i], "]") == 0)
        goto found1;
    usage (stderr, EXIT_FAILURE);

  found1:
    connections = 1;            /* multi-conn not supported */
    src =
      nbd_rw_create_subprocess ((const char **) &argv[optind+1], i-optind-1,
                                false);
    optind = i+1;
  }
  else {                        /* Source is not [...]. */
    const char *src_name = argv[optind++];

    if (! is_nbd_uri (src_name))
      src = open_local (src_name, false);
    else
      src = nbd_rw_create_uri (src_name, src_name, false);
  }

  if (optind >= argc)
    usage (stderr, EXIT_FAILURE);

  if (strcmp (argv[optind], "[") == 0) { /* Destination is [...] */
    for (i = optind+1; i < argc; ++i)
      if (strcmp (argv[i], "]") == 0)
        goto found2;
    usage (stderr, EXIT_FAILURE);

  found2:
    connections = 1;            /* multi-conn not supported */
    dst =
      nbd_rw_create_subprocess ((const char **) &argv[optind+1], i-optind-1,
                                true);
    optind = i+1;
  }
  else {                        /* Destination is not [...] */
    const char *dst_name = argv[optind++];

    if (strcmp (dst_name, "null:") == 0)
      dst = null_create (dst_name);
    else if (! is_nbd_uri (dst_name))
      dst = open_local (dst_name, true /* writing */);
    else
      dst = nbd_rw_create_uri (dst_name, dst_name, true);
  }

  /* There must be no extra parameters. */
  if (optind != argc)
    usage (stderr, EXIT_FAILURE);

  /* Check we've created src and dst and set the expected fields. */
  assert (src != NULL);
  assert (dst != NULL);
  assert (src->ops != NULL);
  assert (src->name != NULL);
  assert (dst->ops != NULL);
  assert (dst->name != NULL);

  /* Obviously this is not going to work if the destination is
   * read-only, so fail early with a nice error message.
   */
  if (dst->ops->is_read_only (dst)) {
    fprintf (stderr, "%s: %s: "
             "the destination is read-only, cannot write to it\n",
             prog, dst->name);
    exit (EXIT_FAILURE);
  }

  /* If multi-conn is not supported, force connections to 1. */
  if (! src->ops->can_multi_conn (src) || ! dst->ops->can_multi_conn (dst))
    connections = 1;

  /* Calculate the number of threads from the number of connections. */
  if (threads == 0) {
    long t;

#ifdef _SC_NPROCESSORS_ONLN
    t = sysconf (_SC_NPROCESSORS_ONLN);
    if (t <= 0) {
      perror ("could not get number of cores online");
      t = 1;
    }
#else
    t = 1;
#endif
    threads = (unsigned) t;
  }

  if (synchronous)
    connections = 1;

  if (connections < threads)
    threads = connections;
  if (threads < connections)
    connections = threads;

  /* Truncate the destination to the same size as the source.  Only
   * has an effect on regular files.
   */
  if (dst->ops->truncate)
    dst->ops->truncate (dst, src->size);

  /* Check if the source is bigger than the destination, since that
   * would truncate (ie. lose) data.  Copying from smaller to larger
   * is OK.
   */
  if (src->size >= 0 && dst->size >= 0 && src->size > dst->size) {
    fprintf (stderr,
             "%s: error: destination size is smaller than source size\n",
             prog);
    exit (EXIT_FAILURE);
  }

  /* Since we have constructed the final src and dst structures here,
   * print them out in verbose mode, and also various useful internal
   * settings.
   */
  if (verbose) {
    print_rw (src, "nbdcopy: src", stderr);
    print_rw (dst, "nbdcopy: dst", stderr);
    fprintf (stderr, "nbdcopy: connections=%u requests=%u threads=%u "
             "synchronous=%s\n",
             connections, max_requests, threads,
             synchronous ? "true" : "false");
  }

  /* If multi-conn is enabled on either side, then at this point we
   * need to ask the backend to open the extra connections.
   */
  if (connections > 1) {
    assert (threads == connections);
    if (src->ops->can_multi_conn (src))
      src->ops->start_multi_conn (src);
    if (dst->ops->can_multi_conn (dst))
      dst->ops->start_multi_conn (dst);
  }

  /* If the source is NBD and we couldn't negotiate meta
   * base:allocation then turn off extents.
   */
  if (! src->ops->can_extents (src))
    extents = false;

  /* Always set the progress bar to 0% at the start of the copy. */
  progress_bar (0, 1);

  /* Start copying. */
  if (synchronous)
    synch_copying ();
  else
    multi_thread_copying ();

  /* Always set the progress bar to 100% at the end of the copy. */
  progress_bar (1, 1);

  /* Shut down the source side. */
  src->ops->close (src);

  /* Shut down the destination side. */
  if (flush)
    dst->ops->flush (dst);
  dst->ops->close (dst);

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
 * Returns the struct rw * which the caller must close.
 *
 * “writing” is true if this is the destination parameter.
 * “rw->u.local.stat” and “rw->size” return the file stat and size,
 * but size can be returned as -1 if we don't know the size (if it's a
 * pipe or stdio).
 */
static struct rw *
open_local (const char *filename, bool writing)
{
  int flags, fd;
  struct stat stat;

  if (strcmp (filename, "-") == 0) {
    synchronous = true;
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
        fprintf (stderr, "%s: %s: %m\n", prog, filename);
        exit (EXIT_FAILURE);
      }
    }
  }

  if (fstat (fd, &stat) == -1) {
    fprintf (stderr, "%s: %s: %m\n", prog, filename);
    exit (EXIT_FAILURE);
  }
  if (S_ISBLK (stat.st_mode) || S_ISREG (stat.st_mode))
    return file_create (filename, fd, stat.st_size, S_ISBLK (stat.st_mode));
  else {
    /* Probably stdin/stdout, a pipe or a socket. */
    synchronous = true;        /* Force synchronous mode for pipes. */
    return pipe_create (filename, fd);
  }
}

/* Print an rw struct, used in --verbose mode. */
static void
print_rw (struct rw *rw, const char *prefix, FILE *fp)
{
  fprintf (fp, "%s: %s \"%s\"\n", prefix, rw->ops->ops_name, rw->name);
  fprintf (fp, "%s: size=%" PRIi64 "\n", prefix, rw->size);
}

/* Default implementation of rw->ops->get_extents for backends which
 * don't/can't support extents.  Also used for the --no-extents case.
 */
void
default_get_extents (struct rw *rw, uintptr_t index,
                     uint64_t offset, uint64_t count,
                     extent_list *ret)
{
  struct extent e;

  ret->size = 0;

  e.offset = offset;
  e.length = count;
  e.zero = false;
  if (extent_list_append (ret, e) == -1) {
    perror ("realloc");
    exit (EXIT_FAILURE);
  }
}

/* Implementations of get_polling_fd and asynch_notify_* for backends
 * which don't support polling.
 */
void
get_polling_fd_not_supported (struct rw *rw, uintptr_t index,
                              int *fd_rtn, int *direction_rtn)
{
  /* Not an error, this causes poll to ignore the fd. */
  *fd_rtn = -1;
  *direction_rtn = LIBNBD_AIO_DIRECTION_READ;
}

void
asynch_notify_read_write_not_supported (struct rw *rw, uintptr_t index)
{
  /* nothing */
}
