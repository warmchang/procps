/*
 * diskstat - Disk statistics - part of procps
 *
 * Copyright (C) 2003 Fabian Frederick
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <proc/diskstat.h>
#include "procps-private.h"

#define DISKSTAT_LINE_LEN 1024
#define DISKSTAT_NAME_LEN 15
#define DISKSTAT_FILE "/proc/diskstats"
#define SYSBLOCK_DIR "/sys/block"

struct procps_diskstat_dev {
    char name[DISKSTAT_NAME_LEN];
    int is_disk;
    unsigned long reads;
    unsigned long reads_merged;
    unsigned long read_sectors;
    unsigned long read_time;
    unsigned long writes;
    unsigned long writes_merged;
    unsigned long write_sectors;
    unsigned long write_time;
    unsigned long io_inprogress;
    unsigned long io_time;
    unsigned long io_wtime;
};

struct procps_diskstat {
    int refcount;
    FILE *diskstat_fp;
    struct procps_diskstat_dev *devs;
    int devs_alloc;
    int devs_used;
};

/*
 * scan_for_disks:
 *
 * All disks start off as partitions. This function
 * scans /sys/block and changes all devices found there
 * into disks. If /sys/block cannot have the directory
 * read, all devices are disks
 */
static int scan_for_disks(struct procps_diskstat *info)
{
    DIR *dirp;
    struct dirent *dent;
    int myerrno, i;

    if ((dirp = opendir(SYSBLOCK_DIR)) == NULL) {
        myerrno = errno;
        if (myerrno != ENOENT && myerrno != EACCES)
            return -myerrno;
        /* Cannot open the dir or perm denied, make all devs disks */
        for (i=0 ; i < info->devs_used; i++)
            info->devs[i].is_disk = 1;
        return 0;
    }

    while((dent = readdir(dirp)) != NULL) {
        for (i=0; i < info->devs_used; i++)
            if (strcmp(dent->d_name, info->devs[i].name) == 0) {
                info->devs[i].is_disk = 1;
                break;
            }
    }
    return 0;
}

static void diskstat_clear(struct procps_diskstat *info)
{
    if (info == NULL)
        return;
    if (info->devs != NULL && info->devs_alloc > 0) {
        memset(info->devs, 0, sizeof(struct procps_diskstat_dev)*info->devs_alloc);
        info->devs_used = 0;
    }
}

static int diskdata_alloc(
        struct procps_diskstat *info)
{
    struct procps_diskstat_dev *new_disks;
    int new_count;

    if (info == NULL)
        return -EINVAL;

    if (info->devs_used < info->devs_alloc)
        return 0;

    /* Increment the allocated number of slabs */
    new_count = info->devs_alloc * 5/4+30;

    if ((new_disks = realloc(info->devs,
                             sizeof(struct procps_diskstat_dev)*new_count)) == NULL)
        return -ENOMEM;
    info->devs = new_disks;
    info->devs_alloc = new_count;
    return 0;
}

static int get_disk(
        struct procps_diskstat *info,
        struct procps_diskstat_dev **node)
{
    int retval;

    if (!info)
        return -EINVAL;

    if (info->devs_used == info->devs_alloc) {
        if ((retval = diskdata_alloc(info)) < 0)
            return retval;
    }
    *node = &(info->devs[info->devs_used++]);
    return 0;
}

/*
 * procps_diskstat_new:
 *
 * Create a new container to hold the diskstat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: a new procps_diskstat container
 */
PROCPS_EXPORT int procps_diskstat_new (
        struct procps_diskstat **info)
{
    struct procps_diskstat *ds;
    ds = calloc(1, sizeof(struct procps_diskstat));
    if (!ds)
        return -ENOMEM;

    ds->refcount = 1;
    ds->diskstat_fp = NULL;
    *info = ds;
    return 0;
}

/*
 * procps_diskstat_read:
 *
 * @info: info structure created at procps_diskstat_new
 *
 * Read the data out of /proc/diskstats putting the information
 * into the supplied info structure
 *
 * Returns: 0 on success, negative on error
 */
PROCPS_EXPORT int procps_diskstat_read (
        struct procps_diskstat *info)
{
    int retval;
    char buf[DISKSTAT_LINE_LEN];
    char devname[DISKSTAT_NAME_LEN];
    struct procps_diskstat_dev *disk;

