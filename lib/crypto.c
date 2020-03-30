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

/* NB: may_set_error = false. */
int
nbd_unlocked_get_tls (struct nbd_handle *h)
{
  return h->tls;
}

int
nbd_unlocked_get_tls_negotiated (struct nbd_handle *h)
{
  return h->tls_negotiated;
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
  free (h->tls_certificates);
  h->tls_certificates = new_dir;
  return 0;
}

/* NB: may_set_error = false. */
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
  free (h->tls_username);
  h->tls_username = new_user;
  return 0;
}

char *
nbd_unlocked_get_tls_username (struct nbd_handle *h)
{
  char *s, *ret;
  size_t len;

  if (h->tls_username) {
    ret = strdup (h->tls_username);
    if (ret == NULL) {
      set_error (errno, "strdup");
      return NULL;
    }
    return ret;
  }

  /* Otherwise we return the local login name.  Try $LOGNAME first for
   * two reasons: (1) So the user can override it.  (2) Because
   * getlogin fails with ENXIO if there is no controlling terminal
   * (which is often the case in test and embedded environments).
   */
  s = getenv ("LOGNAME");
  if (s) {
    ret = strdup (s);
    if (ret == NULL) {
      set_error (errno, "strdup");
      return NULL;
    }
    return ret;
  }

  len = 16;
  ret = NULL;
  while (ret == NULL) {
    char *oldret = ret;

    ret = realloc (oldret, len);
    if (ret == NULL) {
      set_error (errno, "realloc");
      free (oldret);
      return NULL;
    }

    if (getlogin_r (ret, len) != 0) {
      if (errno == ERANGE) {
        /* Try again with a larger buffer. */
        len *= 2;
        continue;
      }
      set_error (errno, "getlogin_r");
      free (ret);
      return NULL;
    }
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
  free (h->tls_psk_file);
  h->tls_psk_file = new_file;
  return 0;
}

#ifdef HAVE_GNUTLS

static ssize_t
tls_recv (struct nbd_handle *h, struct socket *sock, void *buf, size_t len)
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
tls_send (struct nbd_handle *h,
          struct socket *sock, const void *buf, size_t len, int flags)
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

static bool
tls_pending (struct socket *sock)
{
  return gnutls_record_check_pending (sock->u.tls.session) > 0;
}

static int
tls_get_fd (struct socket *sock)
{
  return sock->u.tls.oldsock->ops->get_fd (sock->u.tls.oldsock);
}

static bool
tls_shut_writes (struct nbd_handle *h, struct socket *sock)
{
  int r = gnutls_bye (sock->u.tls.session, GNUTLS_SHUT_WR);

  if (r == GNUTLS_E_AGAIN || r == GNUTLS_E_INTERRUPTED)
    return false;
  if (r != 0)
    debug (h, "ignoring gnutls_bye failure: %s", gnutls_strerror (r));
  return sock->u.tls.oldsock->ops->shut_writes (h, sock->u.tls.oldsock);
}

/* XXX Calling gnutls_bye(GNUTLS_SHUT_RDWR) is possible, but it may send
 * and receive data over the wire which would require further modifications
 * to the state machine.  So instead we abruptly drop the TLS session.
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
  .recv = tls_recv,
  .send = tls_send,
  .pending = tls_pending,
  .get_fd = tls_get_fd,
  .shut_writes = tls_shut_writes,
  .close = tls_close,
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

  fp = fopen (pskfile, "re");
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
set_up_psk_credentials (struct nbd_handle *h, gnutls_session_t session)
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

  username = nbd_unlocked_get_tls_username (h);
  if (username == NULL)
    goto error;

  if (lookup_key (h->tls_psk_file, username, &key) == -1)
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
  /* Only ca-cert.pem must be present. */
  if (access (cacert, R_OK) == -1) {
    free (cacert);
    return 0;
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
set_up_certificate_credentials (struct nbd_handle *h,
                                gnutls_session_t session, bool *is_error)
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
  if (h->tls_certificates) {
    if (load_certificates (h->tls_certificates, &ret) == -1)
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

  /* Not found. */
  free (path);
  return NULL;

 found_certificates:
#ifdef HAVE_GNUTLS_SESSION_SET_VERIFY_CERT
  if (h->hostname && h->tls_verify_peer)
    gnutls_session_set_verify_cert (session, h->hostname, 0);
#else
  debug (h, "ignoring nbd_set_tls_verify_peer, this requires GnuTLS >= 3.4.6");
#endif

  err = gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, ret);
  if (err < 0) {
    set_error (0, "gnutls_credentials_set: %s", gnutls_strerror (err));
    goto error;
  }

  free (path);
  return ret;

 error:
  gnutls_certificate_free_credentials (ret);
  free (path);
  *is_error = true;
  return NULL;
}

static gnutls_certificate_credentials_t
set_up_system_CA (struct nbd_handle *h, gnutls_session_t session)
{
  int err;
  gnutls_certificate_credentials_t ret = NULL;

  err = gnutls_priority_set_direct (session, TLS_PRIORITY, NULL);
  if (err < 0) {
    set_error (0, "gnutls_priority_set_direct: %s", gnutls_strerror (err));
    return NULL;
  }

  err = gnutls_certificate_allocate_credentials (&ret);
  if (err < 0) {
    set_error (0, "gnutls_certificate_allocate_credentials: %s",
               gnutls_strerror (err));
    return NULL;
  }

  err = gnutls_certificate_set_x509_system_trust (ret);
  if (err < 0) {
    set_error (0, "gnutls_certificate_set_x509_system_trust: %s",
               gnutls_strerror (err));
    gnutls_certificate_free_credentials (ret);
    return NULL;
  }

  err = gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, ret);
  if (err < 0) {
    set_error (0, "gnutls_credentials_set: %s", gnutls_strerror (err));
    gnutls_certificate_free_credentials (ret);
    return NULL;
  }

  return ret;
}

