
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
    struct procps_jiffs cpu;
    unsigned int intr;
    unsigned int ctxt;
    unsigned int btime;
    unsigned int procs;
    unsigned int procs_blocked;
    unsigned int procs_running;
};

struct procps_jiffs_private {
    struct procps_jiffs_hist cpu;
    // future additions go here, please
};

struct procps_statinfo {
    int refcount;
    int stat_fd;
    struct stat_data data;
    int jiff_hists_alloc;
    int jiff_hists_inuse;
    struct procps_jiffs_private *jiff_hists;
    struct procps_jiffs_private cpu_summary;
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
/*  v->jiff_hists_alloc = 0;   unecessary with calloc  */
/*  v->jiff_hists_inuse = 0;   but serves as reminder  */
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
        return -EINVAL;

    memset(&(info->data), 0, sizeof(struct stat_data));
    /* read in the data */

    if (-1 == info->stat_fd && (info->stat_fd = open(STAT_FILE, O_RDONLY)) == -1) {
        return -errno;
    }
    if (lseek(info->stat_fd, 0L, SEEK_SET) == -1) {
        return -errno;
    }
    for (;;) {
        if ((size = read(info->stat_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        break;
    }
    if (size == 0)
        return 0;
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
                &(info->data.cpu.user),
                &(info->data.cpu.nice),
                &(info->data.cpu.system),
                &(info->data.cpu.idle),
                &(info->data.cpu.iowait),
                &(info->data.cpu.irq),
                &(info->data.cpu.sirq),
                &(info->data.cpu.stolen),
                &(info->data.cpu.guest),
                &(info->data.cpu.nice)
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
    if (info == NULL || info->refcount == 0)
        return NULL;
    info->refcount--;
    if (info->refcount > 0)
        return info;
    if (info->jiff_hists)
        free(info->jiff_hists);
    free(info);
    return NULL;
}

PROCPS_EXPORT jiff procps_stat_get_cpu (
        struct procps_statinfo *info,
        enum procps_cpu_item item)
{
    switch (item) {
        case PROCPS_CPU_USER:
            return info->data.cpu.user;
        case PROCPS_CPU_NICE:
            return info->data.cpu.nice;
        case PROCPS_CPU_SYSTEM:
            return info->data.cpu.system;
        case PROCPS_CPU_IDLE:
            return info->data.cpu.idle;
        case PROCPS_CPU_IOWAIT:
            return info->data.cpu.iowait;
        case PROCPS_CPU_IRQ:
            return info->data.cpu.irq;
        case PROCPS_CPU_SIRQ:
            return info->data.cpu.sirq;
        case PROCPS_CPU_STOLEN:
            return info->data.cpu.stolen;
        case PROCPS_CPU_GUEST:
            return info->data.cpu.guest;
        case PROCPS_CPU_GNICE:
            return info->data.cpu.gnice;
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
                item->result = info->data.cpu.user;
                break;
            case PROCPS_CPU_NICE:
                item->result = info->data.cpu.nice;
                break;
            case PROCPS_CPU_SYSTEM:
                item->result = info->data.cpu.system;
                break;
            case PROCPS_CPU_IDLE:
                item->result = info->data.cpu.idle;
                break;
            case PROCPS_CPU_IOWAIT:
                item->result = info->data.cpu.iowait;
                break;
            case PROCPS_CPU_IRQ:
                item->result = info->data.cpu.irq;
                break;
            case PROCPS_CPU_SIRQ:
                item->result = info->data.cpu.sirq;
                break;
            case PROCPS_CPU_STOLEN:
                item->result = info->data.cpu.stolen;
                break;
            case PROCPS_CPU_GUEST:
                item->result = info->data.cpu.guest;
                break;
            case PROCPS_CPU_GNICE:
                item->result = info->data.cpu.gnice;
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

/*
 * procps_stat_read_jiffs:
 *
 * Read the cpu data out of /proc/stat putting the information
 * into the dynamically acquired 'jiff_hists' hanging off the
 * info structure.  Along the way we gather historical stats and
 * and embed the cpu summary line in the info structure as well
 * ( since we had to read that first /proc/stat line anyway ).
 *
 * This is all private information but can be copied to caller
 * supplied structures upon request.
 */
PROCPS_EXPORT int procps_stat_read_jiffs (
        struct procps_statinfo *info)
{
  #define ALLOCincr 32
    struct procps_jiffs_private *sum_ptr, *cpu_ptr;
    char buf[8192], *bp;
    int i, rc, size;

    if (info == NULL)
        return -EINVAL;

    if (!info->jiff_hists_alloc) {
        info->jiff_hists = calloc(ALLOCincr, sizeof(struct procps_jiffs_private));
        if (!(info->jiff_hists))
            return -ENOMEM;
        info->jiff_hists_alloc = ALLOCincr;
        info->jiff_hists_inuse = 0;
    }

    if (-1 == info->stat_fd && (info->stat_fd = open(STAT_FILE, O_RDONLY)) == -1)
        return -errno;
    if (lseek(info->stat_fd, 0L, SEEK_SET) == -1)
        return -errno;
    for (;;) {
        if ((size = read(info->stat_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        break;
    }
    if (size == 0)
        return 0;
    buf[size] = '\0';
    bp = buf;

    sum_ptr = &info->cpu_summary;
    // remember from last time around
    memcpy(&sum_ptr->cpu.old, &sum_ptr->cpu.new, sizeof(struct procps_jiffs));
    // then value the summary line
    if (8 > sscanf(bp, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu"
        , &sum_ptr->cpu.new.user,  &sum_ptr->cpu.new.nice,   &sum_ptr->cpu.new.system
        , &sum_ptr->cpu.new.idle,  &sum_ptr->cpu.new.iowait, &sum_ptr->cpu.new.irq
        , &sum_ptr->cpu.new.sirq,  &sum_ptr->cpu.new.stolen
        , &sum_ptr->cpu.new.guest, &sum_ptr->cpu.new.gnice))
            return -1;
    sum_ptr->cpu.id = -1;              // mark as summary

    i = 0;
reap_em_again:
    cpu_ptr = info->jiff_hists + i;    // adapt to relocated if reap_em_again
    do {
        bp = 1 + strchr(bp, '\n');
        // remember from last time around
        memcpy(&cpu_ptr->cpu.old, &cpu_ptr->cpu.new, sizeof(struct procps_jiffs));
        if (8 > (rc = sscanf(bp, "cpu%d %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu"
            , &cpu_ptr->cpu.id
            , &cpu_ptr->cpu.new.user,  &cpu_ptr->cpu.new.nice,   &cpu_ptr->cpu.new.system
            , &cpu_ptr->cpu.new.idle,  &cpu_ptr->cpu.new.iowait, &cpu_ptr->cpu.new.irq
            , &cpu_ptr->cpu.new.sirq,  &cpu_ptr->cpu.new.stolen
            , &cpu_ptr->cpu.new.guest, &cpu_ptr->cpu.new.gnice))) {
                memmove(cpu_ptr, sum_ptr, sizeof(struct procps_jiffs_hist));
                break;                 // we must tolerate cpus taken offline
        }
        ++i;
        ++cpu_ptr;
    } while (i < info->jiff_hists_alloc);

    if (i == info->jiff_hists_alloc && rc >= 8) {
        info->jiff_hists_alloc += ALLOCincr;
        info->jiff_hists = realloc(info->jiff_hists, info->jiff_hists_alloc * sizeof(struct procps_jiffs_private));
        if (!(info->jiff_hists))
            return -ENOMEM;
        goto reap_em_again;
    }

    info->jiff_hists_inuse = i;
    return i;
  #undef ALLOCincr
}

/*
 * procps_stat_get_jiffs:
 *
 * Return the designated cpu data in the caller supplied structure.
 * A negative 'which' denotes the cpu_summary, not a real cpu.
 *
 * This function deals only with the 'current' jiffs counts.
 */
PROCPS_EXPORT int procps_stat_get_jiffs (
        struct procps_statinfo *info,
        struct procps_jiffs *item,
        int which)
{
    struct procps_jiffs_private *p;
    int i;

    if (info == NULL || item == NULL)
        return -EINVAL;
    if (which < 0) {
        // note, we're just copying the 'new' portion of our procps_jiffs_private
        memcpy(item, &info->cpu_summary, sizeof(struct procps_jiffs));
        return 0;
    }
    p = info->jiff_hists;
    for (i = 0; i < info->jiff_hists_inuse; i++) {
        if (p->cpu.id == which) {
            // note, we're just copying the 'new' portion of our procps_jiffs_private
            memcpy(item, p, sizeof(struct procps_jiffs));
            return 0;
        }
        ++p;
    }
    return -1;
}

/*
 * procps_stat_get_jiffs_all:
 *
 * Return all available cpu data in the caller supplied structures,
 * up to the lesser of numitems or total available.
 *
 * We tolerate a numitems greater than the total available, and
 * the caller had better tolerate fewer returned than requested.
 *
 * This function deals only with the 'current' jiffs counts.
 */
PROCPS_EXPORT int procps_stat_get_jiffs_all (
        struct procps_statinfo *info,
        struct procps_jiffs *item,
        int numitems)
{
    int i;

    if (info == NULL || item == NULL)
        return -EINVAL;
    if (!info->jiff_hists_inuse)
        return -1;
    for (i = 0; i < info->jiff_hists_inuse && i < numitems; i++) {
        // note, we're just copying the 'new' portion of our procps_jiffs_private
        memcpy(item + i, info->jiff_hists + i, sizeof(struct procps_jiffs));
    }
    return i;
}

/*
 * procps_stat_get_jiffs_hist:
 *
 * Return the designated cpu data in the caller supplied structure.
 * A negative 'which' denotes the cpu_summary, not a real cpu.
 *
 * This function provides both 'new' and 'old' jiffs counts.
 */
PROCPS_EXPORT int procps_stat_get_jiffs_hist (
        struct procps_statinfo *info,
        struct procps_jiffs_hist *item,
        int which)
{
    struct procps_jiffs_private *p;
    int i;

    if (info == NULL || item == NULL)
        return -EINVAL;
    if (which < 0) {
        memcpy(item, &info->cpu_summary, sizeof(struct procps_jiffs_hist));
        return 0;
    }
    p = info->jiff_hists;
    for (i = 0; i < info->jiff_hists_inuse; i++) {
        if (p->cpu.id == which) {
            memcpy(item, p, sizeof(struct procps_jiffs_hist));
            return 0;
        }
        ++p;
    }
    return -1;
}

/*
 * procps_stat_get_jiffs_hist_all:
 *
 * Return all available cpu data in the caller supplied structures,
 * up to the lesser of numitems or total available.
 *
 * We tolerate a numitems greater than the total available, and
 * the caller had better tolerate fewer returned than requested.
 *
 * This function provides both 'new' and 'old' jiffs counts.
 */
PROCPS_EXPORT int procps_stat_get_jiffs_hist_all (
        struct procps_statinfo *info,
        struct procps_jiffs_hist *item,
        int numitems)
{
    int i;

    if (info == NULL || item == NULL)
        return -EINVAL;
    if (!info->jiff_hists_inuse)
        return -1;
    for (i = 0; i < info->jiff_hists_inuse && i < numitems; i++) {
        memcpy(item + i, info->jiff_hists + i, sizeof(struct procps_jiffs_hist));
    }
    return i;
}
