/*
 * libprocps - Library to read proc filesystem
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
#ifndef PROCPS_PROC_PROCPS_H
#define PROCPS_PROC_PROCPS_H

/* includes that show public exports go here */
#include <proc/diskstat.h>
#include <proc/meminfo.h>
#include <proc/namespace.h>
#include <proc/pids.h>
#include <proc/slab.h>
#include <proc/stat.h>
#include <proc/sysinfo.h>
#include <proc/version.h>
#include <proc/vmstat.h>
#include <proc/uptime.h>

// FIXME: only public function in escape.c
#define ESC_STRETCH 1  // since we mangle to '?' this is 1 (would be 4 for octal escapes)
int escape_str(char *__restrict dst, const char *__restrict src, int bufsize, int *maxcells);

#endif
