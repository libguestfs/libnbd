/* NBD client library in userspace.
 * Copyright (C) 2020-2021 Red Hat Inc.
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

#ifndef NBDINFO_H
#define NBDINFO_H

#include <stdio.h>
#include <stdbool.h>

#include <libnbd.h>

extern const char *progname;
extern struct nbd_handle *nbd;
extern FILE *fp;
extern bool list_all;
extern bool probe_content;
extern bool json_output;
extern const char *map;
extern bool size_only;

/* map.c */
extern void do_map (void);

/* size.c */
extern void do_size (void);

/* utils.c */
extern void print_json_string (const char *);

#endif /* NBDINFO_H */
