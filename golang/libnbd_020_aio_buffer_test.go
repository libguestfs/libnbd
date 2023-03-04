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
	"bytes"
	"testing"
)

func TestAioBuffer(t *testing.T) {
	/* Create a buffer with uninitialized backing array. */
	buf := MakeAioBuffer(uint(32))
	defer buf.Free()

	/* Initialize backing array contents. */
	s := buf.Slice()
	for i := uint(0); i < buf.Size; i++ {
		s[i] = 0
	}

	/* Create a slice by copying the backing array contents into Go memory. */
	b := buf.Bytes()

	zeroes := make([]byte, 32)
	if !bytes.Equal(b, zeroes) {
		t.Fatalf("Expected %v, got %v", zeroes, buf.Bytes())
	}

	/* Modifying returned slice does not modify the buffer. */
	for i := 0; i < len(b); i++ {
		b[i] = 42
	}

	/* Bytes() still returns zeroes. */
	if !bytes.Equal(buf.Bytes(), zeroes) {
		t.Fatalf("Expected %v, got %v", zeroes, buf.Bytes())
	}

	/* Creating a slice without copying the underlying buffer. */
	s = buf.Slice()
	if !bytes.Equal(s, zeroes) {
		t.Fatalf("Expected %v, got %v", zeroes, s)
	}

	/* Modifying the slice modifies the underlying buffer. */
	for i := 0; i < len(s); i++ {
		s[i] = 42
	}

	if !bytes.Equal(buf.Slice(), s) {
		t.Fatalf("Expected %v, got %v", s, buf.Slice())
	}

	/* Create another buffer from Go slice. */
	buf2 := FromBytes(zeroes)
	defer buf2.Free()

	if !bytes.Equal(buf2.Bytes(), zeroes) {
		t.Fatalf("Expected %v, got %v", zeroes, buf2.Bytes())
	}

	/* Crated a zeroed buffer. */
	buf3 := MakeAioBufferZero(uint(32))
	defer buf.Free()

	if !bytes.Equal(buf3.Bytes(), zeroes) {
		t.Fatalf("Expected %v, got %v", zeroes, buf2.Bytes())
	}
}

func TestAioBufferFree(t *testing.T) {
	buf := MakeAioBuffer(uint(32))

	/* Free the underlying C array. */
	buf.Free()

	/* And clear the pointer. */
	if buf.P != nil {
		t.Fatal("Dangling pointer after Free()")
	}

	/* Additional Free does nothing. */
	buf.Free()
}

func TestAioBufferBytesAfterFree(t *testing.T) {
	buf := MakeAioBuffer(uint(32))
	buf.Free()

	defer func() {
		if r := recover(); r == nil {
			t.Fatal("Did not recover from panic calling Bytes() after Free()")
		}
	}()

	buf.Bytes()
}

func TestAioBufferSliceAfterFree(t *testing.T) {
	buf := MakeAioBuffer(uint(32))
	buf.Free()

	defer func() {
		if r := recover(); r == nil {
			t.Fatal("Did not recover from panic calling Bytes() after Free()")
		}
	}()

	s := buf.Slice()
	s[0] = 42
}

func TestAioBufferGetAfterFree(t *testing.T) {
	buf := MakeAioBuffer(uint(32))
	buf.Free()

	defer func() {
		if r := recover(); r == nil {
			t.Fatal("Did not recover from panic calling Get() after Free()")
		}
	}()

	*buf.Get(0) = 42
}

// Typical buffer size.
const bufferSize uint = 256 * 1024

// Benchmark creating an uninitialized buffer.
func BenchmarkMakeAioBuffer(b *testing.B) {
	for i := 0; i < b.N; i++ {
		buf := MakeAioBuffer(bufferSize)
		buf.Free()
	}
}

// Benchmark creating zeroed buffer.
func BenchmarkMakeAioBufferZero(b *testing.B) {
	for i := 0; i < b.N; i++ {
		buf := MakeAioBufferZero(bufferSize)
		buf.Free()
	}
}

// Benchmark zeroing a buffer.
func BenchmarkAioBufferZero(b *testing.B) {
	for i := 0; i < b.N; i++ {
		buf := MakeAioBuffer(bufferSize)
		s := buf.Slice()
		for i := uint(0); i < bufferSize; i++ {
			s[i] = 0
		}
		buf.Free()
	}
}

// Benchmark creating a buffer by copying a Go slice.
func BenchmarkFromBytes(b *testing.B) {
	for i := 0; i < b.N; i++ {
		zeroes := make([]byte, bufferSize)
		buf := FromBytes(zeroes)
		buf.Free()
	}
}

// Benchmark creating a slice by copying the contents.
func BenchmarkAioBufferBytes(b *testing.B) {
	buf := MakeAioBuffer(bufferSize)
	defer buf.Free()
	var r int

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		r += len(buf.Bytes())
	}
}

// Benchmark creating a slice without copying the underlying buffer.
func BenchmarkAioBufferSlice(b *testing.B) {
	buf := MakeAioBuffer(bufferSize)
	defer buf.Free()
	var r int

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		r += len(buf.Slice())
	}
}

var data = make([]byte, bufferSize)

// Benchmark copying into same buffer, used as baseline for CopyMake and
// CopyMakeZero benchmarks.
func BenchmarkAioBufferCopyBaseline(b *testing.B) {
	buf := MakeAioBufferZero(bufferSize)
	defer buf.Free()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		copy(buf.Slice(), data)
	}
}

// Benchmark overhead of making a new buffer per read.
func BenchmarkAioBufferCopyMake(b *testing.B) {
	for i := 0; i < b.N; i++ {
		buf := MakeAioBuffer(bufferSize)
		copy(buf.Slice(), data)
		buf.Free()
	}
}

// Benchmark overhead of making a new zero buffer per read.
func BenchmarkAioBufferCopyMakeZero(b *testing.B) {
	for i := 0; i < b.N; i++ {
		buf := MakeAioBufferZero(bufferSize)
		copy(buf.Slice(), data)
		buf.Free()
	}
}
