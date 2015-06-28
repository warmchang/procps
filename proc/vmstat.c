
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <proc/vmstat.h>
#include "procps-private.h"

#define VMSTAT_FILE "/proc/vmstat"
#define ROW_NAME_LEN 32

struct vmstat_data {
    unsigned long pgpgin;
    unsigned long pgpgout;
    unsigned long pswpin;
    unsigned long pswpout;
};

struct mem_table_struct {
    const char *name;
    unsigned long *slot;
};

struct procps_vmstat {
    int refcount;
    int vmstat_fd;
    struct vmstat_data data;
};

/*
 * procps_vmstat_new:
 *
 * Create a new container to hold the vmstat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: a new procps_vmstat container
 */
PROCPS_EXPORT int procps_vmstat_new (
        struct procps_vmstat **info)
{
    struct procps_vmstat *v;
    v = calloc(1, sizeof(struct procps_vmstat));
    if (!v)
        return -ENOMEM;

    v->refcount = 1;
    v->vmstat_fd = -1;
    *info = v;
    return 0;
}

/*
 * procps_vmstat_read:
 *
 * Read the data out of /proc/vmstat putting the information
 * into the supplied info structure
 *
 * Returns: 0 on success, negative on error
 */
PROCPS_EXPORT int procps_vmstat_read (
        struct procps_vmstat *info)
{
    char buf[8192];
    char *head, *tail;
    int size;
    unsigned long *valptr;

    if (info == NULL)
        return -1;

    memset(&(info->data), 0, sizeof(struct vmstat_data));
    /* read in the data */

    if (-1 == info->vmstat_fd && (info->vmstat_fd = open(VMSTAT_FILE, O_RDONLY)) == -1) {
        return -errno;
    }
    if (lseek(info->vmstat_fd, 0L, SEEK_SET) == -1) {
        return -errno;
    }
    if ((size = read(info->vmstat_fd, buf, sizeof(buf)-1)) < 0) {
        return -1;
    }
    buf[size] = '\0';

    /* Scan the file */
    head = buf;
    do {
        tail = strchr(head, ' ');
        if (!tail)
            break;
        *tail = '\0';
        valptr = NULL;
        if (0 == strcmp(head, "pgpgin")) {
            valptr = &(info->data.pgpgin);
        }else if (0 == strcmp(head, "pgpgout")) {
            valptr = &(info->data.pgpgout);
        }else if (0 == strcmp(head, "pswpin")) {
            valptr = &(info->data.pswpin);
        }else if (0 == strcmp(head, "pswpout")) {
            valptr = &(info->data.pswpout);
        }
        head = tail+1;
        if (valptr) {
            *valptr = strtoul(head, &tail, 10);
        }

        tail = strchr(head, '\n');
        if (!tail)
            break;
        head = tail + 1;
    } while(tail);
    return 0;
}

PROCPS_EXPORT struct procps_vmstat *procps_vmstat_ref (
        struct procps_vmstat *info)
{
    if (info == NULL)
        return NULL;
    info->refcount++;
    return info;
}

PROCPS_EXPORT struct procps_vmstat *procps_vmstat_unref (
        struct procps_vmstat *info)
{
    if (info == NULL)
        return NULL;
    info->refcount--;
    if (info->refcount > 0)
        return NULL;
    free(info);
    return NULL;
}

/* Accessor functions */
PROCPS_EXPORT unsigned long procps_vmstat_get (
        struct procps_vmstat *info,
        enum vmstat_item item)
{
    switch (item) {
        case PROCPS_VMSTAT_PGPGIN:
            return info->data.pgpgin;
        case PROCPS_VMSTAT_PGPGOUT:
            return info->data.pgpgout;
        case PROCPS_VMSTAT_PSWPIN:
            return info->data.pswpin;
        case PROCPS_VMSTAT_PSWPOUT:
            return info->data.pswpout;
    }
    return 0;
}

