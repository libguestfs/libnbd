# NBD client library in userspace
# WARNING: THIS FILE IS GENERATED FROM
# generator/generator
# ANY CHANGES YOU MAKE TO THIS FILE WILL BE LOST.
#
# Copyright (C) 2013-2019 Red Hat Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

generator_built += \
	nbd_set_debug.pod \
	nbd_get_debug.pod \
	nbd_set_debug_callback.pod \
	nbd_clear_debug_callback.pod \
	nbd_set_handle_name.pod \
	nbd_get_handle_name.pod \
	nbd_set_export_name.pod \
	nbd_get_export_name.pod \
	nbd_set_tls.pod \
	nbd_get_tls.pod \
	nbd_set_tls_certificates.pod \
	nbd_set_tls_verify_peer.pod \
	nbd_get_tls_verify_peer.pod \
	nbd_set_tls_username.pod \
	nbd_get_tls_username.pod \
	nbd_set_tls_psk_file.pod \
	nbd_add_meta_context.pod \
	nbd_connect_uri.pod \
	nbd_connect_unix.pod \
	nbd_connect_tcp.pod \
	nbd_connect_command.pod \
	nbd_is_read_only.pod \
	nbd_can_flush.pod \
	nbd_can_fua.pod \
	nbd_is_rotational.pod \
	nbd_can_trim.pod \
	nbd_can_zero.pod \
	nbd_can_df.pod \
	nbd_can_multi_conn.pod \
	nbd_can_cache.pod \
	nbd_can_meta_context.pod \
	nbd_get_size.pod \
	nbd_pread.pod \
	nbd_pread_structured.pod \
	nbd_pwrite.pod \
	nbd_shutdown.pod \
	nbd_flush.pod \
	nbd_trim.pod \
	nbd_cache.pod \
	nbd_zero.pod \
	nbd_block_status.pod \
	nbd_poll.pod \
	nbd_aio_connect.pod \
	nbd_aio_connect_uri.pod \
	nbd_aio_connect_unix.pod \
	nbd_aio_connect_tcp.pod \
	nbd_aio_connect_command.pod \
	nbd_aio_pread.pod \
	nbd_aio_pread_structured.pod \
	nbd_aio_pwrite.pod \
	nbd_aio_disconnect.pod \
	nbd_aio_flush.pod \
	nbd_aio_trim.pod \
	nbd_aio_cache.pod \
	nbd_aio_zero.pod \
	nbd_aio_block_status.pod \
	nbd_aio_get_fd.pod \
	nbd_aio_get_direction.pod \
	nbd_aio_notify_read.pod \
	nbd_aio_notify_write.pod \
	nbd_aio_is_created.pod \
	nbd_aio_is_connecting.pod \
	nbd_aio_is_ready.pod \
	nbd_aio_is_processing.pod \
	nbd_aio_is_dead.pod \
	nbd_aio_is_closed.pod \
	nbd_aio_command_completed.pod \
	nbd_aio_peek_command_completed.pod \
	nbd_aio_in_flight.pod \
	nbd_connection_state.pod \
	nbd_get_package_name.pod \
	nbd_get_version.pod \
	nbd_kill_command.pod \
	nbd_supports_tls.pod \
	nbd_supports_uri.pod \
	$(NULL)

