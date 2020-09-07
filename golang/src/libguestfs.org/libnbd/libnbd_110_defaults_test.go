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

func Test110Defaults(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	name, err := h.GetExportName()
	if err != nil {
		t.Fatalf("could not get export name: %s", err)
	}
	if *name != "" {
		t.Fatalf("unexpected export name: %s", *name)
	}

	info, err := h.GetFullInfo()
	if err != nil {
		t.Fatalf("could not get full info state: %s", err)
	}
	if info != false {
		t.Fatalf("unexpected full info state")
	}

	tls, err := h.GetTls()
	if err != nil {
		t.Fatalf("could not get tls state: %s", err)
	}
	if tls != TLS_DISABLE {
		t.Fatalf("unexpected tls state")
	}

	sr, err := h.GetRequestStructuredReplies()
	if err != nil {
		t.Fatalf("could not get structured replies state: %s", err)
	}
	if sr != true {
		t.Fatalf("unexpected structured replies state")
	}

	flags, err := h.GetHandshakeFlags()
	if err != nil {
		t.Fatalf("could not get handshake flags: %s", err)
	}
	if flags != HANDSHAKE_FLAG_FIXED_NEWSTYLE | HANDSHAKE_FLAG_NO_ZEROES {
		t.Fatalf("unexpected handshake flags")
	}

	opt, err := h.GetOptMode()
	if err != nil {
		t.Fatalf("could not get opt mode state: %s", err)
	}
	if opt != false {
		t.Fatalf("unexpected opt mode state")
	}
}
