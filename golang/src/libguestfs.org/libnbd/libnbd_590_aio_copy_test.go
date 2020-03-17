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
import "syscall"
import "testing"

var disk_size = uint(512 * 1024 * 1024)
var bs = uint64(65536)
var max_reads_in_flight = uint(16)
var bytes_read = uint(0)
var bytes_written = uint(0)

/* Functions to handle FdSet.
   XXX These probably only work on 64 bit platforms. */
func fdset_set(set *syscall.FdSet, fd int) {
	(*set).Bits[fd/64] |= 1 << (uintptr(fd) % 64)
}

func fdset_test(set *syscall.FdSet, fd int) bool {
	return (*set).Bits[fd/64]&(1<<(uintptr(fd)%64)) != 0
}

/* Functions to test socket direction. */
func dir_is_read(h *Libnbd) bool {
	dir, _ := h.AioGetDirection()
	return (uint32(dir) & AIO_DIRECTION_READ) != 0
}
func dir_is_write(h *Libnbd) bool {
	dir, _ := h.AioGetDirection()
	return (uint32(dir) & AIO_DIRECTION_WRITE) != 0
}

/* Queue of writes. */
type wbuf struct {
	buf    AioBuffer
	offset uint64
}

var writes []wbuf

/* Called whenever any asynchronous pread command from
   the source has completed. */
func read_completed(buf AioBuffer, offset uint64) int {
	bytes_read += buf.Size
	/* Move the AIO buffer to the write queue. */
	writes = append(writes, wbuf{buf, offset})
	/* Returning 1 means the command is automatically retired. */
	return 1
}

/* Called whenever any asynchronous pwrite command to the
   destination has completed. */
func write_completed(buf AioBuffer) int {
	bytes_written += buf.Size
	/* Now we have to manually free the AIO buffer. */
	buf.Free()
	/* Returning 1 means the command is automatically retired. */
	return 1
}

/* Copy between two libnbd handles using aynchronous I/O (AIO). */
func asynch_copy(t *testing.T, src *Libnbd, dst *Libnbd) {
	size, _ := dst.GetSize()

	/* This is our reading position in the source. */
	soff := uint64(0)

	for {
		/* Number of commands in flight on source and dest handles. */
		src_in_flight, _ := src.AioInFlight()
		dst_in_flight, _ := dst.AioInFlight()

		/* We're finished when we've read everything from the
		   source and there are no commands in flight. */
		if soff >= size && src_in_flight == 0 &&
			dst_in_flight == 0 {
			break
		}

		/* If we're able to submit more reads from the
		   source then do it now. */
		if soff < size && src_in_flight < max_reads_in_flight {
			n := bs
			if n > size-soff {
				n = size - soff
			}
			buf := MakeAioBuffer(uint(n))
			var optargs AioPreadOptargs
			optargs.CompletionCallbackSet = true
			optargs.CompletionCallback = func(*int) int {
				return read_completed(buf, soff)
			}
			src.AioPread(buf, soff, &optargs)
			soff += n
		}

		/* If there are any write commands waiting to
		   be issued, send them now. */
		for _, wb := range writes {
			var optargs AioPwriteOptargs
			optargs.CompletionCallbackSet = true
			optargs.CompletionCallback = func(*int) int {
				return write_completed(wb.buf)
			}
			dst.AioPwrite(wb.buf, wb.offset, &optargs)
		}
		writes = writes[:0]

		/* Now poll the file descriptors. */
		nfd := 1
		sfd, err := src.AioGetFd()
		if err != nil {
			t.Fatalf("src.AioGetFd: %s", err)
		}
		if sfd >= nfd {
			nfd = sfd + 1
		}
		dfd, err := dst.AioGetFd()
		if err != nil {
			t.Fatalf("dst.AioGetFd: %s", err)
		}
		if dfd >= nfd {
			nfd = dfd + 1
		}
		var rfds syscall.FdSet
		if dir_is_read(src) {
			fdset_set(&rfds, sfd)
		}
		if dir_is_read(dst) {
			fdset_set(&rfds, dfd)
		}
		var wfds syscall.FdSet
		if dir_is_write(src) {
			fdset_set(&wfds, sfd)
		}
		if dir_is_write(dst) {
			fdset_set(&wfds, dfd)
		}
		_, err = syscall.Select(nfd, &rfds, &wfds, nil, nil)
		if err != nil {
			t.Fatalf("select: %s", err)
		}

		if fdset_test(&rfds, sfd) && dir_is_read(src) {
			src.AioNotifyRead()
		} else if fdset_test(&wfds, sfd) && dir_is_write(src) {
			src.AioNotifyWrite()
		} else if fdset_test(&rfds, dfd) && dir_is_read(dst) {
			dst.AioNotifyRead()
		} else if fdset_test(&wfds, dfd) && dir_is_write(dst) {
			dst.AioNotifyWrite()
		}
	}
}

func Test590AioCopy(t *testing.T) {
	src, err := Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer src.Close()
	src.SetHandleName("src")

	var dst *Libnbd
	dst, err = Create()
	if err != nil {
		t.Fatalf("could not create handle: %s", err)
	}
	defer dst.Close()
	dst.SetHandleName("dst")

	err = src.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent", "-r",
		"pattern", fmt.Sprintf("size=%d", disk_size),
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	err = dst.ConnectCommand([]string{
		"nbdkit", "-s", "--exit-with-parent",
		"memory", fmt.Sprintf("size=%d", disk_size),
	})
	if err != nil {
		t.Fatalf("could not connect: %s", err)
	}

	asynch_copy(t, src, dst)
	if bytes_read != disk_size {
		t.Fatalf("bytes_read != disk_size")
	}
	if bytes_written != disk_size {
		t.Fatalf("bytes_written != disk_size")
	}
}