/* Called from the state machine after receiving an ACK from
 * NBD_OPT_STARTTLS when we want to start upgrading the connection to
 * TLS.  Allocate and initialize the TLS session and set up the socket
 * ops.
 */
struct socket *
nbd_internal_crypto_create_session (struct nbd_handle *h,
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

  /* If we have the server name, pass SNI. */
  if (h->hostname) {
    err = gnutls_server_name_set (session, GNUTLS_NAME_DNS,
                                  h->hostname, strlen (h->hostname));
    if (err < 0) {
      set_error (errno, "gnutls_server_name_set: %s", gnutls_strerror (err));
      gnutls_deinit (session);
      return NULL;
    }
  }

  if (h->tls_psk_file) {
    pskcreds = set_up_psk_credentials (h, session);
    if (pskcreds == NULL) {
      gnutls_deinit (session);
      return NULL;
    }
  }
  else {
    bool is_error = false;

    xcreds = set_up_certificate_credentials (h, session, &is_error);
    if (xcreds == NULL) {
      if (!is_error) {
        /* Fallback case: use system CA. */
        xcreds = set_up_system_CA (h, session);
        if (xcreds == NULL)
          is_error = true;
      }
    }

    if (is_error) {
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
nbd_internal_crypto_is_reading (struct nbd_handle *h)
{
  assert (h->sock->u.tls.session);
  return gnutls_record_get_direction (h->sock->u.tls.session) == 0;
}

/* Continue with the TLS handshake.  Returns 0 if the handshake
 * completed successfully, 1 if the handshake is continuing, and -1 if
 * there was a GnuTLS error.
 */
int
nbd_internal_crypto_handshake (struct nbd_handle *h)
{
  int err;
  gnutls_handshake_description_t in, out;
  const gnutls_session_t session = h->sock->u.tls.session;

  assert (session);
  err = gnutls_handshake (session);
  if (err == 0)
    return 0;
  if (!gnutls_error_is_fatal (err))
    return 1;

  /* Get some additional debug information about where in the
   * handshake protocol it failed.  You have to look up these codes in
   * <gnutls/gnutls.h>.
   */
  in = gnutls_handshake_get_last_in (session);
  out = gnutls_handshake_get_last_out (session);
  set_error (0, "gnutls_handshake: %s (%d/%d)",
             gnutls_strerror (err), (int) in, (int) out);
  return -1;
}

/* The state machine calls this when TLS has definitely been enabled
 * on the connection (after the handshake), and we use it to print
 * useful debugging information.
 */
void
nbd_internal_crypto_debug_tls_enabled (struct nbd_handle *h)
{
  if_debug (h) {
    const gnutls_session_t session = h->sock->u.tls.session;
    const gnutls_cipher_algorithm_t cipher = gnutls_cipher_get (session);
    const gnutls_kx_algorithm_t kx = gnutls_kx_get (session);
    const gnutls_mac_algorithm_t mac = gnutls_mac_get (session);

    debug (h,
           "connection is using TLS: "
           "cipher %s (%zu bits) key exchange %s mac %s (%zu bits)",
           gnutls_cipher_get_name (cipher),
           8 * gnutls_cipher_get_key_size (cipher),
           gnutls_kx_get_name (kx),
           gnutls_mac_get_name (mac),
           8 * gnutls_mac_get_key_size (mac)
           );
  }
}

#else /* !HAVE_GNUTLS */

/* These functions should never be called from the state machine if
 * !HAVE_GNUTLS.
 */
struct socket *
nbd_internal_crypto_create_session (struct nbd_handle *h,
                                    struct socket *oldsock)
{
  abort ();
}

bool
nbd_internal_crypto_is_reading (struct nbd_handle *h)
{
  abort ();
}

int
nbd_internal_crypto_handshake (struct nbd_handle *h)
{
  abort ();
}

void
nbd_internal_crypto_debug_tls_enabled (struct nbd_handle *h)
{
  abort ();
}

#endif /* !HAVE_GNUTLS */
