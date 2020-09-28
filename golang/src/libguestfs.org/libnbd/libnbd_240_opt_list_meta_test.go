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
	"testing"
)

var count uint
var seen bool

func listmetaf(user_data int, name string) int {
	if user_data != 42 {
		panic("expected user_data == 42")
	}
	count++
	if (name == context_base_allocation) {
		seen = true
	}
	return 0
}

func Test240OptListMeta(t *testing.T) {
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

	/* First pass: empty query should give at least "base:allocation". */
	count = 0
	seen = false
	r, err := h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context: %s", err)
	}
	if r != count || r < 1 || !seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}
	max := count

	/* Second pass: bogus query has no response. */
	count = 0
	seen = false
	err = h.AddMetaContext("x-nosuch:")
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err = h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context: %s", err)
	}
	if r != 0 || r != count || seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	/* Third pass: specific query should have one match. */
	count = 0
	seen = false
	err = h.AddMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err = h.GetNrMetaContexts()
	if err != nil {
		t.Fatalf("could not request get_nr_meta_contexts: %s", err)
	}
	if r != 2 {
		t.Fatalf("wrong number of meta_contexts: %d", r)
	}
	tmp, err := h.GetMetaContext(1)
	if err != nil {
		t.Fatalf("could not request get_meta_context: %s", err)
	}
	if *tmp != context_base_allocation {
		t.Fatalf("wrong result of get_meta_context: %s", *tmp)
	}
	r, err = h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context: %s", err)
	}
	if r != 1 || r != count || !seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	/* Final pass: "base:" query should get at least "base:allocation" */
	count = 0
	seen = false
	err = h.ClearMetaContexts()
	if err != nil {
		t.Fatalf("could not request clear_meta_contexts: %s", err)
	}
	err = h.AddMetaContext("base:")
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err = h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context: %s", err)
	}
	if r < 1 || r > max || r != count || !seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	err = h.OptAbort()
	if err != nil {
		t.Fatalf("could not request opt_abort: %s", err)
	}
}
