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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#include "internal.h"

int
nbd_unlocked_set_tls (struct nbd_handle *h, int tls)
{
#ifdef HAVE_GNUTLS
  h->tls = tls;
  return 0;
#else
  /* Don't allow setting this to any non-zero value, but setting it to
   * 0 (disable TLS) is OK.
   */
  if (tls != 0) {
    set_error (ENOTSUP, "libnbd was compiled without TLS support");
    return -1;
  }
  return 0;
#endif
}

int
nbd_unlocked_get_tls (struct nbd_handle *h)
{
  return h->tls;
}

int
nbd_unlocked_set_tls_certificates (struct nbd_handle *h, const char *dir)
{
  char *new_dir;

  new_dir = strdup (dir);
  if (!new_dir) {
    set_error (errno, "strdup");
    return -1;
  }
  h->tls_certificates = new_dir;
  return 0;
}

int
nbd_unlocked_set_tls_verify_peer (struct nbd_handle *h, bool verify)
{
  h->tls_verify_peer = verify;
  return 0;
}

int
nbd_unlocked_get_tls_verify_peer (struct nbd_handle *h)
{
  return h->tls_verify_peer;
}

int
nbd_unlocked_set_tls_username (struct nbd_handle *h, const char *username)
{
  char *new_user;

  new_user = strdup (username);
  if (!new_user) {
    set_error (errno, "strdup");
    return -1;
  }
  h->tls_username = new_user;
  return 0;
}

char *
nbd_unlocked_get_tls_username (struct nbd_handle *h)
{
  char *ret;

  if (h->tls_username) {
    ret = strdup (h->tls_username);
    if (ret == NULL) {
      set_error (errno, "strdup");
      return NULL;
    }
    return ret;
  }

  /* Otherwise we return the local login name. */
  ret = malloc (L_cuserid);
  if (ret == NULL) {
    set_error (errno, "malloc");
    return NULL;
  }
  if (getlogin_r (ret, L_cuserid) != 0) {
    set_error (errno, "getlogin");
    free (ret);
    return NULL;
  }
  return ret;
}

int
nbd_unlocked_set_tls_psk_file (struct nbd_handle *h, const char *filename)
{
  char *new_file;

  new_file = strdup (filename);
  if (!new_file) {
    set_error (errno, "strdup");
    return -1;
  }
  h->tls_psk_file = new_file;
  return 0;
}

#ifdef HAVE_GNUTLS

static ssize_t
tls_recv (struct socket *sock, void *buf, size_t len)
{
  ssize_t r;

  r = gnutls_record_recv (sock->u.tls.session, buf, len);
  if (r < 0) {
    if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
      errno = EAGAIN;
      return -1;
    }
    set_error (0, "gnutls_record_recv: %s", gnutls_strerror (r));
    errno = EIO;
    return -1;
  }
  return r;
}

static ssize_t
tls_send (struct socket *sock, const void *buf, size_t len)
{
  ssize_t r;

  r = gnutls_record_send (sock->u.tls.session, buf, len);
  if (r < 0) {
    if (r == GNUTLS_E_INTERRUPTED || r == GNUTLS_E_AGAIN) {
      errno = EAGAIN;
      return -1;
    }
    set_error (0, "gnutls_record_send: %s", gnutls_strerror (r));
    errno = EIO;
    return -1;
  }
  return r;
}

static int
tls_get_fd (struct socket *sock)
{
  return sock->u.tls.oldsock->ops->get_fd (sock->u.tls.oldsock);
}

/* XXX Calling gnutls_bye is possible, but it may send and receive
 * data over the wire which would require modifications to the state
 * machine.  So instead we abruptly drop the TLS session.
 */
static int
tls_close (struct socket *sock)
{
  int r;

  r = sock->u.tls.oldsock->ops->close (sock->u.tls.oldsock);
  gnutls_deinit (sock->u.tls.session);
  if (sock->u.tls.pskcreds)
    gnutls_psk_free_client_credentials (sock->u.tls.pskcreds);
  if (sock->u.tls.xcreds)
    gnutls_certificate_free_credentials (sock->u.tls.xcreds);
  free (sock);
  return r;
}

static struct socket_ops crypto_ops = {
  recv: tls_recv,
  send: tls_send,
  get_fd: tls_get_fd,
  close: tls_close,
};

/* Look up the user's key in the PSK file. */
static int
lookup_key (const char *pskfile, const char *username,
            gnutls_datum_t *key)
{
  FILE *fp;
  const size_t ulen = strlen (username);
  size_t len = 0;
  ssize_t r;
  char *line = NULL;

  fp = fopen (pskfile, "r");
  if (fp == NULL) {
    set_error (errno, "open: %s", pskfile);
    goto error;
  }
  while ((r = getline (&line, &len, fp)) != -1) {
    if (r > 0 && line[r-1] == '\n') line[--r] = '\0';
    if (r > 0 && line[r-1] == '\r') line[--r] = '\0';

    if (r > ulen+1 &&
        strncmp (line, username, ulen) == 0 &&
        line[ulen] == ':') {
      key->data = (unsigned char *) strdup (&line[ulen+1]);
      if (key->data == NULL) {
        set_error (errno, "strdup");
        goto error;
      }
      key->size = r - ulen - 1;
      break;
    }
  }
  if (ferror (fp)) {
    set_error (errno, "%s: getline failed", pskfile);
    goto error;
  }

  if (key->data == NULL) {
    set_error (EINVAL, "%s: username %s was not found in PSK file",
               pskfile, username);
    goto error;
  }

  fclose (fp);
  free (line);
  return 0;

 error:
  if (fp)
    fclose (fp);
  free (line);
  return -1;
}

