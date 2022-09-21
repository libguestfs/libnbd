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

var setq_count uint
var setq_seen bool

func setmetaqf(user_data int, name string) int {
	if user_data != 42 {
		panic("expected user_data == 42")
	}
	setq_count++
	if (name == context_base_allocation) {
		setq_seen = true
	}
	return 0
}

func Test255OptSetMetaQueries(t *testing.T) {
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

	/* nbdkit does not match wildcard for SET, even though it does for LIST */
	setq_count = 0
	setq_seen = false
	r, err := h.OptSetMetaContextQueries([]string{"base:"},
		func(name string) int {
			return setmetaqf(42, name)
		})
	if err != nil {
		t.Fatalf("could not request opt_set_meta_context_queries: %s", err)
	}
	if r != setq_count || r != 0 || setq_seen {
		t.Fatalf("unexpected count after opt_set_meta_context_queries")
	}

	/* Negotiating with no contexts is not an error, but selects nothing.
	 * An explicit empty list overrides a non-empty implicit list.
	 */
	setq_count = 0
	setq_seen = false
	err = h.AddMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err = h.OptSetMetaContextQueries([]string{}, func(name string) int {
		return setmetaqf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_set_meta_context_queries: %s", err)
	}
	if r != setq_count || r != 0 || setq_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context_queries")
	}
	meta, err := h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not check can meta context: %s", err)
	}
	if meta {
		t.Fatalf("unexpected can meta context state")
	}

	/* Request 2 with expectation of 1; with SetRequestMetaContext off */
	setq_count = 0
	setq_seen = false
	r, err = h.OptSetMetaContextQueries([]string{
		"x-nosuch:context", context_base_allocation},
		func(name string) int {
	        return setmetaqf(42, name)
		})
	if err != nil {
		t.Fatalf("could not request opt_set_meta_context_queries: %s", err)
	}
	if r != 1 || r != setq_count || !setq_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context_queries")
	}
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not check can meta context: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected can meta context state")
	}

	/* Transition to transmission phase; our last set should remain active */
	err = h.SetRequestMetaContext(false)
	if err != nil {
		t.Fatalf("could not set_request_meta_context: %s", err)
	}
	err = h.OptGo()
	if err != nil {
		t.Fatalf("could not request opt_go: %s", err)
	}
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not check can meta context: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected can meta context state")
	}

	err = h.Shutdown(nil)
	if err != nil {
		t.Fatalf("could not request shutdown: %s", err)
	}
}
