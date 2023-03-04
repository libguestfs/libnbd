/* libnbd example
 * Copyright Red Hat
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
//   ./simple_copy nbd+unix:///?socket=/tmp.nbd >/dev/null
//
package main

import (
	"flag"
	"os"

	"libguestfs.org/libnbd"
)

var (
	requestSize = flag.Uint("buffer-size", 2048*1024, "maximum request size in bytes")
)

func main() {
	flag.Parse()

	h, err := libnbd.Create()
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

	buf := make([]byte, *requestSize)
	var offset uint64

	for offset < size {
		if size-offset < uint64(len(buf)) {
			buf = buf[:offset-size]
		}

		err = h.Pread(buf, offset, nil)
		if err != nil {
			panic(err)
		}

		_, err := os.Stdout.Write(buf)
		if err != nil {
			panic(err)
		}

		offset += uint64(len(buf))
	}
}
