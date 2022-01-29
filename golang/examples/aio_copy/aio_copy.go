/* libnbd example
 * Copyright (C) 2013-2022 Red Hat Inc.
 * Examples are under a permissive BSD-like license.  See also
 * golang/examples/LICENSE-For-EXAMPLES
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of Red Hat nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RED HAT AND CONTRIBUTORS ''AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RED HAT OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// Copy image from NBD URI to stdout.
//
// Example:
//
//   ./aio_copy nbd+unix:///?socket=/tmp.nbd >/dev/null
//
package main

import (
	"container/list"
	"flag"
	"os"
	"syscall"

	"libguestfs.org/libnbd"
)

var (
	// These options give best performance with fast NVMe drive.
	requestSize = flag.Uint("request-size", 256*1024, "maximum request size in bytes")
	requests    = flag.Uint("requests", 4, "maximum number of requests in flight")

	h *libnbd.Libnbd

	// Keeping commands in a queue ensures commands are written in the right
	// order, even if they complete out of order. This allows parallel reads
	// with non-seekable output.
	queue list.List
)

// command keeps state of single AioPread call while the read is handled by
// libnbd, until the command reach the front of the queue and can be writen to
// the output.
type command struct {
	buf    libnbd.AioBuffer
	ready  bool
}

func main() {
	flag.Parse()

	var err error

	h, err = libnbd.Create()
	if err != nil {
		panic(err)
	}
	defer h.Close()

	err = h.ConnectUri(flag.Arg(0))
	if err != nil {
		panic(err)
	}

	size, err := h.GetSize()
	if err != nil {
		panic(err)
	}

	var offset uint64

	for offset < size || queue.Len() > 0 {

		for offset < size && inflightRequests() < *requests {
			length := *requestSize
			if size-offset < uint64(length) {
				length = uint(size - offset)
			}
			startRead(offset, length)
			offset += uint64(length)
		}

		waitForCompletion()

		for readReady() {
			finishRead()
		}
	}
}

func inflightRequests() uint {
	n, err := h.AioInFlight()
	if err != nil {
		panic(err)
	}
	return n
}

func waitForCompletion() {
	start := inflightRequests()

	for {
		_, err := h.Poll(-1)
		if err != nil {
			panic(err)
		}

		if inflightRequests() < start {
			break // A read completed.
		}
	}
}

func startRead(offset uint64, length uint) {
	cmd := &command{buf: libnbd.MakeAioBuffer(length)}

	args := libnbd.AioPreadOptargs{
		CompletionCallbackSet: true,
		CompletionCallback: func(error *int) int {
			if *error != 0 {
				// This is not documented, but *error is errno value translated
				// from the the NBD server error.
				err := syscall.Errno(*error).Error()
				panic(err)
			}
			cmd.ready = true
			return 1
		},
	}

	_, err := h.AioPread(cmd.buf, offset, &args)
	if err != nil {
		panic(err)
	}

	queue.PushBack(cmd)
}

func readReady() bool {
	return queue.Len() > 0 && queue.Front().Value.(*command).ready
}

func finishRead() {
	e := queue.Front()
	queue.Remove(e)

	cmd := e.Value.(*command)

	_, err := os.Stdout.Write(cmd.buf.Slice())
	if err != nil {
		panic(err)
	}

	cmd.buf.Free()
}
