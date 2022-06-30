/* NBD client library in userspace
 * Copyright (C) 2020-2022 Red Hat Inc.
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
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>
#include <getopt.h>

#include <libnbd.h>

#include "minmax.h"
#include "rounding.h"
#include "version.h"
#include "vector.h"

DEFINE_VECTOR_TYPE (uint32_vector, uint32_t)

static const char *progname;
static struct nbd_handle *nbd;
static bool colour;
static uint64_t limit = UINT64_MAX; /* --length (unlimited by default) */
static int64_t size;                /* actual size */
static bool can_meta_context;       /* did we get extent data? */

/* See do_connect () */
static enum { MODE_URI = 1, MODE_SQUARE_BRACKET } mode;
static char **args;

/* Read buffer. */
static unsigned char buffer[16*1024*1024];

static void do_connect (void);
static void do_dump (void);
static void catch_signal (int);

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Hexdump the content of a disk over NBD:\n"
"\n"
"    nbddump NBD-URI | [ CMD ARGS ... ]\n"
"\n"
"Other options:\n"
"\n"
"    nbddump --help\n"
"    nbddump --version\n"
"\n"
"Examples:\n"
"\n"
"    nbddump nbd://localhost\n"
"    nbddump -- [ qemu-nbd -r -f qcow2 file.qcow2 ]\n"
"\n"
"Please read the nbddump(1) manual page for full usage.\n"
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
    COLOUR_OPTION,
    NO_COLOUR_OPTION,
  };
  const char *short_options = "n:V";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "version",            no_argument,       NULL, 'V' },

    { "color",              no_argument,       NULL, COLOUR_OPTION },
    { "colors",             no_argument,       NULL, COLOUR_OPTION },
    { "colour",             no_argument,       NULL, COLOUR_OPTION },
    { "colours",            no_argument,       NULL, COLOUR_OPTION },
    { "no-color",           no_argument,       NULL, NO_COLOUR_OPTION },
    { "no-colors",          no_argument,       NULL, NO_COLOUR_OPTION },
    { "no-colour",          no_argument,       NULL, NO_COLOUR_OPTION },
    { "no-colours",         no_argument,       NULL, NO_COLOUR_OPTION },
    { "length",             required_argument, NULL, 'n' },
    { "limit",              required_argument, NULL, 'n' },
    { NULL }
  };
  int c;
  size_t i;

  progname = argv[0];
  colour = isatty (STDOUT_FILENO);

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

    case COLOUR_OPTION:
      colour = true;
      break;

    case NO_COLOUR_OPTION:
      colour = false;
      break;

    case 'n':
      /* XXX Allow human sizes here. */
      if (sscanf (optarg, "%" SCNu64, &limit) != 1) {
        fprintf (stderr, "%s: could not parse --length option: %s\n",
                 progname, optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'V':
      display_version ("nbddump");
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* Is it a URI or subprocess? */
  if (argc - optind >= 3 &&
      strcmp (argv[optind], "[") == 0 &&
      strcmp (argv[argc-1], "]") == 0) {
    mode = MODE_SQUARE_BRACKET;
    argv[argc-1] = NULL;
    args = &argv[optind+1];
  }
  else if (argc - optind == 1) {
    mode = MODE_URI;
    args = &argv[optind];
  }
  else {
    usage (stderr, EXIT_FAILURE);
  }

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */
  nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);

  /* Connect to the server. */
  do_connect ();
  can_meta_context =
    nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) > 0;

  /* Get the size. */
  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Before dumping, make sure we restore the terminal on ^C etc. */
  signal (SIGINT, catch_signal);
  signal (SIGQUIT, catch_signal);
  signal (SIGTERM, catch_signal);
  signal (SIGHUP, catch_signal);

  /* Dump the content. */
  do_dump ();

  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}

