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
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "internal.h"
#include "vector.h"

#ifdef HAVE_LIBXML2

#include <libxml/uri.h>

/* Connect to an NBD URI. */
int
nbd_unlocked_connect_uri (struct nbd_handle *h, const char *uri)
{
  if (nbd_unlocked_aio_connect_uri (h, uri) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

struct uri_query {
  char *name;
  char *value;
};

DEFINE_VECTOR_TYPE (uri_query_list, struct uri_query)

/* Parse the query_raw substring of a URI into a list of decoded queries.
 * Return 0 on success or -1 on error.
 */
static int
parse_uri_queries (const char *query_raw, uri_query_list *list)
{
  /* Borrows from libvirt's viruri.c:virURIParseParams() */
  const char *end, *eq;
  const char *query = query_raw;
  size_t i;

  if (!query || !*query)
    return 0;

  while (*query) {
    struct uri_query q = {0};

    /* Find the next separator, or end of the string. */
    end = strchr (query, '&');
    if (!end)
      end = strchr (query, ';');
    if (!end)
      end = query + strlen (query);

    /* Find the first '=' character between here and end. */
    eq = strchr (query, '=');
    if (eq && eq >= end) eq = NULL;

    if (end == query) {
      /* Empty section (eg. "&&"). */
      goto next;
    } else if (!eq) {
      /* If there is no '=' character, then we have just "name"
       * and consistent with CGI.pm we assume value is "".
       */
      q.name = xmlURIUnescapeString (query, end - query, NULL);
      if (!q.name) goto error;
    } else if (eq+1 == end) {
      /* Or if we have "name=" here (works around annoying
       * problem when calling xmlURIUnescapeString with len = 0).
       */
      q.name = xmlURIUnescapeString (query, eq - query, NULL);
      if (!q.name) goto error;
    } else if (query == eq) {
      /* If the '=' character is at the beginning then we have
       * "=value" and consistent with CGI.pm we _ignore_ this.
       */
      goto next;
    } else {
      /* Otherwise it's "name=value". */
      q.name = xmlURIUnescapeString (query, eq - query, NULL);
      if (!q.name)
        goto error;
      q.value = xmlURIUnescapeString (eq+1, end - (eq+1), NULL);
      if (!q.value) {
        free (q.name);
        goto error;
      }
    }
    if (!q.value) {
      q.value = strdup ("");
      if (!q.value) {
        free (q.name);
        goto error;
      }
    }

    /* Append to the list of queries. */
    if (uri_query_list_append (list, q) == -1) {
      free (q.name);
      free (q.value);
      goto error;
    }

  next:
    query = end;
    if (*query) query++; /* skip '&' separator */
  }

  return 0;

error:
  for (i = 0; i < list->size; ++list) {
    free (list->ptr[i].name);
    free (list->ptr[i].value);
  }
  uri_query_list_reset (list);
  return -1;
}

int
nbd_unlocked_aio_connect_uri (struct nbd_handle *h, const char *raw_uri)
{
  xmlURIPtr uri = NULL;
  enum { tcp, unix_sock, vsock } transport;
  bool tls, socket_required;
  uri_query_list queries = empty_vector;
  int i, r;
  int ret = -1;
  const char *unixsocket = NULL;
  char port_str[32];
  uint32_t cid, svm_port;

  uri = xmlParseURI (raw_uri);
  if (!uri) {
    set_error (EINVAL, "unable to parse URI: %s", raw_uri);
    goto cleanup;
  }
  if (parse_uri_queries (uri->query_raw, &queries) == -1) {
    set_error (EINVAL, "unable to parse URI queries: %s", uri->query_raw);
    goto cleanup;
  }

  /* Scheme. */
  if (uri->scheme) {
    if (strcmp (uri->scheme, "nbd") == 0) {
      transport = tcp;
      tls = false;
      socket_required = false;
    }
    else if (strcmp (uri->scheme, "nbds") == 0) {
      transport = tcp;
      tls = true;
      socket_required = false;
    }
    else if (strcmp (uri->scheme, "nbd+unix") == 0) {
      transport = unix_sock;
      tls = false;
      socket_required = true;
    }
    else if (strcmp (uri->scheme, "nbds+unix") == 0) {
      transport = unix_sock;
      tls = true;
      socket_required = true;
    }
    else if (strcmp (uri->scheme, "nbd+vsock") == 0) {
      transport = vsock;
      tls = false;
      socket_required = false;
    }
    else if (strcmp (uri->scheme, "nbds+vsock") == 0) {
      transport = vsock;
      tls = true;
      socket_required = false;
    }
    else {
      set_error (EINVAL, "unknown NBD URI scheme: %s", uri->scheme);
      goto cleanup;
    }
  }
  else {
    set_error (EINVAL, "NBD URI does not have a scheme, valid NBD URIs should start with nbd://, nbds://, nbd+unix:// or similar");
    goto cleanup;
  }

  /* Insist on the scheme://[authority][/absname][?queries] form. */
  if (strncmp (raw_uri + strlen (uri->scheme), "://", 3)) {
    set_error (EINVAL, "URI must begin with '%s://'", uri->scheme);
    goto cleanup;
  }

  /* Check the transport is allowed. */
  if ((transport == tcp &&
       (h->uri_allow_transports & LIBNBD_ALLOW_TRANSPORT_TCP) == 0) ||
      (transport == unix_sock &&
       (h->uri_allow_transports & LIBNBD_ALLOW_TRANSPORT_UNIX) == 0) ||
      (transport == vsock &&
       (h->uri_allow_transports & LIBNBD_ALLOW_TRANSPORT_VSOCK) == 0)) {
    set_error (EPERM, "URI transport %s is not permitted", uri->scheme);
    goto cleanup;
  }

  /* Check TLS is allowed. */
  if ((tls && h->uri_allow_tls == LIBNBD_TLS_DISABLE) ||
      (!tls && h->uri_allow_tls == LIBNBD_TLS_REQUIRE)) {
    set_error (EPERM, "URI TLS setting %s is not permitted", uri->scheme);
    goto cleanup;
  }

  /* Parse the socket parameter. */
  for (i = 0; i < queries.size; i++) {
    if (strcmp (queries.ptr[i].name, "socket") == 0)
      unixsocket = queries.ptr[i].value;
  }

  if (socket_required && !unixsocket) {
    set_error (EINVAL, "cannot parse socket parameter from NBD URI: %s",
               uri->query_raw ? : "NULL");
    goto cleanup;
  }
  else if (!socket_required && unixsocket) {
    set_error (EINVAL, "socket=%s URI query is incompatible with \"%s:\" "
               "(did you mean to use \"%s\" ?)",
               unixsocket, uri->scheme, !tls ? "nbd+unix:" : "nbds+unix:");
    goto cleanup;
  }

  /* TLS */
  if (tls && nbd_unlocked_set_tls (h, LIBNBD_TLS_REQUIRE) == -1)
    goto cleanup;

  /* Look for some tls-* parameters.  XXX More to come. */
  for (i = 0; i < queries.size; i++) {
    if (strcmp (queries.ptr[i].name, "tls-psk-file") == 0) {
      if (! h->uri_allow_local_file) {
        set_error (EPERM,
                   "local file access (tls-psk-file) is not allowed, "
                   "call nbd_set_uri_allow_local_file to enable this");
        goto cleanup;
      }
      if (nbd_unlocked_set_tls_psk_file (h, queries.ptr[i].value) == -1)
        goto cleanup;
    }
  }

  /* Username. */
  if (uri->user && nbd_unlocked_set_tls_username (h, uri->user) == -1)
    goto cleanup;

  /* Export name. */
  if (uri->path) {
    /* Since we require scheme://authority above, any path is absolute */
    assert (uri->path[0] == '/');
    r = nbd_unlocked_set_export_name (h, &uri->path[1]);
  }
  else
    r = nbd_unlocked_set_export_name (h, "");
  if (r == -1)
    goto cleanup;

  switch (transport) {
  case tcp:                     /* TCP */
    snprintf (port_str, sizeof port_str,
              "%d", uri->port > 0 ? uri->port : 10809);
    if (nbd_unlocked_aio_connect_tcp (h, uri->server ? : "localhost",
                                      port_str) == -1)
      goto cleanup;

    break;

  case unix_sock:               /* Unix domain socket */
    if (nbd_unlocked_aio_connect_unix (h, unixsocket) == -1)
      goto cleanup;

    break;

  case vsock:                   /* AF_VSOCK */
    /* Server, if present, must be the numeric CID.  Else we
     * assume the host (2).
     */
    if (uri->server && strcmp (uri->server, "") != 0) {
      /* This doesn't deal with overflow, but that seems unlikely to
       * matter because you'll end up with a CID one way or another.
       */
      if (sscanf (uri->server, "%" SCNu32, &cid) != 1) {
        set_error (EINVAL, "cannot parse vsock CID from NBD URI: %s",
                   uri->server);
        goto cleanup;
      }
    }
    else
      cid = 2;

    /* For unknown reasons libxml2 sets uri->port = -1 if the
     * authority field is not present at all.  So we must check that
     * uri->port > 0.  This prevents us from using certain very large
     * port numbers, but that's not an issue that matters in practice.
     */
    svm_port = uri->port > 0 ? (uint32_t) uri->port : 10809;
    if (nbd_unlocked_aio_connect_vsock (h, cid, svm_port) == -1)
      goto cleanup;

    break;
  }

  ret = 0;

cleanup:
  for (i = 0; i < queries.size; ++i) {
    free (queries.ptr[i].name);
    free (queries.ptr[i].value);
  }
  free (queries.ptr);
  xmlFreeURI (uri);
  return ret;
}

#else /* !HAVE_LIBXML2 */

#define NOT_SUPPORTED_ERROR \
  "libnbd was compiled without libxml2 support, so we do not support NBD URI"

int
nbd_unlocked_connect_uri (struct nbd_handle *h, const char *uri)
{
  set_error (ENOTSUP, NOT_SUPPORTED_ERROR);
  return -1;
}

int
nbd_unlocked_aio_connect_uri (struct nbd_handle *h, const char *raw_uri)
{
  set_error (ENOTSUP, NOT_SUPPORTED_ERROR);
  return -1;
}

#endif /* !HAVE_LIBXML2 */