static gnutls_psk_client_credentials_t
set_up_psk_credentials (struct nbd_connection *conn, gnutls_session_t session)
{
  int err;
  const char prio[] = TLS_PRIORITY ":" "+ECDHE-PSK:+DHE-PSK:+PSK";
  gnutls_datum_t key = { .data = NULL };
  char *username = NULL;
  gnutls_psk_client_credentials_t ret = NULL;

  err = gnutls_priority_set_direct (session, prio, NULL);
  if (err < 0) {
    set_error (0, "gnutls_priority_set_direct: %s", gnutls_strerror (err));
    goto error;
  }

  username = nbd_unlocked_get_tls_username (conn->h);
  if (username == NULL)
    goto error;

  if (lookup_key (conn->h->tls_psk_file, username, &key) == -1)
    goto error;

  err = gnutls_psk_allocate_client_credentials (&ret);
  if (err < 0) {
    set_error (0, "gnutls_psk_allocate_client_credentials: %s",
               gnutls_strerror (err));
    goto error;
  }
  err = gnutls_psk_set_client_credentials (ret, username,
                                           &key, GNUTLS_PSK_KEY_HEX);
  if (err < 0) {
    set_error (0, "gnutls_psk_set_client_credentials: %s",
               gnutls_strerror (err));
    goto error;
  }

  err = gnutls_credentials_set (session, GNUTLS_CRD_PSK, ret);
  if (err < 0) {
    set_error (0, "gnutls_credentials_set: %s", gnutls_strerror (err));
    goto error;
  }

  free (username);
  free (key.data);
  return ret;

 error:
  free (username);
  free (key.data);
  if (ret)
    gnutls_psk_free_client_credentials (ret);
  return NULL;
}

static int
load_certificates (const char *path, gnutls_certificate_credentials_t *ret)
{
  int err;
  char *cacert = NULL;
  char *clientcert = NULL;
  char *clientkey = NULL;
  char *cacrl = NULL;

  if (asprintf (&cacert, "%s/ca-cert.pem", path) == -1) {
    set_error (errno, "asprintf");
    goto error;
  }
  if (asprintf (&clientcert, "%s/client-cert.pem", path) == -1) {
    set_error (errno, "asprintf");
    goto error;
  }
  if (asprintf (&clientkey, "%s/client-key.pem", path) == -1) {
    set_error (errno, "asprintf");
    goto error;
  }
  if (asprintf (&cacrl, "%s/ca-crl.pem", path) == -1) {
    set_error (errno, "asprintf");
    goto error;
  }

  /* Only ca-cert.pem must be present. */
  if (access (cacert, R_OK) == -1)
    return 0;

  err = gnutls_certificate_allocate_credentials (ret);
  if (err < 0) {
    set_error (0, "gnutls_certificate_allocate_credentials: %s",
               gnutls_strerror (err));
    goto error;
  }

  err = gnutls_certificate_set_x509_trust_file (*ret, cacert,
                                                GNUTLS_X509_FMT_PEM);
  if (err < 0) {
    set_error (0, "gnutls_certificate_set_x509_trust_file: %s: %s",
               cacert, gnutls_strerror (err));
    goto error;
  }

  /* Optional for client certification authentication. */
  if (access (clientcert, R_OK) == 0 && access (clientkey, R_OK) == 0) {
    err = gnutls_certificate_set_x509_key_file (*ret, clientcert, clientkey,
                                                GNUTLS_X509_FMT_PEM);
    if (err < 0) {
      set_error (0, "gnutls_certificate_set_x509_key_file: %s, %s: %s",
                 clientcert, clientkey, gnutls_strerror (err));
      goto error;
    }
  }

  if (access (cacrl, R_OK) == 0) {
    err = gnutls_certificate_set_x509_crl_file (*ret, cacrl,
                                                GNUTLS_X509_FMT_PEM);
    if (err < 0) {
      set_error (0, "gnutls_certificate_set_x509_crl_file: %s: %s",
                 cacrl, gnutls_strerror (err));
      goto error;
    }
  }

  free (cacert);
  free (clientcert);
  free (clientkey);
  free (cacrl);
  return 0;

 error:
  if (*ret)
    gnutls_certificate_free_credentials (*ret);
  *ret = NULL;
  free (cacert);
  free (clientcert);
  free (clientkey);
  free (cacrl);
  return -1;
}

