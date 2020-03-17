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

import (
	"fmt"
	"os"
	"strings"
	"testing"
)

var srcdir = os.Getenv("srcdir")
var script = srcdir + "/../../../../tests/meta-base-allocation.sh"

var entries []uint32

func mcf(metacontext string, offset uint64, e []uint32, error *int) int {
	if *error != 0 {
		panic("expected *error == 0")
	}
	if metacontext == "base:allocation" {
		entries = e
	}
	return 0
}

// Seriously WTF?
func mc_compare(a1 []uint32, a2 []uint32) bool {
	if len(a1) != len(a2) {
		return false
	}
	for i := 0; i < len(a1); i++ {
		if a1[i] != a2[i] {
			return false
		}
	}
	return true
}

// Like, WTF, again?
func mc_to_string(a []uint32) string {
	ss := make([]string, len(a))
	for i := 0; i < len(a); i++ {
		ss[i] = fmt.Sprint(a[i])
	}
	return strings.Join(ss, ", ")
}

func Test460BlockStatus(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.AddMetaContext("base:allocation")
	if err != nil {
		t.Fatalf("%s", err)
	}
	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-v",
		"sh", script,
	})
	if err != nil {
		t.Fatalf("%s", err)
	}

	err = h.BlockStatus(65536, 0, mcf, nil)
	if err != nil {
		t.Fatalf("%s", err)
	}
	if !mc_compare(entries, []uint32{
		8192, 0,
		8192, 1,
		16384, 3,
		16384, 2,
		16384, 0,
	}) {
		t.Fatalf("unexpected entries (1): %s", mc_to_string(entries))
	}

	err = h.BlockStatus(1024, 32256, mcf, nil)
	if err != nil {
		t.Fatalf("%s", err)
	}
	if !mc_compare(entries, []uint32{
		512, 3,
		16384, 2,
	}) {
		t.Fatalf("unexpected entries (2): %s", mc_to_string(entries))
	}

	var optargs BlockStatusOptargs
	optargs.FlagsSet = true
	optargs.Flags = CMD_FLAG_REQ_ONE
	err = h.BlockStatus(1024, 32256, mcf, &optargs)
	if err != nil {
		t.Fatalf("%s", err)
	}
	if !mc_compare(entries, []uint32{512, 3}) {
		t.Fatalf("unexpected entries (3): %s", mc_to_string(entries))
	}

}
