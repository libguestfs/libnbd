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

var set_count uint
var set_seen bool

func setmetaf(user_data int, name string) int {
	if user_data != 42 {
		panic("expected user_data == 42")
	}
	set_count++
	if (name == context_base_allocation) {
		set_seen = true
	}
	return 0
}

func Test250OptSetMeta(t *testing.T) {
	/* Get into negotiating state without structured replies. */
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	err = h.SetOptMode(true)
	if err != nil {
		t.Fatalf("could not set opt mode: %s", err)
	}
	err = h.SetRequestStructuredReplies(false)
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

	/* No contexts negotiated yet; CanMeta should be error if any requested */
	sr, err := h.GetStructuredRepliesNegotiated()
	if err != nil {
		t.Fatalf("could not check structured replies negotiated: %s", err)
	}
	if sr {
		t.Fatalf("unexpected structured replies state")
	}
	meta, err := h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not check can meta context: %s", err)
	}
	if meta {
		t.Fatalf("unexpected can meta context state")
	}
	err = h.AddMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	_, err = h.CanMetaContext(context_base_allocation)
	if err == nil {
		t.Fatalf("expected error")
	}

	/* SET cannot succeed until SR is negotiated. */
	set_count = 0
	set_seen = false
	_, err = h.OptSetMetaContext(func(name string) int {
		return setmetaf(42, name)
	})
	if err == nil {
		t.Fatalf("expected error")
	}
	if set_count != 0 || set_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context")
	}
	sr, err = h.OptStructuredReply()
	if err != nil {
		t.Fatalf("could not trigger opt_structured_reply: %s", err)
	}
	if !sr {
		t.Fatalf("unexpected structured replies state")
	}
	sr, err = h.GetStructuredRepliesNegotiated()
	if err != nil {
		t.Fatalf("could not check structured replies negotiated: %s", err)
	}
	if !sr {
		t.Fatalf("unexpected structured replies state")
	}
	_, err = h.CanMetaContext(context_base_allocation)
	if err == nil {
		t.Fatalf("expected error")
	}

	/* nbdkit does not match wildcard for SET, even though it does for LIST */
	set_count = 0
	set_seen = false
	err = h.ClearMetaContexts()
	if err != nil {
		t.Fatalf("could not request clear_meta_contexts: %s", err)
	}
	err = h.AddMetaContext("base:")
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	r, err := h.OptSetMetaContext(func(name string) int {
		return setmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_set_meta_context: %s", err)
	}
	if r != set_count || r != 0 || set_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context")
	}

	/* Negotiating with no contexts is not an error, but selects nothing */
	set_count = 0
	set_seen = false
	err = h.ClearMetaContexts()
	if err != nil {
		t.Fatalf("could not request clear_meta_contexts: %s", err)
	}
	r, err = h.OptSetMetaContext(func(name string) int {
		return setmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_set_meta_context: %s", err)
	}
	if r != set_count || r != 0 || set_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context")
	}
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not check can meta context: %s", err)
	}
	if meta {
		t.Fatalf("unexpected can meta context state")
	}

	/* Request 2 with expectation of 1; with SetRequestMetaContext off */
	set_count = 0
	set_seen = false
	err = h.AddMetaContext("x-nosuch:context")
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	err = h.AddMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
	}
	err = h.SetRequestMetaContext(false)
	if err != nil {
		t.Fatalf("could not set_request_meta_context: %s", err)
	}
	r, err = h.OptSetMetaContext(func(name string) int {
		return setmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_set_meta_context: %s", err)
	}
	if r != 1 || r != set_count || !set_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context")
	}
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not check can meta context: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected can meta context state")
	}

	/* Transition to transmission phase; our last set should remain active */
	err = h.ClearMetaContexts()
	if err != nil {
		t.Fatalf("could not request clear_meta_contexts: %s", err)
	}
	err = h.AddMetaContext("x-nosuch:context")
	if err != nil {
		t.Fatalf("could not request add_meta_context: %s", err)
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

	/* Now too late to set; but should not lose earlier state */
	set_count = 0
	set_seen = false
	_, err = h.OptSetMetaContext(func(name string) int {
		return setmetaf(42, name)
	})
	if err == nil {
		t.Fatalf("expected error")
	}
	if set_count != 0 || set_seen {
		t.Fatalf("unexpected set_count after opt_set_meta_context")
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
