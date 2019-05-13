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
#include <unistd.h>
#include <errno.h>

#include "internal.h"

int
nbd_unlocked_set_tls (struct nbd_handle *h, int tls)
{
  h->tls = tls;
  return 0;
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
