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

api_built += \
	nbd_set_debug \
	nbd_get_debug \
	nbd_set_debug_callback \
	nbd_clear_debug_callback \
	nbd_set_handle_name \
	nbd_get_handle_name \
	nbd_set_export_name \
	nbd_get_export_name \
	nbd_set_tls \
	nbd_get_tls \
	nbd_set_tls_certificates \
	nbd_set_tls_verify_peer \
	nbd_get_tls_verify_peer \
	nbd_set_tls_username \
	nbd_get_tls_username \
	nbd_set_tls_psk_file \
	nbd_add_meta_context \
	nbd_connect_uri \
	nbd_connect_unix \
	nbd_connect_tcp \
	nbd_connect_command \
	nbd_is_read_only \
	nbd_can_flush \
	nbd_can_fua \
	nbd_is_rotational \
	nbd_can_trim \
	nbd_can_zero \
	nbd_can_df \
	nbd_can_multi_conn \
	nbd_can_cache \
	nbd_can_meta_context \
	nbd_get_size \
	nbd_pread \
	nbd_pread_structured \
	nbd_pwrite \
	nbd_shutdown \
	nbd_flush \
	nbd_trim \
	nbd_cache \
	nbd_zero \
	nbd_block_status \
	nbd_poll \
	nbd_aio_connect \
	nbd_aio_connect_uri \
	nbd_aio_connect_unix \
	nbd_aio_connect_tcp \
	nbd_aio_connect_command \
	nbd_aio_pread \
	nbd_aio_pread_structured \
	nbd_aio_pwrite \
	nbd_aio_disconnect \
	nbd_aio_flush \
	nbd_aio_trim \
	nbd_aio_cache \
	nbd_aio_zero \
	nbd_aio_block_status \
	nbd_aio_get_fd \
	nbd_aio_get_direction \
	nbd_aio_notify_read \
	nbd_aio_notify_write \
	nbd_aio_is_created \
	nbd_aio_is_connecting \
	nbd_aio_is_ready \
	nbd_aio_is_processing \
	nbd_aio_is_dead \
	nbd_aio_is_closed \
	nbd_aio_command_completed \
	nbd_aio_peek_command_completed \
	nbd_aio_in_flight \
	nbd_connection_state \
	nbd_get_package_name \
	nbd_get_version \
	nbd_kill_command \
	nbd_supports_tls \
	nbd_supports_uri \
	$(NULL)
