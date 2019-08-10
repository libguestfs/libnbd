# libnbd Python bindings
# Copyright (C) 2010-2019 Red Hat Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

import select
import nbd

disk_size = 512 * 1024 * 1024
bs = 65536
max_reads_in_flight = 16
bytes_read = 0
bytes_written = 0

def asynch_copy (src, dst):
    size = src.get_size ()

    # This is our reading position in the source.
    soff = 0

    # This callback is called when any pread from the source
    # has completed.
    writes = []
    def read_completed (buf, offset, error):
        global bytes_read
        bytes_read += buf.size ()
        wr = (buf, offset)
        writes.append (wr)
        # By returning 1 here we auto-retire the pread command.
        return 1

    # This callback is called when any pwrite to the destination
    # has completed.
    def write_completed (buf, error):
        global bytes_written
        bytes_written += buf.size ()
        # By returning 1 here we auto-retire the pwrite command.
        return 1

    # The main loop which runs until we have finished reading and
    # there are no more commands in flight.
    while soff < size or dst.aio_in_flight () > 0:
        # If we're able to submit more reads from the source
        # then do so now.
        if soff < size and src.aio_in_flight () < max_reads_in_flight:
            bufsize = min (bs, size - soff)
            buf = nbd.Buffer (bufsize)
            # NB: Python lambdas are BROKEN.
            # https://stackoverflow.com/questions/2295290
            src.aio_pread (buf, soff,
                           lambda err, buf=buf, soff=soff:
                           read_completed (buf, soff, err))
            soff += bufsize

        # If there are any write commands waiting to be issued
        # to the destination, send them now.
        for buf, offset in writes:
            # See above link about broken Python lambdas.
            dst.aio_pwrite (buf, offset,
                            lambda err, buf=buf:
                            write_completed (buf, err))
        writes = []

        poll = select.poll ()

        sfd = src.aio_get_fd ()
        dfd = dst.aio_get_fd ()

        sevents = 0
        devents = 0
        if src.aio_get_direction () & nbd.AIO_DIRECTION_READ:
            sevents += select.POLLIN
        if src.aio_get_direction () & nbd.AIO_DIRECTION_WRITE:
            sevents += select.POLLOUT
        if dst.aio_get_direction () & nbd.AIO_DIRECTION_READ:
            devents += select.POLLIN
        if dst.aio_get_direction () & nbd.AIO_DIRECTION_WRITE:
            devents += select.POLLOUT
        poll.register (sfd, sevents)
        poll.register (dfd, devents)
        for (fd, revents) in poll.poll ():
            # The direction of each handle can change since we
            # slept in the select.
            if fd == sfd and revents & select.POLLIN and \
               src.aio_get_direction () & nbd.AIO_DIRECTION_READ:
                src.aio_notify_read ()
            elif fd == sfd and revents & select.POLLOUT and \
                 src.aio_get_direction () & nbd.AIO_DIRECTION_WRITE:
                src.aio_notify_write ()
            elif fd == dfd and revents & select.POLLIN and \
                 dst.aio_get_direction () & nbd.AIO_DIRECTION_READ:
                dst.aio_notify_read ()
            elif fd == dfd and revents & select.POLLOUT and \
                 dst.aio_get_direction () & nbd.AIO_DIRECTION_WRITE:
                dst.aio_notify_write ()

src = nbd.NBD ()
src.set_handle_name ("src")
dst = nbd.NBD ()
dst.set_handle_name ("dst")
src.connect_command (["nbdkit", "-s", "--exit-with-parent", "-r",
                      "pattern", "size=%d" % disk_size])
dst.connect_command (["nbdkit", "-s", "--exit-with-parent",
                      "memory", "size=%d" % disk_size])
asynch_copy (src, dst)

print ("bytes read: %d written: %d size: %d" %
       (bytes_read, bytes_written, disk_size))
assert bytes_read == disk_size
assert bytes_written == disk_size
