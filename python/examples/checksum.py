#!/usr/bin/python3

# This example can be used to compute the checksum of each
# block in a disk image.  eg:
#
# $ ./checksum.py disk.qcow2
# block @ 0 (size 1073741824) sha1 587b1dec5c55d2a2845e18c08bd135135396eb52
#
# The disk image can be in any format that is supported by qemu-nbd.
# You can change the block size and checksum (hash) type using command
# line parameters.

import argparse
import hashlib
import nbd

p = argparse.ArgumentParser(
    description="Compute checksums of blocks in a disk image")
p.add_argument('filename', metavar='FILENAME',
               help='disk image')
p.add_argument('--format', dest='format',
               help='disk image format, eg. qcow2 or raw')
p.add_argument('--blocksize', dest='blocksize', type=int,
               default=1024*1024*1024,
               help='block size')
p.add_argument('--hash', dest='hash', default='sha1',
               help='hash to use, eg. sha1, sha256, etc.')
arg = p.parse_args()

max_read = 32*1024*1024

h = nbd.NBD()

# Set up the qemu-nbd command line.
cmd = ["qemu-nbd", "--read-only", "--persistent", arg.filename]
if arg.format is not None:
    cmd += ["-f", arg.format]

h.connect_systemd_socket_activation(cmd)

size = h.get_size()
offset = 0

while offset < size:
    hh = hashlib.new(arg.hash)
    start = offset
    got = 0
    while offset < size and got < arg.blocksize:
        c = min(max_read, arg.blocksize - got)
        c = min(c, size-offset)
        hh.update(h.pread(c, offset))
        got += c
        offset += c
    print("block @ %d (size %d)" % (start, got), end=" ")
    print("%s %s" % (arg.hash, hh.hexdigest()))

h.shutdown()
