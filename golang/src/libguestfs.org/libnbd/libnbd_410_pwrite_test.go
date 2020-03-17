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
import "testing"

func Test410PWrite(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-v",
		"memory", "size=512",
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	/* Write a pattern and read it back. */
	buf := make([]byte, 512)
	for i := 0; i < 512; i += 2 {
		buf[i] = 0x55
		buf[i+1] = 0xAA
	}
	err = h.Pwrite(buf, 0, nil)
	if err != nil {
		t.Fatalf("%s", err)
	}
	buf2 := make([]byte, 512)
	err = h.Pread(buf2, 0, nil)
	if err != nil {
		t.Fatalf("%s", err)
	}

	if !bytes.Equal(buf, buf2) {
		t.Fatalf("did not read back same data as written")
	}
}
