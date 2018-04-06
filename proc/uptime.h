/*
 * uptime - uptime related functions - part of procps
 *
 * Copyright (C) 1992-1998 Michael K. Johnson <johnsonm@redhat.com>
 * Copyright (C) ???? Larry Greenfield <greenfie@gauss.rutgers.edu>
 * Copyright (C) 1993 J. Cowley
 * Copyright (C) 1998-2003 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
 *
 * This library is free software; you can redistribute it and/or
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
#ifndef PROC_UPTIME_H
#define PROC_UPTIME_H

#ifdef __cplusplus
extern "C" {
#endif

int procps_uptime(double *uptime_secs, double *idle_secs);
char *procps_uptime_sprint(void);
char *procps_uptime_sprint_short(void);

#ifdef __cplusplus
}
#endif
#endif
