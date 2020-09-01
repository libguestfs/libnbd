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
	"os"
	"os/exec"
	"testing"
)

var exports []string

func listf(name string, desc string) int {
	if desc != "" {
		panic("expected empty description")
	}
	exports = append(exports, name)
	return 0
}

func Test220OptList(t *testing.T) {
	/* Require new-enough nbdkit */
	srcdir := os.Getenv("abs_top_srcdir")
	script := srcdir + "/tests/opt-list.sh"
	cmd := exec.Command("/bin/sh", "-c",
		"nbdkit sh --dump-plugin | grep -q has_list_exports=1")
	err := cmd.Run()
	if err != nil {
		t.Skip("Skipping: nbdkit too old for this test")
	}

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
		"nbdkit", "-s", "--exit-with-parent", "-v", "sh", script,
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	/* First pass: server fails NBD_OPT_LIST */
	exports = make([]string, 0, 4)
	_, err = h.OptList(listf)
	if err == nil {
		t.Fatalf("expected error")
	}
	if len(exports) != 0 {
		t.Fatalf("exports should be empty")
	}

	/* Second pass: server advertises 'a' and 'b' */
	exports = make([]string, 0, 4)
	count, err := h.OptList(listf)
	if err != nil {
		t.Fatalf("could not request opt_list: %s", err)
	}
	if count != 2 {
		t.Fatalf("unexpected count after opt_list")
	}
	if len(exports) != 2  || exports[0] != "a" || exports[1] != "b" {
		t.Fatalf("unexpected exports contents after opt_list")
	}

	/* Third pass: server advertises empty list */
	exports = make([]string, 0, 4)
	count, err = h.OptList(listf)
	if err != nil {
		t.Fatalf("could not request opt_list: %s", err)
	}
	if count != 0 {
		t.Fatalf("unexpected count after opt_list")
	}
	if len(exports) != 0 {
		t.Fatalf("exports should be empty")
	}

	/* Final pass: server advertises 'a' */
	exports = make([]string, 0, 4)
	count, err = h.OptList(listf)
	if err != nil {
		t.Fatalf("could not request opt_list: %s", err)
	}
	if count != 1 {
		t.Fatalf("unexpected count after opt_list")
	}
	if len(exports) != 1 || exports[0] != "a" {
		t.Fatalf("unexpected exports contents after opt_list")
	}

	h.OptAbort()
}
