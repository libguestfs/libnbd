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

import "bytes"
import "encoding/binary"
import "testing"

func Test500AioPRead(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-v",
		"pattern", "size=512",
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	buf := MakeAioBuffer(512)
	defer buf.Free()
	var cookie uint64
	cookie, err = h.AioPread(buf, 0, nil)
	if err != nil {
		t.Fatalf("%s", err)
	}
	for {
		var b bool
		b, err = h.AioCommandCompleted(cookie)
		if err != nil {
			t.Fatalf("%s", err)
		}
		if b {
			break
		}
		h.Poll(-1)
	}

	// Expected data.
	expected := make([]byte, 512)
	for i := 0; i < 512; i += 8 {
		binary.BigEndian.PutUint64(expected[i:i+8], uint64(i))
	}

	if !bytes.Equal(buf.Bytes(), expected) {
		t.Fatalf("did not read expected data")
	}
}
