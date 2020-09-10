/* libnbd golang tests
 * Copyright (C) 2013-2020 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

package libnbd

import "testing"

func Test120SetNonDefaults(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.SetExportName("name")
	if err != nil {
		t.Fatalf("could not set export name: %s", err)
	}
	name, err := h.GetExportName()
	if err != nil {
		t.Fatalf("could not get export name: %s", err)
	}
	if *name != "name" {
		t.Fatalf("unexpected export name: %s", *name)
	}

	err = h.SetFullInfo(true)
	if err != nil {
		t.Fatalf("could not set full info state: %s", err)
	}
	info, err := h.GetFullInfo()
	if err != nil {
		t.Fatalf("could not get full info state: %s", err)
	}
	if info != true {
		t.Fatalf("unexpected full info state")
	}

	err = h.SetTls(TLS_REQUIRE + 1)
	if err == nil {
		t.Fatalf("expect failure for out-of-range enum")
	}
	tls, err := h.GetTls()
	if err != nil {
		t.Fatalf("could not get tls state: %s", err)
	}
	if tls != TLS_DISABLE {
		t.Fatalf("unexpected tls state")
	}

	support, err := h.SupportsTls()
	if err != nil {
		t.Fatalf("could not check if tls is supported: %s", err)
	}
	if support {
		err = h.SetTls(TLS_ALLOW)
		if err != nil {
			t.Fatalf("could not set tls state: %s", err)
		}
		tls, err = h.GetTls()
		if err != nil {
			t.Fatalf("could not get tls state: %s", err)
		}
		if tls != TLS_ALLOW {
			t.Fatalf("unexpected tls state")
		}
	}

	err = h.SetRequestStructuredReplies(false)
	if err != nil {
		t.Fatalf("could not set structured replies state: %s", err)
	}
	sr, err := h.GetRequestStructuredReplies()
	if err != nil {
		t.Fatalf("could not get structured replies state: %s", err)
	}
	if sr != false {
		t.Fatalf("unexpected structured replies state")
	}

	err = h.SetHandshakeFlags(HANDSHAKE_FLAG_MASK + 1)
	if err == nil {
		t.Fatalf("expect failure for out-of-range flags")
	}
	flags, err := h.GetHandshakeFlags()
	if err != nil {
		t.Fatalf("could not get handshake flags: %s", err)
	}
	if flags != HANDSHAKE_FLAG_MASK {
		t.Fatalf("unexpected handshake flags")
	}

	err = h.SetHandshakeFlags(0)
	if err != nil {
		t.Fatalf("could not set handshake flags: %s", err)
	}
	flags, err = h.GetHandshakeFlags()
	if err != nil {
		t.Fatalf("could not get handshake flags: %s", err)
	}
	if flags != 0 {
		t.Fatalf("unexpected handshake flags")
	}

	err = h.SetOptMode(true)
	if err != nil {
		t.Fatalf("could not set opt mode state: %s", err)
	}
	opt, err := h.GetOptMode()
	if err != nil {
		t.Fatalf("could not get opt mode state: %s", err)
	}
	if opt != true {
		t.Fatalf("unexpected opt mode state")
	}
}