CLEANFILES += \
	nbd_set_debug.3 \
	nbd_get_debug.3 \
	nbd_set_debug_callback.3 \
	nbd_clear_debug_callback.3 \
	nbd_set_handle_name.3 \
	nbd_get_handle_name.3 \
	nbd_set_export_name.3 \
	nbd_get_export_name.3 \
	nbd_set_tls.3 \
	nbd_get_tls.3 \
	nbd_set_tls_certificates.3 \
	nbd_set_tls_verify_peer.3 \
	nbd_get_tls_verify_peer.3 \
	nbd_set_tls_username.3 \
	nbd_get_tls_username.3 \
	nbd_set_tls_psk_file.3 \
	nbd_add_meta_context.3 \
	nbd_connect_uri.3 \
	nbd_connect_unix.3 \
	nbd_connect_tcp.3 \
	nbd_connect_command.3 \
	nbd_is_read_only.3 \
	nbd_can_flush.3 \
	nbd_can_fua.3 \
	nbd_is_rotational.3 \
	nbd_can_trim.3 \
	nbd_can_zero.3 \
	nbd_can_df.3 \
	nbd_can_multi_conn.3 \
	nbd_can_cache.3 \
	nbd_can_meta_context.3 \
	nbd_get_size.3 \
	nbd_pread.3 \
	nbd_pread_structured.3 \
	nbd_pwrite.3 \
	nbd_shutdown.3 \
	nbd_flush.3 \
	nbd_trim.3 \
	nbd_cache.3 \
	nbd_zero.3 \
	nbd_block_status.3 \
	nbd_poll.3 \
	nbd_aio_connect.3 \
	nbd_aio_connect_uri.3 \
	nbd_aio_connect_unix.3 \
	nbd_aio_connect_tcp.3 \
	nbd_aio_connect_command.3 \
	nbd_aio_pread.3 \
	nbd_aio_pread_structured.3 \
	nbd_aio_pwrite.3 \
	nbd_aio_disconnect.3 \
	nbd_aio_flush.3 \
	nbd_aio_trim.3 \
	nbd_aio_cache.3 \
	nbd_aio_zero.3 \
	nbd_aio_block_status.3 \
	nbd_aio_get_fd.3 \
	nbd_aio_get_direction.3 \
	nbd_aio_notify_read.3 \
	nbd_aio_notify_write.3 \
	nbd_aio_is_created.3 \
	nbd_aio_is_connecting.3 \
	nbd_aio_is_ready.3 \
	nbd_aio_is_processing.3 \
	nbd_aio_is_dead.3 \
	nbd_aio_is_closed.3 \
	nbd_aio_command_completed.3 \
	nbd_aio_peek_command_completed.3 \
	nbd_aio_in_flight.3 \
	nbd_connection_state.3 \
	nbd_get_package_name.3 \
	nbd_get_version.3 \
	nbd_kill_command.3 \
	nbd_supports_tls.3 \
	nbd_supports_uri.3 \
	$(NULL)

if HAVE_POD

man_MANS += \
	nbd_set_debug.3 \
	nbd_get_debug.3 \
	nbd_set_debug_callback.3 \
	nbd_clear_debug_callback.3 \
	nbd_set_handle_name.3 \
	nbd_get_handle_name.3 \
	nbd_set_export_name.3 \
	nbd_get_export_name.3 \
	nbd_set_tls.3 \
	nbd_get_tls.3 \
	nbd_set_tls_certificates.3 \
	nbd_set_tls_verify_peer.3 \
	nbd_get_tls_verify_peer.3 \
	nbd_set_tls_username.3 \
	nbd_get_tls_username.3 \
	nbd_set_tls_psk_file.3 \
	nbd_add_meta_context.3 \
	nbd_connect_uri.3 \
	nbd_connect_unix.3 \
	nbd_connect_tcp.3 \
	nbd_connect_command.3 \
	nbd_is_read_only.3 \
	nbd_can_flush.3 \
	nbd_can_fua.3 \
	nbd_is_rotational.3 \
	nbd_can_trim.3 \
	nbd_can_zero.3 \
	nbd_can_df.3 \
	nbd_can_multi_conn.3 \
	nbd_can_cache.3 \
	nbd_can_meta_context.3 \
	nbd_get_size.3 \
	nbd_pread.3 \
	nbd_pread_structured.3 \
	nbd_pwrite.3 \
	nbd_shutdown.3 \
	nbd_flush.3 \
	nbd_trim.3 \
	nbd_cache.3 \
	nbd_zero.3 \
	nbd_block_status.3 \
	nbd_poll.3 \
	nbd_aio_connect.3 \
	nbd_aio_connect_uri.3 \
	nbd_aio_connect_unix.3 \
	nbd_aio_connect_tcp.3 \
	nbd_aio_connect_command.3 \
	nbd_aio_pread.3 \
	nbd_aio_pread_structured.3 \
	nbd_aio_pwrite.3 \
	nbd_aio_disconnect.3 \
	nbd_aio_flush.3 \
	nbd_aio_trim.3 \
	nbd_aio_cache.3 \
	nbd_aio_zero.3 \
	nbd_aio_block_status.3 \
	nbd_aio_get_fd.3 \
	nbd_aio_get_direction.3 \
	nbd_aio_notify_read.3 \
	nbd_aio_notify_write.3 \
	nbd_aio_is_created.3 \
	nbd_aio_is_connecting.3 \
	nbd_aio_is_ready.3 \
	nbd_aio_is_processing.3 \
	nbd_aio_is_dead.3 \
	nbd_aio_is_closed.3 \
	nbd_aio_command_completed.3 \
	nbd_aio_peek_command_completed.3 \
	nbd_aio_in_flight.3 \
	nbd_connection_state.3 \
	nbd_get_package_name.3 \
	nbd_get_version.3 \
	nbd_kill_command.3 \
	nbd_supports_tls.3 \
	nbd_supports_uri.3 \
	$(NULL)

