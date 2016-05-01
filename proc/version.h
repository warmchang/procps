/*
 * libprocps - Library to read proc filesystem
 *
 * Copyright (C) 1995 Martin Schulze <joey@infodrom.north.de>
 * Copyright (C) 1996 Charles Blake <cblake@bbn.com>
 * Copyright (C) 2003 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef PROC_VERSION_H
#define PROC_VERSION_H

#include <features.h>
__BEGIN_DECLS

int procps_linux_version(void);

/* Convenience macros for composing/decomposing version codes */
#define LINUX_VERSION(x,y,z)   (0x10000*((x)&0x7fff) + 0x100*((y)&0xff) + ((z)&0xff))
#define LINUX_VERSION_MAJOR(x) (((x)>>16) & 0xFF)
#define LINUX_VERSION_MINOR(x) (((x)>> 8) & 0xFF)
#define LINUX_VERSION_PATCH(x) ( (x)      & 0xFF)

__END_DECLS

#endif	/* PROC_VERSION_H */
