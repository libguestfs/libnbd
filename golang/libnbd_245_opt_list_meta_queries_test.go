/* libnbd golang tests
 * Copyright (C) 2013-2022 Red Hat Inc.
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

var listq_count uint
var listq_seen bool

func listmetaqf(user_data int, name string) int {
	if user_data != 42 {
		panic("expected user_data == 42")
	}
	listq_count++
	if (name == context_base_allocation) {
		listq_seen = true
	}
	return 0
}

func Test245OptListMetaQueries(t *testing.T) {
	/* Get into negotiating state. */
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
		"nbdkit", "-s", "--exit-with-parent", "-v",
		"memory", "size=1M",
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	/* First pass: empty query should give at least "base:allocation".
	 * The explicit query overrides a non-empty nbd_add_meta_context.
	 */
	listq_count = 0
	listq_seen = false
	err = h.AddMetaContext("x-nosuch:")
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err := h.OptListMetaContextQueries([]string{ },
		func(name string) int {
	        return listmetaqf(42, name)
		})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context_queries: %s", err)
	}
	if r != listq_count || r < 1 || !listq_seen {
		t.Fatalf("unexpected count after opt_list_meta_context_queries")
	}

	/* Second pass: bogus query has no response. */
	listq_count = 0
	listq_seen = false
	err = h.ClearMetaContexts()
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err = h.OptListMetaContextQueries([]string{ "x-nosuch:" },
		func(name string) int {
	        return listmetaqf(42, name)
		})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context_queries: %s", err)
	}
	if r != 0 || r != listq_count || listq_seen {
		t.Fatalf("unexpected count after opt_list_meta_context_queries")
	}

	/* Third pass: specific query should have one match. */
	listq_count = 0
	listq_seen = false
	r, err = h.OptListMetaContextQueries([]string{
		"x-nosuch:", context_base_allocation },
		func(name string) int {
	        return listmetaqf(42, name)
		})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context_queries: %s", err)
	}
	if r != 1 || r != listq_count || !listq_seen {
		t.Fatalf("unexpected count after opt_list_meta_context_queries")
	}

	err = h.OptAbort()
	if err != nil {
		t.Fatalf("could not request opt_abort: %s", err)
	}
}
