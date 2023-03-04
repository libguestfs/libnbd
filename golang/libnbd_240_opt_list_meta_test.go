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

import (
	"fmt";
	"testing"
)

var list_count uint
var list_seen bool

func listmetaf(user_data int, name string) int {
	if user_data != 42 {
		panic("expected user_data == 42")
	}
	list_count++
	if (name == context_base_allocation) {
		list_seen = true
	}
	return 0
}

func Test240OptListMeta(t *testing.T) {
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

	/* First pass: empty query should give at least "base:allocation". */
	list_count = 0
	list_seen = false
	r, err := h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context: %s", err)
	}
	if r != list_count || r < 1 || !list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}
	max := list_count

	/* Second pass: bogus query has no response. */
	list_count = 0
	list_seen = false
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
	if r != 0 || r != list_count || list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	/* Third pass: specific query should have one match. */
	list_count = 0
	list_seen = false
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
	if r != 1 || r != list_count || !list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	/* Fourth pass: opt_list_meta_context is stateless, so it should
     * not wipe status learned during opt_info
	 */
	list_count = 0
	list_seen = false
	_, err = h.GetSize()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.CanMetaContext(context_base_allocation)
	if err == nil {
		t.Fatalf("expected error")
	}
	err = h.OptInfo()
	if err != nil {
		t.Fatalf("opt_info failed unexpectedly: %s", err)
	}
        size, err := h.GetSize()
	if err != nil {
		t.Fatalf("get_size failed unexpectedly: %s", err)
	}
        if size != 1048576 {
		t.Fatalf("get_size gave wrong size")
        }
	meta, err := h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("can_meta_context failed unexpectedly: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected count after can_meta_context")
	}
	err = h.ClearMetaContexts()
	if err != nil {
		t.Fatalf("could not request clear_meta_contexts: %s", err)
	}
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
	if r != 0 || r != list_count || list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}
        size, err = h.GetSize()
	if err != nil {
		t.Fatalf("get_size failed unexpectedly: %s", err)
	}
        if size != 1048576 {
		t.Fatalf("get_size gave wrong size")
        }
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("can_meta_context failed unexpectedly: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected count after can_meta_context")
	}
	err = h.ClearMetaContexts()

	/* Final pass: "base:" query should get at least "base:allocation" */
	list_count = 0
	list_seen = false
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
	if r < 1 || r > max || r != list_count || !list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	err = h.OptAbort()
	if err != nil {
		t.Fatalf("could not request opt_abort: %s", err)
	}

	/* Repeat but this time without structured replies. Deal gracefully
	 * with older servers that don't allow the attempt.
	 */
	h, err = Create()
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
		t.Fatalf("could not set request structured replies: %s", err)
	}

	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-v",
		"memory", "size=1M",
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	bytes, err := h.StatsBytesSent()
	if err != nil {
		t.Fatalf("could not collect stats: %s", err)
	}

	list_count = 0
	list_seen = false
	r, err = h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		bytes2, err2 := h.StatsBytesSent()
		if err2 != nil {
			t.Fatalf("could not collect stats: %s", err2)
		}
		if bytes2 <= bytes {
			t.Fatalf("unexpected bytes sent after opt_list_meta_context")
		}
		fmt.Printf("ignoring failure from old server: %s", err)
	} else if r < 1 || r != list_count || !list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	/* Now enable structured replies, and a retry should pass. */
	sr, err := h.OptStructuredReply()
	if err != nil {
		t.Fatalf("could not request opt_structured_reply: %s", err)
	}
	if !sr {
		t.Fatalf("structured replies not enabled: %s", err)
	}

	list_count = 0
	list_seen = false
	r, err = h.OptListMetaContext(func(name string) int {
	        return listmetaf(42, name)
	})
	if err != nil {
		t.Fatalf("could not request opt_list_meta_context: %s", err)
	}
	if r < 1 || r != list_count || !list_seen {
		t.Fatalf("unexpected count after opt_list_meta_context")
	}

	err = h.OptAbort()
	if err != nil {
		t.Fatalf("could not request opt_abort: %s", err)
	}
}
