/*
 * libprocps - Library to read proc filesystem
 *
 * Copyright (C) 2016 Jim Warner <james.warner@comcast.net>
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

#ifndef XTRA_PROCPS_DEBUG_H
#define XTRA_PROCPS_DEBUG_H

#include <proc/procps-private.h>


// --- DISKSTATS ------------------------------------------
#ifdef DISKSTATS_GET
#undef DISKSTATS_GET
struct diskstats_result *xtra_diskstats_get (
    struct diskstats_info *info,
    const char *name,
    enum diskstats_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

#define DISKSTATS_GET( info, name, actual_enum, type ) ( { \
    struct diskstats_result *r; \
    r = xtra_diskstats_get(info, name, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .

#ifdef DISKSTATS_VAL
#undef DISKSTATS_VAL
void xtra_diskstats_val (
    int relative_enum,
    const char *typestr,
    const struct diskstats_stack *stack,
    struct diskstats_info *info,
    const char *file,
    int lineno);

#define DISKSTATS_VAL( relative_enum, type, stack, info ) ( { \
    xtra_diskstats_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    stack -> head [ relative_enum ] . result . type; } )
#endif // . . . . . . . . . .


// --- MEMINFO --------------------------------------------
#ifdef MEMINFO_GET
#undef MEMINFO_GET
struct meminfo_result *xtra_meminfo_get (
    struct meminfo_info *info,
    enum meminfo_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

#define MEMINFO_GET( info, actual_enum, type ) ( { \
    struct meminfo_result *r; \
    r = xtra_meminfo_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .

#ifdef MEMINFO_VAL
#undef MEMINFO_VAL
void xtra_meminfo_val (
    int relative_enum,
    const char *typestr,
    const struct meminfo_stack *stack,
    struct meminfo_info *info,
    const char *file,
    int lineno);

#define MEMINFO_VAL( relative_enum, type, stack, info ) ( { \
    xtra_meminfo_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    stack -> head [ relative_enum ] . result . type; } )
#endif // . . . . . . . . . .


// --- PIDS -----------------------------------------------
#ifdef PIDS_VAL
#undef PIDS_VAL
void xtra_pids_val (
    int relative_enum,
    const char *typestr,
    const struct pids_stack *stack,
    struct pids_info *info,
    const char *file,
    int lineno);

#define PIDS_VAL( relative_enum, type, stack, info ) ( { \
    xtra_pids_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    stack -> head [ relative_enum ] . result . type; } )
#endif // . . . . . . . . . .


// --- SLABINFO -------------------------------------------
#ifdef SLABINFO_GET
#undef SLABINFO_GET
struct slabinfo_result *xtra_slabinfo_get (
    struct slabinfo_info *info,
    enum slabinfo_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

#define SLABINFO_GET( info, actual_enum, type ) ( { \
    struct slabinfo_result *r; \
    r = xtra_slabinfo_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .

#ifdef SLABINFO_VAL
#undef SLABINFO_VAL
void xtra_slabinfo_val (
    int relative_enum,
    const char *typestr,
    const struct slabinfo_stack *stack,
    struct slabinfo_info *info,
    const char *file,
    int lineno);

#define SLABINFO_VAL( relative_enum, type, stack, info ) ( { \
    xtra_slabinfo_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    stack -> head [ relative_enum ] . result . type; } )
#endif // . . . . . . . . . .


// --- STAT -----------------------------------------------
#ifdef STAT_GET
#undef STAT_GET
struct stat_result *xtra_stat_get (
    struct stat_info *info,
    enum stat_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

#define STAT_GET( info, actual_enum, type ) ( { \
    struct stat_result *r; \
    r = xtra_stat_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .

#ifdef STAT_VAL
#undef STAT_VAL
void xtra_stat_val (
    int relative_enum,
    const char *typestr,
    const struct stat_stack *stack,
    struct stat_info *info,
    const char *file,
    int lineno);

#define STAT_VAL( relative_enum, type, stack, info ) ( { \
    xtra_stat_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    stack -> head [ relative_enum ] . result . type; } )
#endif // . . . . . . . . . .


// --- VMSTAT ---------------------------------------------
#ifdef VMSTAT_GET
#undef VMSTAT_GET
struct vmstat_result *xtra_vmstat_get (
    struct vmstat_info *info,
    enum vmstat_item actual_enum,
    const char *typestr,
    const char *file,
    int lineno);

#define VMSTAT_GET( info, actual_enum, type ) ( { \
    struct vmstat_result *r; \
    r = xtra_vmstat_get(info, actual_enum , STRINGIFY(type), __FILE__, __LINE__); \
    r ? r->result . type : 0; } )
#endif // . . . . . . . . . .

#ifdef VMSTAT_VAL
#undef VMSTAT_VAL
void xtra_vmstat_val (
    int relative_enum,
    const char *typestr,
    const struct vmstat_stack *stack,
    struct vmstat_info *info,
    const char *file,
    int lineno);

#define VMSTAT_VAL( relative_enum, type, stack, info ) ( { \
    xtra_vmstat_val(relative_enum, STRINGIFY(type), stack, info, __FILE__, __LINE__); \
    stack -> head [ relative_enum ] . result . type; } )
#endif // . . . . . . . . . .

#endif // end: XTRA_PROCPS_DEBUG_H