    /* clear/zero structures */
    diskstat_clear(info);

    if (NULL == info->diskstat_fp &&
        NULL == (info->diskstat_fp = fopen(DISKSTAT_FILE, "r")))
        return -errno;

    if (fseek(info->diskstat_fp, 0L, SEEK_SET) == -1)
        return -errno;

    while (fgets(buf, DISKSTAT_LINE_LEN, info->diskstat_fp)) {
        if (sscanf(buf,
                   "%*d %*d %" STRINGIFY(DISKSTAT_NAME_LEN) "s",
                   devname) < 1) {
            if (errno != 0)
                return -errno;
            return -EINVAL;
        }
        if ((retval = get_disk(info, &disk)) < 0)
                return retval;
        if (sscanf(buf, "   %*d    %*d %*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &disk->reads, &disk->reads_merged, &disk->read_sectors,
                   &disk->read_time,
                   &disk->writes, &disk->writes_merged, &disk->write_sectors,
                   &disk->write_time,
                   &disk->io_inprogress, &disk->io_time, &disk->io_wtime) != 11) {
            if (errno != 0)
                return -errno;
            return -EINVAL;
        }
        disk->is_disk = 0; /* default to no until we scan */
        strcpy(disk->name, devname);
    }
    if ((retval = scan_for_disks(info)) < 0)
        return retval;
    return 0;
}

/*
 * procps_diskstat_dev_count:
 *
 * @info: structure that has been read
 *
 * Returns: number of devices (disk+partitions)
 */
PROCPS_EXPORT int procps_diskstat_dev_count (
        const struct procps_diskstat *info)
{
    if (!info)
        return -EINVAL;
    return info->devs_used;
}


/*
 * procps_diskstat_dev_getbyname
 *
 * @info: diskstat data already read
 * @name: name of partition
 *
 * Find the device index by the name
 *
 * Returns: devid if found
 *  -1 if not found
 */
PROCPS_EXPORT int procps_diskstat_dev_getbyname(
        const struct procps_diskstat *info,
        const char *name)
{
    int i;

    if (info == NULL || info->devs_used == 0)
        return -1;
    for (i=0; i < info->devs_used; i++)
        if (strcmp(info->devs[i].name, name) == 0)
            return i;
    return -1;
}


PROCPS_EXPORT unsigned long procps_diskstat_dev_get(
        const struct procps_diskstat *info,
        const enum procps_diskstat_devitem item,
        const unsigned int devid)
{
    if (info == NULL)
        return 0;
    if (devid > info->devs_used)
        return 0;
    switch(item) {
    case PROCPS_DISKSTAT_READS:
        return info->devs[devid].reads;
    case PROCPS_DISKSTAT_READS_MERGED:
        return info->devs[devid].reads_merged;
    case PROCPS_DISKSTAT_READ_SECTORS:
        return info->devs[devid].read_sectors;
    case PROCPS_DISKSTAT_READ_TIME:
        return info->devs[devid].read_time;
    case PROCPS_DISKSTAT_WRITES:
        return info->devs[devid].writes;
    case PROCPS_DISKSTAT_WRITES_MERGED:
        return info->devs[devid].writes_merged;
    case PROCPS_DISKSTAT_WRITE_SECTORS:
        return info->devs[devid].write_sectors;
    case PROCPS_DISKSTAT_WRITE_TIME:
        return info->devs[devid].write_time;
    case PROCPS_DISKSTAT_IO_INPROGRESS:
        return info->devs[devid].io_inprogress;
    case PROCPS_DISKSTAT_IO_TIME:
        return info->devs[devid].io_time;
    case PROCPS_DISKSTAT_IO_WTIME:
        return info->devs[devid].io_wtime;
    default:
        return 0;
    }
}

PROCPS_EXPORT char* procps_diskstat_dev_getname(
        const struct procps_diskstat *info,
        const unsigned int devid)
{
    if (info == NULL)
        return 0;
    if (devid > info->devs_used)
        return 0;
    return info->devs[devid].name;
}

PROCPS_EXPORT int procps_diskstat_dev_isdisk(
        const struct procps_diskstat *info,
        const unsigned int devid)
{
    if (info == NULL || devid > info->devs_used)
        return -EINVAL;

    return info->devs[devid].is_disk;
}
