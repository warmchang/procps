
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <proc/readstat.h>
#include "procps-private.h"

#define STAT_FILE "/proc/stat"

struct stat_data {
    jiff cpu_user;
    jiff cpu_nice;
    jiff cpu_sys;
    jiff cpu_idle;
    jiff cpu_iowait;
    jiff cpu_irq;
    jiff cpu_sirq;
    jiff cpu_stol;
    jiff cpu_guest;
    jiff cpu_gnice;
    unsigned int intr;
    unsigned int ctxt;
    unsigned int btime;
    unsigned int procs;
    unsigned int procs_blocked;
    unsigned int procs_running;
};

struct procps_statinfo {
    int refcount;
    int stat_fd;
    struct stat_data data;
};


/*
 * procps_stat_new:
 *
 * Create a new container to hold the stat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: a new stat info container
 */
PROCPS_EXPORT int procps_stat_new (
        struct procps_statinfo **info)
{
    struct procps_statinfo *v;
    v = calloc(1, sizeof(struct procps_statinfo));
    if (!v)
        return -ENOMEM;

    v->refcount = 1;
    v->stat_fd = -1;
    *info = v;
    return 0;
}

/*
 * procps_stat_read:
 *
 * Read the data out of /proc/stat putting the information
 * into the supplied info structure.
 *
 * If CPU stats only needed, set cpu_only to non-zero
 */
PROCPS_EXPORT int procps_stat_read (
        struct procps_statinfo *info,
        const int cpu_only)
{
    char buf[8192];
    char *head, *tail;
    int size;

    if (info == NULL)
        return -1;

    memset(&(info->data), 0, sizeof(struct stat_data));
    /* read in the data */

    if (-1 == info->stat_fd && (info->stat_fd = open(STAT_FILE, O_RDONLY)) == -1) {
        return -errno;
    }
    if (lseek(info->stat_fd, 0L, SEEK_SET) == -1) {
        return -errno;
    }
    if ((size = read(info->stat_fd, buf, sizeof(buf)-1)) < 0) {
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
        if (0 == strcmp(head, "cpu")) {
            if (sscanf(tail+1, "%Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
                &(info->data.cpu_user),
                &(info->data.cpu_nice),
                &(info->data.cpu_sys),
                &(info->data.cpu_idle),
                &(info->data.cpu_iowait),
                &(info->data.cpu_irq),
                &(info->data.cpu_sirq),
                &(info->data.cpu_stol),
                &(info->data.cpu_guest),
                &(info->data.cpu_nice)
                ) != 10)
                return -1;

            if (cpu_only)
                return 0; // we got what we need
        } else if (0 == strcmp(head, "intr")) {
            info->data.intr = strtoul(tail+1, &tail, 10);
        } else if (0 == strcmp(head, "ctxt")) {
            info->data.ctxt = strtoul(tail+1, &tail, 10);
        } else if (0 == strcmp(head, "btime")) {
            info->data.btime = strtoul(tail+1, &tail, 10);
        } else if (0 == strcmp(head, "processes")) {
            info->data.procs = strtoul(tail+1, &tail, 10);
        } else if (0 == strcmp(head, "procs_blocked")) {
            info->data.procs_blocked = strtoul(tail+1, &tail, 10);
        } else if (0 == strcmp(head, "procs_running")) {
            info->data.procs_running = strtoul(tail+1, &tail, 10);
        }
        if (tail[0] != '\n')
            tail = strchr(tail+1, '\n');
        if (!tail)
            break;
        head = tail + 1;
    } while (tail);
    if (info->data.procs)
        info->data.procs--; // exclude this process
    return 0;
}

PROCPS_EXPORT struct procps_statinfo *procps_stat_ref (
        struct procps_statinfo *info)
{
    if (info == NULL)
        return NULL;
    info->refcount++;
    return info;
}

PROCPS_EXPORT struct procps_statinfo *procps_stat_unref (
        struct procps_statinfo *info)
{
    if (info == NULL)
        return NULL;
    info->refcount--;
    if (info->refcount > 0)
        return NULL;
    free(info);
    return NULL;
}