/* Connect the handle to the server. */
static void
do_connect (void)
{
  int r;

  switch (mode) {
  case MODE_URI:                /* NBD-URI */
    r = nbd_connect_uri (nbd, args[0]);
    break;

  case MODE_SQUARE_BRACKET:     /* [ CMD ARGS ... ] */
    r = nbd_connect_systemd_socket_activation (nbd, args);
    break;

  default:
    abort ();
  }

  if (r == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

/* Various ANSI colours, suppressed if --no-colour / not tty output. */
static void
ansi_restore (void)
{
  if (colour)
    fputs ("\033[0m", stdout);
}

static void
ansi_blue (void)
{
  if (colour)
    fputs ("\033[1;34m", stdout);
}

static void
ansi_green (void)
{
  if (colour)
    fputs ("\033[0;32m", stdout);
}

static void
ansi_magenta (void)
{
  if (colour)
    fputs ("\033[1;35m", stdout);
}

static void
ansi_red (void)
{
  if (colour)
    fputs ("\033[1;31m", stdout);
}

static void
ansi_grey (void)
{
  if (colour)
    fputs ("\033[0;90m", stdout);
}

static void
catch_signal (int sig)
{
  printf ("\n");
  ansi_restore ();
  fflush (stdout);
  _exit (EXIT_FAILURE);
}

/* Read the extent map for the next block and return true if it is all
 * zeroes.  This is conservative and returns false if we did not get
 * the full extent map from the server, or if the server doesn't
 * support base:allocation at all.
 */
static int
extent_callback (void *user_data, const char *metacontext,
                 uint64_t offset,
                 uint32_t *entries, size_t nr_entries,
                 int *error)
{
  uint32_vector *list = user_data;
  size_t i;

  if (strcmp (metacontext, LIBNBD_CONTEXT_BASE_ALLOCATION) != 0)
    return 0;

  /* Just append the entries we got to the list. */
  for (i = 0; i < nr_entries; ++i) {
    if (uint32_vector_append (list, entries[i]) == -1) {
      perror ("realloc");
      exit (EXIT_FAILURE);
    }
  }
  return 0;
}

static bool
test_all_zeroes (uint64_t offset, size_t count)
{
  uint32_vector entries = empty_vector;
  size_t i;
  uint64_t count_read;

  if (!can_meta_context)
    return false;

  /* Get the extent map for the block.  Note the server doesn't need
   * to return all requested data here.  If it does not then we return
   * false, causing the main code to do a full read.  We could be
   * smarter and keep asking the server (XXX).
   */
  if (nbd_block_status (nbd, count, offset,
                        (nbd_extent_callback) {
                          .callback = extent_callback,
                          .user_data = &entries },
                        0) == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  count_read = 0;
  for (i = 0; i < entries.len; i += 2) {
    uint32_t len = entries.ptr[i];
    uint32_t type = entries.ptr[i+1];

    count_read += len;
    if (!(type & 2))            /* not zero */
      return false;
  }

  /* Did we read at least the whole range wanted? */
  if (count_read < count)
    return false;

  /* If we got here, we read the whole range and it was all zeroes. */
  return true;
}

/* Hexdump the NBD data.
 *
 * XXX In future we could do this all asynch (including writing to
 * stdout) which could make it very efficient.
 */
static void
do_dump (void)
{
  /* If --no-colour, don't use unicode in the output. */
  const char *splat = colour ? "☆" : "*";
  const char *pipe = colour ? "│" : "|";
  const char *dot = colour ? "·" : ".";
  uint64_t offset = 0;
  uint64_t count = size > limit ? limit : size;
  size_t i, j, n;
  char last[16];
  bool printed_splat = false, same;

  while (count) {
    n = MIN (count, sizeof buffer);

    if (! test_all_zeroes (offset, n)) {
      if (nbd_pread (nbd, buffer, n, offset, 0) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else {
      memset (buffer, 0, n);
    }

    /* Make sure a multiple of 16 bytes gets written to the buffer. */
    if (n & 15)
      memset (&buffer[n], 0, 16 - (n & 15));

    for (i = 0; i < n; i += 16) {
      /* Is this line the same as the last line?  (Squashing) */
      same =
        offset + i > 0 && /* first line is never squashed */
        offset + i + 16 < size && /* last line is never squashed */
        memcmp (&buffer[i], last, 16) == 0;
      if (same) {
        if (!printed_splat) {
          printf ("%s\n", splat);
          printed_splat = true;
        }
        continue;
      }
      printed_splat = false;
      memcpy (last, &buffer[i], 16); /* Save the current line. */

      /* Print the offset. */
      ansi_green ();
      printf ("%010zx", offset + i);
      ansi_grey ();
      printf (": ");

      /* Print the hex codes. */
      for (j = i; j < MIN (i+16, n); ++j) {
        if (buffer[j])
          ansi_blue ();
        else
          ansi_grey ();
        printf ("%02x ", buffer[j]);
        if ((j - i) == 7) printf (" ");
      }
      ansi_grey ();
      for (; j < i+16; ++j) {
        printf ("   ");
        if ((j - i) == 7) printf (" ");
      }

      /* Print the ASCII codes. */
      printf ("%s", pipe);
      for (j = i; j < MIN (i+16, n); ++j) {
        char c = (char) buffer[j];
        if (isalnum (c)) {
          ansi_red ();
          printf ("%c", c);
        }
        else if (isprint (c)) {
          ansi_magenta ();
          printf ("%c", c);
        }
        else {
          ansi_grey ();
          printf ("%s", dot);
        }
      }
      ansi_grey ();
      for (; j < i+16; ++j)
        printf (" ");
      printf ("%s\n", pipe);
      ansi_restore ();
    }

    offset += n;
    count -= n;
  }
}
