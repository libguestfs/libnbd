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

import "fmt"
import "testing"

var msgs []string

func f(context string, msg string) int {
	fmt.Printf("debug callback called: context=%s msg=%s\n",
		context, msg)
	msgs = append(msgs, context, msg)
	return 0
}

func Test600DebugCallback(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.SetDebugCallback(f)
	if err != nil {
		t.Fatalf("%s", err)
	}
	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "null",
	})
	if err != nil {
		t.Fatalf("%s", err)
	}
	err = h.Shutdown(nil)
	if err != nil {
		t.Fatalf("%s", err)
	}

	fmt.Printf("msgs = %s\n", msgs)
}
