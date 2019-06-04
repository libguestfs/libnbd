/* NBD client library in userspace
 * Copyright (C) 2013-2019 Red Hat Inc.
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
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef HAVE_LIBXML2
#include <libxml/uri.h>
#endif

#include "internal.h"

static int
error_unless_ready (struct nbd_handle *h)
{
  if (nbd_internal_is_state_ready (get_state (h)))
    return 0;

  /* Why did it fail? */
  if (nbd_internal_is_state_closed (get_state (h))) {
    set_error (0, "connection is closed");
    return -1;
  }

  if (nbd_internal_is_state_dead (get_state (h)))
    /* Don't set the error here, keep the error set when
     * the connection died.
     */
    return -1;

  /* Should probably never happen. */
  set_error (0, "connection in an unexpected state (%s)",
             nbd_internal_state_short_string (get_state (h)));
  return -1;
}

static int
wait_until_connected (struct nbd_handle *h)
{
  while (nbd_internal_is_state_connecting (get_state (h))) {
    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return error_unless_ready (h);
}

/* Connect to an NBD URI. */
int
nbd_unlocked_connect_uri (struct nbd_handle *h, const char *uri)
{
  if (nbd_unlocked_aio_connect_uri (h, uri) == -1)
    return -1;

  return wait_until_connected (h);
}

/* Connect to a Unix domain socket. */
int
nbd_unlocked_connect_unix (struct nbd_handle *h, const char *unixsocket)
{
  if (nbd_unlocked_aio_connect_unix (h, unixsocket) == -1)
    return -1;

  return wait_until_connected (h);
}

/* Connect to a TCP port. */
int
nbd_unlocked_connect_tcp (struct nbd_handle *h,
                          const char *hostname, const char *port)
{
  if (nbd_unlocked_aio_connect_tcp (h, hostname, port) == -1)
    return -1;

  return wait_until_connected (h);
}

/* Connect to a local command. */
int
nbd_unlocked_connect_command (struct nbd_handle *h, char **argv)
{
  if (nbd_unlocked_aio_connect_command (h, argv) == -1)
    return -1;

  return wait_until_connected (h);
}

int
nbd_unlocked_aio_connect (struct nbd_handle *h,
                          const struct sockaddr *addr, socklen_t len)
{
  memcpy (&h->connaddr, addr, len);
  h->connaddrlen = len;

  return nbd_internal_run (h, cmd_connect_sockaddr);
}

int
nbd_unlocked_aio_connect_uri (struct nbd_handle *h, const char *raw_uri)
{
#ifdef HAVE_LIBXML2
  xmlURIPtr uri = NULL;
  bool tcp, tls;
  int r;

  uri = xmlParseURI (raw_uri);
  if (!uri) {
    set_error (EINVAL, "unable to parse URI: %s", raw_uri);
    goto error;
  }

  /* Scheme. */
  if (uri->scheme) {
    if (strcmp (uri->scheme, "nbd") == 0) {
      tcp = true;
      tls = false;
    }
    else if (strcmp (uri->scheme, "nbds") == 0) {
      tcp = true;
      tls = true;
    }
    else if (strcmp (uri->scheme, "nbd+unix") == 0) {
      tcp = false;
      tls = false;
    }
    else if (strcmp (uri->scheme, "nbds+unix") == 0) {
      tcp = false;
      tls = true;
    }
    else goto unknown_scheme;
  }
  else {
  unknown_scheme:
    set_error (EINVAL, "unknown URI scheme: %s", uri->scheme ? : "NULL");
    goto error;
  }

  /* TLS */
  if (tls && nbd_unlocked_set_tls (h, 2) == -1)
    goto error;
  /* XXX If uri->query_raw includes TLS parameters, we should call
   * nbd_unlocked_set_tls_* to match...
   */

  /* Export name. */
  if (uri->path) {
    if (uri->path[0] == '/')
      r = nbd_unlocked_set_export_name (h, &uri->path[1]);
    else
      r = nbd_unlocked_set_export_name (h, uri->path);
  }
  else
    r = nbd_unlocked_set_export_name (h, "");
  if (r == -1)
    goto error;

  if (tcp) {
    char port_str[32];

    snprintf (port_str, sizeof port_str,
              "%d", uri->port > 0 ? uri->port : 10809);
    if (nbd_unlocked_aio_connect_tcp (h, uri->server ? : "localhost",
                                      port_str) == -1)
      goto error;
  }
  else /* Unix domain socket */ {
    const char *p, *unixsocket = NULL;

    /* XXX parsing and unquoting are all wrong.  This will only work
     * in trivial cases.
     */
    if (uri->query_raw) {
      p = strstr (uri->query_raw, "socket=");
      if (p)
        unixsocket = p+7;
    }
    if (!unixsocket) {
      set_error (EINVAL, "cannot parse socket parameter from NBD URI: %s",
                 uri->query_raw ? : "NULL");
      goto error;
    }

    if (nbd_unlocked_aio_connect_unix (h, unixsocket) == -1)
      goto error;
  }

  xmlFreeURI (uri);
  return 0;

error:
  xmlFreeURI (uri);
  return -1;

#else /* !HAVE_LIBXML2 */
  set_error (ENOTSUP, "libnbd was compiled without libxml2 support, "
             "so we do not support NBD URI");
  return -1;
#endif /* !HAVE_LIBXML2 */
}

int
nbd_unlocked_aio_connect_unix (struct nbd_handle *h, const char *unixsocket)
{
  if (h->unixsocket)
    free (h->unixsocket);
  h->unixsocket = strdup (unixsocket);
  if (!h->unixsocket) {
    set_error (errno, "strdup");
    return -1;
  }

  return nbd_internal_run (h, cmd_connect_unix);
}

int
nbd_unlocked_aio_connect_tcp (struct nbd_handle *h,
                              const char *hostname, const char *port)
{
  if (h->hostname)
    free (h->hostname);
  h->hostname = strdup (hostname);
  if (!h->hostname) {
    set_error (errno, "strdup");
    return -1;
  }
  if (h->port)
    free (h->port);
  h->port = strdup (port);
  if (!h->port) {
    set_error (errno, "strdup");
    return -1;
  }

  return nbd_internal_run (h, cmd_connect_tcp);
}

int
nbd_unlocked_aio_connect_command (struct nbd_handle *h, char **argv)
{
  char **copy;

  copy = nbd_internal_copy_string_list (argv);
  if (!copy) {
    set_error (errno, "copy_string_list");
    return -1;
  }

  if (h->argv)
    nbd_internal_free_string_list (h->argv);
  h->argv = copy;

  return nbd_internal_run (h, cmd_connect_command);
}
