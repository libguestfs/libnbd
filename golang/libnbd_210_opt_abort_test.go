/* libnbd golang tests
 * Copyright Red Hat
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

package libnbd

import "testing"

func Test210OptAbort(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.SetOptMode(true)
	if err != nil {
		t.Fatalf("could not set opt mode: %s", err)
	}

	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-v", "null",
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	proto, err := h.GetProtocol()
	if err != nil {
		t.Fatalf("could not get correct protocol: %s", err)
	} else if *proto != "newstyle-fixed" {
		t.Fatalf("wrong protocol detected: %s", *proto)
	}

	sr, err := h.GetStructuredRepliesNegotiated()
	if err != nil {
		t.Fatalf("could not determine structured replies: %s", err)
	} else if !sr {
		t.Fatalf("structured replies not negotiated")
	}

	err = h.OptAbort()
	if err != nil {
		t.Fatalf("could not abort option mode: %s", err)
	}

	closed, err := h.AioIsClosed()
	if err != nil {
		t.Fatalf("could not determine state after opt_abort: %s", err)
	} else if !closed {
		t.Fatalf("wrong state after opt_abort")
	}

	_, err = h.GetSize()
	if err == nil {
		t.Fatalf("getting a size should not be possible")
	}
}
