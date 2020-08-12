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
	"testing"
)

func Test230OptInfo(t *testing.T) {
	srcdir := os.Getenv("srcdir")
	script := srcdir + "/../../../../tests/opt-info.sh"

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

	err = h.AddMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("could not add meta context: %s", err)
	}

	/* No size, flags, or meta-contexts yet */
	_, err = h.GetSize()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.IsReadOnly()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.CanMetaContext(context_base_allocation)
	if err == nil {
		t.Fatalf("expected error")
	}

	/* info with no prior name gets info on "" */
	err = h.OptInfo()
	if err != nil {
		t.Fatalf("opt_info failed unexpectedly: %s", err)
	}
	size, err := h.GetSize()
	if err != nil {
		t.Fatalf("size failed unexpectedly: %s", err)
	}
	if size != 0 {
		t.Fatalf("unexpected size")
	}
	ro, err := h.IsReadOnly()
	if err != nil {
		t.Fatalf("readonly failed unexpectedly: %s", err)
	}
	if !ro {
		t.Fatalf("unexpected readonly")
	}
	meta, err := h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("can_meta failed unexpectedly: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected meta context")
	}

	/* info on something not present fails, wipes out prior info */
	err = h.SetExportName("a")
	if err != nil {
		t.Fatalf("set export name failed unexpectedly: %s", err)
	}
	err = h.OptInfo()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.GetSize()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.IsReadOnly()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.CanMetaContext(context_base_allocation)
	if err == nil {
		t.Fatalf("expected error")
	}

	/* info for a different export */
	err = h.SetExportName("b")
	if err != nil {
		t.Fatalf("set export name failed unexpectedly: %s", err)
	}
	err = h.OptInfo()
	if err != nil {
		t.Fatalf("opt_info failed unexpectedly: %s", err)
	}
	size, err = h.GetSize()
	if err != nil {
		t.Fatalf("size failed unexpectedly: %s", err)
	}
	if size != 1 {
		t.Fatalf("unexpected size")
	}
	ro, err = h.IsReadOnly()
	if err != nil {
		t.Fatalf("readonly failed unexpectedly: %s", err)
	}
	if ro {
		t.Fatalf("unexpected readonly")
	}
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("can_meta failed unexpectedly: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected meta context")
	}

	/* go on something not present */
	err = h.SetExportName("a")
	if err != nil {
		t.Fatalf("set export name failed unexpectedly: %s", err)
	}
	err = h.OptGo()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.GetSize()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.IsReadOnly()
	if err == nil {
		t.Fatalf("expected error")
	}
	_, err = h.CanMetaContext(context_base_allocation)
	if err == nil {
		t.Fatalf("expected error")
	}

	/* go on a valid export */
	err = h.SetExportName("good")
	if err != nil {
		t.Fatalf("set export name failed unexpectedly: %s", err)
	}
	err = h.OptGo()
	if err != nil {
		t.Fatalf("opt_go failed unexpectedly: %s", err)
	}
	size, err = h.GetSize()
	if err != nil {
		t.Fatalf("size failed unexpectedly: %s", err)
	}
	if size != 4 {
		t.Fatalf("unexpected size")
	}
	ro, err = h.IsReadOnly()
	if err != nil {
		t.Fatalf("readonly failed unexpectedly: %s", err)
	}
	if !ro {
		t.Fatalf("unexpected readonly")
	}
	meta, err = h.CanMetaContext(context_base_allocation)
	if err != nil {
		t.Fatalf("can_meta failed unexpectedly: %s", err)
	}
	if !meta {
		t.Fatalf("unexpected meta context")
	}

	/* now info is no longer valid, but does not wipe data */
	err = h.SetExportName("a")
	if err == nil {
		t.Fatalf("expected error")
	}
	name, err := h.GetExportName()
	if err != nil {
		t.Fatalf("get export name failed unexpectedly: %s", err)
	}
	if *name != "good" {
		t.Fatalf("wrong name returned")
	}
	err = h.OptInfo()
	if err == nil {
		t.Fatalf("expected error")
	}
	size, err = h.GetSize()
	if err != nil {
		t.Fatalf("size failed unexpectedly: %s", err)
	}
	if size != 4 {
		t.Fatalf("unexpected size")
	}

	h.Shutdown(nil)
}
