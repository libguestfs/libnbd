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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#ifdef HAVE_LINUX_VM_SOCKETS_H
#include <linux/vm_sockets.h>
#elif HAVE_SYS_VSOCK_H
#include <sys/vsock.h>
#endif

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
  for (i = 0; i < list->len; ++i) {
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
    const char *explanation = NULL;

    if (raw_uri[0] == '/' &&
        (h->uri_allow_transports & LIBNBD_ALLOW_TRANSPORT_UNIX) != 0)
      explanation = "to open a local socket use \"nbd+unix://?socket=PATH\"";
    else if (strncasecmp (raw_uri, "localhost", 9) == 0 &&
             (h->uri_allow_transports & LIBNBD_ALLOW_TRANSPORT_TCP) != 0)
      explanation =
        "to open a local port use \"nbd://localhost\" or "
        "\"nbd://localhost:PORT\"";

    set_error (EINVAL,
               "NBD URI does not have a scheme: valid NBD URIs should "
               "start with a scheme like nbd://, nbds:// or nbd+unix://"
               "%s%s",
               explanation ? ": " : "",
               explanation ? explanation : "");
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
  for (i = 0; i < queries.len; i++) {
    if (strcmp (queries.ptr[i].name, "socket") == 0)
      unixsocket = queries.ptr[i].value;
  }

  if (socket_required && !unixsocket) {
    set_error (EINVAL, "cannot parse socket parameter from NBD URI "
               "(did you mean to use \"%s:///?socket=...\"?)",
               uri->scheme);
    goto cleanup;
  }
  else if (!socket_required && unixsocket) {
    set_error (EINVAL, "socket parameter is incompatible with \"%s:\" "
               "(did you mean to use \"%s+unix:///?socket=...\"?)",
               uri->scheme, !tls ? "nbd" : "nbds");
    goto cleanup;
  }

  /* TLS */
  if (tls && nbd_unlocked_set_tls (h, LIBNBD_TLS_REQUIRE) == -1)
    goto cleanup;

  /* Look for some tls-* parameters. */
  for (i = 0; i < queries.len; i++) {
    if (strcmp (queries.ptr[i].name, "tls-certificates") == 0) {
      if (! h->uri_allow_local_file) {
        set_error (EPERM,
                   "local file access (tls-certificates) is not allowed, "
                   "call nbd_set_uri_allow_local_file to enable this");
        goto cleanup;
      }
      if (nbd_unlocked_set_tls_certificates (h, queries.ptr[i].value) == -1)
        goto cleanup;
    }
    else if (strcmp (queries.ptr[i].name, "tls-psk-file") == 0) {
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
  case tcp: {                   /* TCP */
    char port_str[32];
    char *server;
    size_t server_len;

    snprintf (port_str, sizeof port_str,
              "%d", uri->port > 0 ? uri->port : 10809);

    /* If the uri->server field is NULL, substitute "localhost".  This
     * would be unusual and probably doesn't happen in reality. XXX
     */
    server = uri->server ? : "localhost";
    server_len = strlen (server);

    /* For a literal IPv6 address, the uri->server field will contain
     * "[addr]" and we must remove the brackets before passing it to
     * getaddrinfo in the state machine.
     */
    if (server_len >= 2 && server[0] == '[' && server[server_len-1] == ']') {
      server_len -= 2;
      server++;
      server[server_len] = '\0';
    }

    if (nbd_unlocked_aio_connect_tcp (h, server, port_str) == -1)
      goto cleanup;

    break;
  }

  case unix_sock:               /* Unix domain socket */
    if (nbd_unlocked_aio_connect_unix (h, unixsocket) == -1)
      goto cleanup;

    break;

  case vsock: {                 /* AF_VSOCK */
    uint32_t cid, svm_port;

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
  }

  ret = 0;

cleanup:
  for (i = 0; i < queries.len; ++i) {
    free (queries.ptr[i].name);
    free (queries.ptr[i].value);
  }
  free (queries.ptr);
  xmlFreeURI (uri);
  return ret;
}

/* This is best effort.  If we didn't save enough information when
 * connecting then return NULL but try to set errno and the error
 * string to something useful.
 */

static int append_query_params (char **query_params,
                                const char *key, const char *value)
  LIBNBD_ATTRIBUTE_NONNULL((1, 2, 3));

char *
nbd_unlocked_get_uri (struct nbd_handle *h)
{
  xmlURI uri = { 0 };
  bool using_tls;
  char *server = NULL;
  char *query_params = NULL;
  char *path = NULL;
  char *ret = NULL;

  if (h->tls == 2)              /* TLS == require */
    using_tls = true;
  else if (h->tls_negotiated)
    using_tls = true;
  else
    using_tls = false;

  /* Set scheme, server or socket. */
  if (h->hostname && h->port) {
    int r;

    uri.scheme = using_tls ? "nbds" : "nbd";
    /* We must try to guess here if the hostname is really an IPv6
     * numeric address.  Regular hostnames or IPv4 addresses wouldn't
     * contain ':'.
     */
    if (strchr (h->hostname, ':') == NULL)
      r = asprintf (&server, "%s:%s", h->hostname, h->port);
    else
      r = asprintf (&server, "[%s]:%s", h->hostname, h->port);
    if (r == -1) {
      set_error (errno, "asprintf");
      goto out;
    }
    uri.server = server;
  }

  else if (h->connaddrlen > 0) {
    switch (h->connaddr.ss_family) {
    case AF_INET:
    case AF_INET6: {
      int r, err;
      char host[NI_MAXHOST];
      char serv[NI_MAXSERV];

      uri.scheme = using_tls ? "nbds" : "nbd";
      err = getnameinfo ((struct sockaddr *) &h->connaddr, h->connaddrlen,
                         host, sizeof host, serv, sizeof serv,
                         NI_NUMERICHOST | NI_NUMERICSERV);
      if (err != 0) {
        set_error (0, "getnameinfo: %s", gai_strerror (err));
        goto out;
      }
      if (h->connaddr.ss_family == AF_INET)
        r = asprintf (&server, "%s:%s", host, serv);
      else /* AF_INET6 */
        r = asprintf (&server, "[%s]:%s", host, serv);
      if (r == -1) {
        set_error (errno, "asprintf");
        goto out;
      }
      uri.server = server;
      break;
    }

    case AF_UNIX: {
      struct sockaddr_un *sun = (struct sockaddr_un *) &h->connaddr;

      if (sun->sun_path[0] == '\0') {
        /* Unix domain sockets in the abstract namespace are in theory
         * supported in NBD URIs, but libxml2 cannot handle them so
         * libnbd cannot use them here or in nbd_connect_uri.
         */
        set_error (EPROTONOSUPPORT, "Unix domain sockets in the "
                   "abstract namespace are not yet supported");
        goto out;
      }

      uri.scheme = using_tls ? "nbds+unix" : "nbd+unix";
      if (append_query_params (&query_params, "socket", sun->sun_path) == -1)
        goto out;
      /* You have to set this otherwise xmlSaveUri generates bogus
       * URIs "nbd+unix:/?socket=..."
       */
      uri.server = "";
      break;
    }

#ifdef AF_VSOCK
    case AF_VSOCK: {
      struct sockaddr_vm *svm = (struct sockaddr_vm *) &h->connaddr;

      uri.scheme = using_tls ? "nbds+vsock" : "nbd+vsock";
      if (asprintf (&server, "%u:%u", svm->svm_cid, svm->svm_port) == -1) {
        set_error (errno, "asprintf");
        goto out;
      }
      uri.server = server;
      break;
    }
#endif

    default:
      set_error (EAFNOSUPPORT,
                 "address family %d not supported", h->connaddr.ss_family);
      goto out;
    }
  }

  else {
    set_error (EINVAL, "cannot construct a URI for this connection type");
    goto out;
  }

  /* Set other fields. */
  if (h->tls_username)
    uri.user = h->tls_username;
  if (h->export_name) {
    if (asprintf (&path, "/%s", h->export_name) == -1) {
      set_error (errno, "asprintf");
      goto out;
    }
    uri.path = path;
  }
  if (h->tls_certificates) {
    if (append_query_params (&query_params,
                             "tls-certificates", h->tls_certificates) == -1)
      goto out;
  }
  if (h->tls_psk_file) {
    if (append_query_params (&query_params,
                             "tls-psk-file", h->tls_psk_file) == -1)
      goto out;
  }

  uri.query_raw = query_params;

  /* Construct the final URI and return it. */
  ret = (char *) xmlSaveUri (&uri);
  if (ret == NULL)
    set_error (errno, "xmlSaveUri failed");
 out:
  free (server);
  free (query_params);
  free (path);
  return ret;
}

static int
append_query_params (char **query_params, const char *key, const char *value)
{
  char *old_query_params = *query_params;

  if (asprintf (query_params, "%s%s%s=%s",
                old_query_params ? : "",
                old_query_params ? "&" : "",
                key, value) == -1) {
    set_error (errno, "asprintf");
    return -1;
  }
  free (old_query_params);
  return 0;
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

char *
nbd_unlocked_get_uri (struct nbd_handle *h)
{
  set_error (ENOTSUP, NOT_SUPPORTED_ERROR);
  return NULL;
}

#endif /* !HAVE_LIBXML2 */