nbd_set_debug.3: nbd_set_debug.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_debug.3: nbd_get_debug.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_debug_callback.3: nbd_set_debug_callback.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_clear_debug_callback.3: nbd_clear_debug_callback.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_handle_name.3: nbd_set_handle_name.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_handle_name.3: nbd_get_handle_name.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_export_name.3: nbd_set_export_name.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_export_name.3: nbd_get_export_name.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_tls.3: nbd_set_tls.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_tls.3: nbd_get_tls.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_tls_certificates.3: nbd_set_tls_certificates.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_tls_verify_peer.3: nbd_set_tls_verify_peer.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_tls_verify_peer.3: nbd_get_tls_verify_peer.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_tls_username.3: nbd_set_tls_username.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_tls_username.3: nbd_get_tls_username.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_set_tls_psk_file.3: nbd_set_tls_psk_file.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_add_meta_context.3: nbd_add_meta_context.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_connect_uri.3: nbd_connect_uri.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_connect_unix.3: nbd_connect_unix.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_connect_tcp.3: nbd_connect_tcp.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_connect_command.3: nbd_connect_command.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_is_read_only.3: nbd_is_read_only.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_flush.3: nbd_can_flush.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_fua.3: nbd_can_fua.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_is_rotational.3: nbd_is_rotational.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_trim.3: nbd_can_trim.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_zero.3: nbd_can_zero.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_df.3: nbd_can_df.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_multi_conn.3: nbd_can_multi_conn.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_cache.3: nbd_can_cache.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_can_meta_context.3: nbd_can_meta_context.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_size.3: nbd_get_size.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_pread.3: nbd_pread.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_pread_structured.3: nbd_pread_structured.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_pwrite.3: nbd_pwrite.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_shutdown.3: nbd_shutdown.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_flush.3: nbd_flush.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_trim.3: nbd_trim.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_cache.3: nbd_cache.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_zero.3: nbd_zero.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_block_status.3: nbd_block_status.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_poll.3: nbd_poll.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_connect.3: nbd_aio_connect.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_connect_uri.3: nbd_aio_connect_uri.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_connect_unix.3: nbd_aio_connect_unix.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_connect_tcp.3: nbd_aio_connect_tcp.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_connect_command.3: nbd_aio_connect_command.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_pread.3: nbd_aio_pread.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_pread_structured.3: nbd_aio_pread_structured.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_pwrite.3: nbd_aio_pwrite.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_disconnect.3: nbd_aio_disconnect.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_flush.3: nbd_aio_flush.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_trim.3: nbd_aio_trim.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_cache.3: nbd_aio_cache.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_zero.3: nbd_aio_zero.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_block_status.3: nbd_aio_block_status.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_get_fd.3: nbd_aio_get_fd.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_get_direction.3: nbd_aio_get_direction.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_notify_read.3: nbd_aio_notify_read.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_notify_write.3: nbd_aio_notify_write.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_is_created.3: nbd_aio_is_created.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_is_connecting.3: nbd_aio_is_connecting.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_is_ready.3: nbd_aio_is_ready.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_is_processing.3: nbd_aio_is_processing.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_is_dead.3: nbd_aio_is_dead.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_is_closed.3: nbd_aio_is_closed.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_command_completed.3: nbd_aio_command_completed.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_peek_command_completed.3: nbd_aio_peek_command_completed.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_aio_in_flight.3: nbd_aio_in_flight.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_connection_state.3: nbd_connection_state.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_package_name.3: nbd_get_package_name.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_get_version.3: nbd_get_version.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_kill_command.3: nbd_kill_command.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_supports_tls.3: nbd_supports_tls.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

nbd_supports_uri.3: nbd_supports_uri.pod $(top_builddir)/podwrapper.pl
	$(PODWRAPPER) --allow-long-lines --section=3 --man $@ \
	    --html $(top_builddir)/html/$@.html \
	    $<

endif HAVE_POD