static gnutls_certificate_credentials_t
set_up_certificate_credentials (struct nbd_connection *conn,
                                gnutls_session_t session)
{
  int err;
  gnutls_certificate_credentials_t ret = NULL;
  const char *home = getenv ("HOME");
  char *path = NULL;

  err = gnutls_priority_set_direct (session, TLS_PRIORITY, NULL);
  if (err < 0) {
    set_error (0, "gnutls_priority_set_direct: %s", gnutls_strerror (err));
    goto error;
  }

  /* Try to load the certificates from the directory. */
  if (conn->h->tls_certificates) {
    if (load_certificates (conn->h->tls_certificates, &ret) == -1)
      goto error;
    if (ret)
      goto found_certificates;
  }
  else {
    if (geteuid () != 0 && home != NULL) {
      if (asprintf (&path, "%s/.pki/%s", home, PACKAGE_NAME) == -1) {
        set_error (errno, "asprintf");
        goto error;
      }
      if (load_certificates (path, &ret) == -1)
        goto error;
      if (ret)
        goto found_certificates;
      free (path);
      if (asprintf (&path, "%s/.config/pki/%s", home, PACKAGE_NAME) == -1) {
        set_error (errno, "asprintf");
        goto error;
      }
      if (load_certificates (path, &ret) == -1)
        goto error;
      if (ret)
        goto found_certificates;
    }
    else { /* geteuid () == 0 */
      if (load_certificates (sysconfdir "/pki/" PACKAGE_NAME, &ret) == -1)
        goto error;
      if (ret)
        goto found_certificates;
    }
  }
  set_error (EINVAL,
             "TLS requested, "
             "but certificates directory nor PSK was specified, "
             "this is probably a programming error in your application");
  goto error;

 found_certificates:
  if (conn->hostname && conn->h->tls_verify_peer)
    gnutls_session_set_verify_cert (session, conn->hostname, 0);

  free (path);
  return ret;

 error:
  gnutls_certificate_free_credentials (ret);
  free (path);
  return NULL;
}

/* Called from the state machine after receiving an ACK from
 * NBD_OPT_STARTTLS when we want to start upgrading the connection to
 * TLS.  Allocate and initialize the TLS session and set up the socket
 * ops.
 */
struct socket *
nbd_internal_crypto_create_session (struct nbd_connection *conn,
                                    struct socket *oldsock)
{
  int err;
  struct socket *sock;
  gnutls_session_t session;
  gnutls_psk_client_credentials_t pskcreds = NULL;
  gnutls_certificate_credentials_t xcreds = NULL;

  err = gnutls_init (&session, GNUTLS_CLIENT|GNUTLS_NONBLOCK);
  if (err < 0) {
    set_error (errno, "gnutls_init: %s", gnutls_strerror (err));
    return NULL;
  }

  if (conn->h->tls_psk_file) {
    pskcreds = set_up_psk_credentials (conn, session);
    if (pskcreds == NULL) {
      gnutls_deinit (session);
      return NULL;
    }
  }
  else {
    xcreds = set_up_certificate_credentials (conn, session);
    if (xcreds == NULL) {
      gnutls_deinit (session);
      return NULL;
    }
  }

  /* Wrap the underlying socket with GnuTLS. */
  gnutls_transport_set_int (session, oldsock->ops->get_fd (oldsock));

  gnutls_handshake_set_timeout (session,
                                GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

  sock = malloc (sizeof *sock);
  if (sock == NULL) {
    set_error (errno, "malloc");
    gnutls_deinit (session);
    if (pskcreds)
      gnutls_psk_free_client_credentials (pskcreds);
    if (xcreds)
      gnutls_certificate_free_credentials (xcreds);
    return NULL;
  }
  sock->u.tls.session = session;
  sock->u.tls.pskcreds = pskcreds;
  sock->u.tls.xcreds = xcreds;
  sock->u.tls.oldsock = oldsock;
  sock->ops = &crypto_ops;
  return sock;
}

/* Return the read/write direction. */
bool
nbd_internal_crypto_is_reading (struct nbd_connection *conn)
{
  assert (conn->sock->u.tls.session);
  return gnutls_record_get_direction (conn->sock->u.tls.session) == 0;
}

/* Continue with the TLS handshake.  Returns 0 if the handshake
 * completed successfully, 1 if the handshake is continuing, and -1 if
 * there was a GnuTLS error.
 */
int
nbd_internal_crypto_handshake (struct nbd_connection *conn)
{
  int err;

  assert (conn->sock->u.tls.session);
  err = gnutls_handshake (conn->sock->u.tls.session);
  if (err == 0)
    return 0;
  if (!gnutls_error_is_fatal (err))
    return 1;
  set_error (0, "gnutls_handshake: %s", gnutls_strerror (err));
  return -1;
}

#else /* !HAVE_GNUTLS */

/* These functions should never be called from the state machine if
 * !HAVE_GNUTLS.
 */
int
nbd_internal_crypto_create_session (struct nbd_connection *conn)
{
  abort ();
}

bool
nbd_internal_crypto_is_reading (struct nbd_connection *conn)
{
  abort ();
}

int
nbd_internal_crypto_handshake (struct nbd_connection *conn)
{
  abort ();
}

#endif /* !HAVE_GNUTLS */