PROCPS_EXPORT jiff procps_stat_get_cpu (
        struct procps_statinfo *info,
        enum procps_cpu_item item)
{
    switch (item) {
        case PROCPS_CPU_USER:
            return info->data.cpu_user;
        case PROCPS_CPU_NICE:
            return info->data.cpu_nice;
        case PROCPS_CPU_SYSTEM:
            return info->data.cpu_sys;
        case PROCPS_CPU_IDLE:
            return info->data.cpu_idle;
        case PROCPS_CPU_IOWAIT:
            return info->data.cpu_iowait;
        case PROCPS_CPU_IRQ:
            return info->data.cpu_irq;
        case PROCPS_CPU_SIRQ:
            return info->data.cpu_sirq;
        case PROCPS_CPU_STOLEN:
            return info->data.cpu_stol;
        case PROCPS_CPU_GUEST:
            return info->data.cpu_guest;
        case PROCPS_CPU_GNICE:
            return info->data.cpu_gnice;
    }
    return 0;
}

PROCPS_EXPORT int procps_get_cpu_chain (
        struct procps_statinfo *info,
        struct procps_cpu_result *item)
{
    if (item == NULL)
        return -EINVAL;

    do {
        switch (item->item) {
            case PROCPS_CPU_USER:
                item->result = info->data.cpu_user;
                break;
            case PROCPS_CPU_NICE:
                item->result = info->data.cpu_nice;
                break;
            case PROCPS_CPU_SYSTEM:
                item->result = info->data.cpu_sys;
                break;
            case PROCPS_CPU_IDLE:
                item->result = info->data.cpu_idle;
                break;
            case PROCPS_CPU_IOWAIT:
                item->result = info->data.cpu_iowait;
                break;
            case PROCPS_CPU_IRQ:
                item->result = info->data.cpu_irq;
                break;
            case PROCPS_CPU_SIRQ:
                item->result = info->data.cpu_sirq;
                break;
            case PROCPS_CPU_STOLEN:
                item->result = info->data.cpu_stol;
                break;
            case PROCPS_CPU_GUEST:
                item->result = info->data.cpu_guest;
                break;
            case PROCPS_CPU_GNICE:
                item->result = info->data.cpu_gnice;
                break;
            default:
                return -EINVAL;
        }
        item = item->next;
    } while (item);

    return 0;
}

PROCPS_EXPORT unsigned int procps_stat_get_sys (
        struct procps_statinfo *info,
        enum procps_stat_item item)
{
    switch (item) {
        case PROCPS_STAT_INTR:
            return info->data.intr;
        case PROCPS_STAT_CTXT:
            return info->data.ctxt;
        case PROCPS_STAT_BTIME:
            return info->data.btime;
        case PROCPS_STAT_PROCS:
            return info->data.procs;
        case PROCPS_STAT_PROCS_BLK:
            return info->data.procs_blocked;
        case PROCPS_STAT_PROCS_RUN:
            return info->data.procs_running;
    }
    return 0;
}

PROCPS_EXPORT int procps_stat_get_sys_chain (
        struct procps_statinfo *info,
        struct procps_sys_result *item)
{
    if (item == NULL)
        return -EINVAL;

    do {
        switch (item->item) {
            case PROCPS_STAT_INTR:
                item->result = info->data.intr;
                break;
            case PROCPS_STAT_CTXT:
                item->result = info->data.ctxt;
                break;
            case PROCPS_STAT_BTIME:
                item->result = info->data.btime;
                break;
            case PROCPS_STAT_PROCS:
                item->result = info->data.procs;
                break;
            case PROCPS_STAT_PROCS_BLK:
                item->result = info->data.procs_blocked;
                break;
            case PROCPS_STAT_PROCS_RUN:
                item->result = info->data.procs_running;
                break;
            default:
                return -EINVAL;
        }
        item = item->next;
    } while (item);

    return 0;
}
