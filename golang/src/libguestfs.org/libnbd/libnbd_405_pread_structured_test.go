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
import "fmt"
import "testing"

var expected405 = make([]byte, 512)

func psf(buf2 []byte, offset uint64, status uint, error *int) int {
	if *error != 0 {
		panic("expected *error == 0")
	}
	if offset != 0 {
		panic("expected offset == 0")
	}
	if status != uint(READ_DATA) {
		panic("expected status == READ_DATA")
	}
	if !bytes.Equal(buf2, expected405) {
		fmt.Printf("buf2 = %s\nexpected = %s\n", buf2, expected405)
		panic("expected sub-buffer == expected pattern")
	}
	return 0
}

func Test405PReadStructured(t *testing.T) {
	// Expected data.
	for i := 0; i < 512; i += 8 {
		binary.BigEndian.PutUint64(expected405[i:i+8], uint64(i))
	}

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

	buf := make([]byte, 512)
	err = h.PreadStructured(buf, 0, psf, nil)
	if err != nil {
		t.Fatalf("%s", err)
	}
	if !bytes.Equal(buf, expected405) {
		t.Fatalf("did not read expected data")
	}

	var optargs PreadStructuredOptargs
	optargs.FlagsSet = true
	optargs.Flags = CMD_FLAG_DF
	err = h.PreadStructured(buf, 0, psf, &optargs)
	if err != nil {
		t.Fatalf("%s", err)
	}
	if !bytes.Equal(buf, expected405) {
		t.Fatalf("did not read expected data")
	}
}
