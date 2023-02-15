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
#include <getopt.h>
#include <limits.h>

#include <libnbd.h>

#include "ansi-colours.h"
#include "version.h"

#include "nbdinfo.h"

const char *progname;
struct nbd_handle *nbd;
FILE *fp;                       /* output file descriptor */
bool colour;                    /* --colour / --no-colour option */
bool list_all = false;          /* --list option */
bool probe_content = false;     /* --content / --no-content option */
bool json_output = false;       /* --json option */
const char *can = NULL;         /* --is/--can option */
const char *map = NULL;         /* --map option */
bool size_only = false;         /* --size option */
bool totals = false;            /* --totals option */

/* See do_connect () */
static enum { MODE_URI = 1, MODE_SQUARE_BRACKET } mode;
static char **args;

static void __attribute__ ((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Display information and metadata about NBD servers and exports:\n"
"\n"
"    nbdinfo [--json] NBD-URI | [ CMD ARGS ... ]\n"
"    nbdinfo --size [--json] NBD-URI | [ CMD ARGS ... ]\n"
"    nbdinfo --is read-only|rotational NBD-URI | [ CMD ARGS ... ]\n"
"    nbdinfo --can cache|connect|... NBD-URI | [ CMD ARGS ... ]\n"
"    nbdinfo --map [--totals] [--json] NBD-URI | [ CMD ARGS ... ]\n"
"    nbdinfo -L|--list [--json] NBD-URI | [ CMD ARGS ... ]\n"
"\n"
"Other options:\n"
"\n"
"    nbdinfo --help\n"
"    nbdinfo --version\n"
"\n"
"Examples:\n"
"\n"
"    nbdinfo nbd://localhost\n"
"    nbdinfo \"nbd+unix:///?socket=/tmp/unixsock\"\n"
"    nbdinfo --size nbd://example.com\n"
"    nbdinfo --can connect nbd://example.com\n"
"    nbdinfo --is read-only nbd://example.com\n"
"    nbdinfo --map nbd://example.com\n"
"    nbdinfo --json nbd://example.com\n"
"    nbdinfo --list nbd://example.com\n"
"    nbdinfo --map -- [ qemu-nbd -r -f qcow2 file.qcow2 ]\n"
"\n"
"Please read the nbdinfo(1) manual page for full usage.\n"
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
    CONTENT_OPTION,
    NO_CONTENT_OPTION,
    JSON_OPTION,
    CAN_OPTION,
    MAP_OPTION,
    SIZE_OPTION,
    TOTALS_OPTION,
  };
  const char *short_options = "LV";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "can",                required_argument, NULL, CAN_OPTION },
    { "color",              no_argument,       NULL, COLOUR_OPTION },
    { "colors",             no_argument,       NULL, COLOUR_OPTION },
    { "colour",             no_argument,       NULL, COLOUR_OPTION },
    { "colours",            no_argument,       NULL, COLOUR_OPTION },
    { "no-color",           no_argument,       NULL, NO_COLOUR_OPTION },
    { "no-colors",          no_argument,       NULL, NO_COLOUR_OPTION },
    { "no-colour",          no_argument,       NULL, NO_COLOUR_OPTION },
    { "no-colours",         no_argument,       NULL, NO_COLOUR_OPTION },
    { "content",            no_argument,       NULL, CONTENT_OPTION },
    { "no-content",         no_argument,       NULL, NO_CONTENT_OPTION },
    { "is",                 required_argument, NULL, CAN_OPTION },
    { "json",               no_argument,       NULL, JSON_OPTION },
    { "list",               no_argument,       NULL, 'L' },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "map",                optional_argument, NULL, MAP_OPTION },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "size",               no_argument,       NULL, SIZE_OPTION },
    { "total",              no_argument,       NULL, TOTALS_OPTION },
    { "totals",             no_argument,       NULL, TOTALS_OPTION },
    { "version",            no_argument,       NULL, 'V' },
    { NULL }
  };
  size_t i;
  char *output = NULL;
  size_t output_len = 0;
  bool content_flag = false, no_content_flag = false;
  bool list_okay = true;

  progname = argv[0];
  colour = isatty (STDOUT_FILENO);

  for (;;) {
    int c = getopt_long (argc, argv, short_options, long_options, NULL);
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

    case JSON_OPTION:
      json_output = true;
      break;

    case COLOUR_OPTION:
      colour = true;
      break;

    case NO_COLOUR_OPTION:
      colour = false;
      break;

    case CONTENT_OPTION:
      content_flag = true;
      break;

    case NO_CONTENT_OPTION:
      no_content_flag = true;
      break;

    case CAN_OPTION:
      can = optarg;
      break;

    case MAP_OPTION:
      map = optarg ? optarg : "base:allocation";
      break;

    case SIZE_OPTION:
      size_only = true;
      break;

    case TOTALS_OPTION:
      totals = true;
      break;

    case 'L':
      list_all = true;
      break;

    case 'V':
      display_version ("nbdinfo");
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

  /* You cannot combine certain options. */
  if (!!list_all + !!can + !!map + !!size_only > 1) {
    fprintf (stderr,
             "%s: you cannot use --list, --map and --size together.\n",
             progname);
    exit (EXIT_FAILURE);
  }
  if (content_flag && no_content_flag) {
    fprintf (stderr, "%s: you cannot use %s and %s together.\n",
             progname, "--content", "--no-content");
    exit (EXIT_FAILURE);
  }
  if (totals && !map) {
    fprintf (stderr, "%s: you must use --totals only with --map option.\n",
             progname);
    exit (EXIT_FAILURE);
  }

  /* Work out if we should probe content. */
  probe_content = !list_all;
  if (content_flag)
    probe_content = true;
  if (no_content_flag)
    probe_content = false;
  if (can)
    probe_content = false;
  if (map)
    probe_content = false;

  /* Try to write output atomically.  We spool output into a
   * memstream, pointed to by fp, and write it all at once at the end.
   * On error nothing should be printed on stdout.
   */
  fp = open_memstream (&output, &output_len);
  if (fp == NULL) {
    fprintf (stderr, "%s: ", progname);
    perror ("open_memstream");
    exit (EXIT_FAILURE);
  }

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

  /* Set optional modes in the handle. */
  if (!can && !map && !size_only) {
    nbd_set_opt_mode (nbd, true);
    nbd_set_full_info (nbd, true);
  }
  if (map)
    nbd_add_meta_context (nbd, map);

  /* Connect to the server. */
  do_connect (nbd);

  /* In --list mode, during negotiation we collect the list of exports. */
  if (list_all)                 /* --list */
    collect_exports ();

  if (size_only)                /* --size (!list_all) */
    do_size ();
  else if (can)                 /* --is/--can (!list_all) */
    do_can ();
  else if (map)                 /* --map (!list_all) */
    do_map ();
  else {                        /* not --size or --map */
    const char *protocol;
    int tls_negotiated;
    int sr_negotiated;

    /* Print per-connection fields. */
    protocol = nbd_get_protocol (nbd);
    tls_negotiated = nbd_get_tls_negotiated (nbd);
    sr_negotiated = nbd_get_structured_replies_negotiated (nbd);

    if (!json_output) {
      if (protocol) {
        ansi_colour (ANSI_FG_GREY, fp);
        fprintf (fp, "protocol: %s", protocol);
        if (tls_negotiated >= 0)
          fprintf (fp, " %s TLS", tls_negotiated ? "with" : "without");
        if (sr_negotiated >= 0)
          fprintf (fp, ", using %s packets",
                   sr_negotiated ? "structured" : "simple");
        fprintf (fp, "\n");
        ansi_restore (fp);
      }
    }
    else {
      fprintf (fp, "{\n");
      if (protocol) {
        fprintf (fp, "\"protocol\": ");
        print_json_string (protocol);
        fprintf (fp, ",\n");
      }

      if (tls_negotiated >= 0)
        fprintf (fp, "\"TLS\": %s,\n", tls_negotiated ? "true" : "false");
      if (sr_negotiated >= 0)
        fprintf (fp, "\"structured\": %s,\n", sr_negotiated ? "true" : "false");
    }

    if (!list_all)
      list_okay = show_one_export (nbd, NULL, true, true);
    else
      list_okay = list_all_exports ();

    if (json_output)
      fprintf (fp, "}\n");
  }

  free_exports ();
  nbd_opt_abort (nbd);
  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  /* Close the output stream and copy it to the real stdout. */
  if (fclose (fp) == EOF) {
    fprintf (stderr, "%s: ", progname);
    perror ("fclose");
    exit (EXIT_FAILURE);
  }
  if (fputs (output, stdout) == EOF) {
    fprintf (stderr, "%s: ", progname);
    perror ("puts");
    exit (EXIT_FAILURE);
  }

  free (output);

  if (can) exit (can_exit_code);
  exit (list_okay ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* Connect the handle to the server. */
void
do_connect (struct nbd_handle *nbd)
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

/* The URI field in output is not meaningful unless there's a
 * persistent NBD server running, that is to say that nbdinfo was
 * invoked with a URI (not a [ subprocess ]).  If this returns false
 * it suppresses the uri: field in output.
 */
bool
uri_is_meaingful (void)
{
  return mode == MODE_URI;
}
