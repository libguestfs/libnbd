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

func Test620Stats(t *testing.T) {
	h, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer h.Close()

	/* Pre-connection, stats start out at 0 */
	bs0, err := h.StatsBytesSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cs0, err := h.StatsChunksSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	br0, err := h.StatsBytesReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cr0, err := h.StatsChunksReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}

	if bs0 != 0 {
		t.Fatalf("unexpected value for bs0")
	}
	if cs0 != 0 {
		t.Fatalf("unexpected value for cs0")
	}
	if br0 != 0 {
		t.Fatalf("unexpected value for br0")
	}
	if cr0 != 0 {
		t.Fatalf("unexpected value for cr0")
	}

	/* Connection performs handshaking, which increments stats.
	 * The number of bytes/chunks here may grow over time as more features
	 * get automatically negotiated, so merely check that they are non-zero.
	 */
	err = h.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "null",
	})
	if err != nil {
		t.Fatalf("%s", err)
	}

	bs1, err := h.StatsBytesSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cs1, err := h.StatsChunksSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	br1, err := h.StatsBytesReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cr1, err := h.StatsChunksReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}

	if cs1 == 0 {
		t.Fatalf("unexpected value for cs1")
	}
	if bs1 <= cs1 {
		t.Fatalf("unexpected value for bs1")
	}
	if cr1 == 0 {
		t.Fatalf("unexpected value for cr1")
	}
	if br1 <= cr1 {
		t.Fatalf("unexpected value for br1")
	}

	/* A flush command should be one chunk out, one chunk back (even if
	 * structured replies are in use)
	 */
	err = h.Flush(nil)
	if err != nil {
		t.Fatalf("%s", err)
	}

	bs2, err := h.StatsBytesSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cs2, err := h.StatsChunksSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	br2, err := h.StatsBytesReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cr2, err := h.StatsChunksReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}

	if bs2 != bs1 + 28 {
		t.Fatalf("unexpected value for bs2")
	}
	if cs2 != cs1 + 1 {
		t.Fatalf("unexpected value for cs2")
	}
	if br2 != br1 + 16 {   /* assumes nbdkit uses simple reply */
		t.Fatalf("unexpected value for br2")
	}
	if cr2 != cr1 + 1 {
		t.Fatalf("unexpected value for cr2")
	}

	/* Stats are still readable after the connection closes; we don't know if
	 * the server sent reply bytes to our NBD_CMD_DISC, so don't insist on it.
	 */
	err = h.Shutdown(nil)
	if err != nil {
		t.Fatalf("%s", err)
	}

	bs3, err := h.StatsBytesSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cs3, err := h.StatsChunksSent()
	if err != nil {
		t.Fatalf("%s", err)
	}
	br3, err := h.StatsBytesReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}
	cr3, err := h.StatsChunksReceived()
	if err != nil {
		t.Fatalf("%s", err)
	}
	slop := uint64(1)
	if br2 == br3 {
		slop = uint64(0)
	}

	if bs3 <= bs2 {
		t.Fatalf("unexpected value for bs3")
	}
	if cs3 != cs2 + 1 {
		t.Fatalf("unexpected value for cs3")
	}
	if br3 < br2 {
		t.Fatalf("unexpected value for br3")
	}
	if cr3 != cr2 + slop {
		t.Fatalf("unexpected value for cr3")
	}
}
