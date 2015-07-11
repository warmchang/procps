/*
 * old: "Copyright 1994 by Henry Ware <al172@yfn.ysu.edu>. Copyleft same year."
 * most code copyright 2002 Albert Cahalan
 *
 * 27/05/2003 (Fabian Frederick) : Add unit conversion + interface
 *				   Export proc/stat access to libproc
 *				   Adapt vmstat helpfile
 * 31/05/2003 (Fabian) : Add diskstat support (/libproc)
 * June 2003 (Fabian)  : -S <x> -s & -s -S <x> patch
 * June 2003 (Fabian)  : Adding diskstat against 3.1.9, slabinfo
 *			 patching 'header' in disk & slab
 * July 2003 (Fabian)  : Adding disk partition output
 *			 Adding disk table
 *			 Syncing help / usage
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

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"
#include "proc/sysinfo.h"
#include <proc/vmstat.h>
#include <proc/readstat.h>
#include <proc/meminfo.h>
#include <proc/diskstat.h>
#include <proc/slab.h>

#define UNIT_B        1
#define UNIT_k        1000
#define UNIT_K        1024
#define UNIT_m        1000000
#define UNIT_M        1048576

static unsigned long dataUnit = UNIT_K;
static char szDataUnit[3] = "K";

#define VMSTAT        0
#define DISKSTAT      0x00000001
#define VMSUMSTAT     0x00000002
#define SLABSTAT      0x00000004
#define PARTITIONSTAT 0x00000008
#define DISKSUMSTAT   0x00000010

static int statMode = VMSTAT;

/* "-a" means "show active/inactive" */
static int a_option;

/* "-w" means "wide output" */
static int w_option;

/* "-t" means "show timestamp" */
static int t_option;

static unsigned sleep_time = 1;
static int infinite_updates = 0;
static unsigned long num_updates =1;
/* window height */
static unsigned int height;
static unsigned int moreheaders = TRUE;

static void __attribute__ ((__noreturn__))
    usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
	      _(" %s [options] [delay [count]]\n"),
		program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -a, --active           active/inactive memory\n"), out);
	fputs(_(" -f, --forks            number of forks since boot\n"), out);
	fputs(_(" -m, --slabs            slabinfo\n"), out);
	fputs(_(" -n, --one-header       do not redisplay header\n"), out);
	fputs(_(" -s, --stats            event counter statistics\n"), out);
	fputs(_(" -d, --disk             disk statistics\n"), out);
	fputs(_(" -D, --disk-sum         summarize disk statistics\n"), out);
	fputs(_(" -p, --partition <dev>  partition specific statistics\n"), out);
	fputs(_(" -S, --unit <char>      define display unit\n"), out);
	fputs(_(" -w, --wide             wide output\n"), out);
	fputs(_(" -t, --timestamp        show timestamp\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("vmstat(8)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void new_header(void)
{
	struct tm *tm_ptr;
	time_t the_time;
	char timebuf[32];

	/* Translation Hint: Translating folloging header & fields
	 * that follow (marked with max x chars) might not work,
	 * unless manual page is translated as well.  */
	const char *header =
	    _("procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----");
	const char *wide_header =
	    _("procs -----------------------memory---------------------- ---swap-- -----io---- -system-- --------cpu--------");
	const char *timestamp_header = _(" -----timestamp-----");

	const char format[] =
	    "%2s %2s %6s %6s %6s %6s %4s %4s %5s %5s %4s %4s %2s %2s %2s %2s %2s";
	const char wide_format[] =
	    "%2s %2s %12s %12s %12s %12s %4s %4s %5s %5s %4s %4s %3s %3s %3s %3s %3s";


	printf("%s", w_option ? wide_header : header);

	if (t_option) {
		printf("%s", timestamp_header);
	}

	printf("\n");

	printf(
	    w_option ? wide_format : format,
	    /* Translation Hint: max 2 chars */
	     _("r"),
	    /* Translation Hint: max 2 chars */
	     _("b"),
	    /* Translation Hint: max 6 chars */
	     _("swpd"),
	    /* Translation Hint: max 6 chars */
	     _("free"),
	    /* Translation Hint: max 6 chars */
	     a_option ? _("inact") :
	    /* Translation Hint: max 6 chars */
			_("buff"),
	    /* Translation Hint: max 6 chars */
	     a_option ? _("active") :
	    /* Translation Hint: max 6 chars */
			_("cache"),
	    /* Translation Hint: max 4 chars */
	     _("si"),
	    /* Translation Hint: max 4 chars */
	     _("so"),
	    /* Translation Hint: max 5 chars */
	     _("bi"),
	    /* Translation Hint: max 5 chars */
	     _("bo"),
	    /* Translation Hint: max 4 chars */
	     _("in"),
	    /* Translation Hint: max 4 chars */
	     _("cs"),
	    /* Translation Hint: max 2 chars */
	     _("us"),
	    /* Translation Hint: max 2 chars */
	     _("sy"),
	    /* Translation Hint: max 2 chars */
	     _("id"),
	    /* Translation Hint: max 2 chars */
	     _("wa"),
	    /* Translation Hint: max 2 chars */
	     _("st"));

	if (t_option) {
		(void) time( &the_time );
		tm_ptr = localtime( &the_time );
		if (strftime(timebuf, sizeof(timebuf), "%Z", tm_ptr)) {
			timebuf[strlen(timestamp_header) - 1] = '\0';
		} else {
			timebuf[0] = '\0';
		}
		printf(" %*s", (int)(strlen(timestamp_header) - 1), timebuf);
	}

	printf("\n");
}


static unsigned long unitConvert(unsigned long size)
{
	float cvSize;
	cvSize = (float)size / dataUnit * ((statMode == SLABSTAT) ? 1 : 1024);
	return ((unsigned long)cvSize);
}

static void new_format(void)
{
	const char format[] =
	    "%2u %2u %6lu %6lu %6lu %6lu %4u %4u %5u %5u %4u %4u %2u %2u %2u %2u %2u";
	const char wide_format[] =
	    "%2u %2u %12lu %12lu %12lu %12lu %4u %4u %5u %5u %4u %4u %3u %3u %3u %3u %3u";

	unsigned int tog = 0;	/* toggle switch for cleaner code */
	unsigned int i;
	long hz;
	jiff cpu_use[2], cpu_sys[2], cpu_idl[2], cpu_iow[2], cpu_sto[2];
	jiff duse, dsys, didl, diow, dstl, Div, divo2;
	unsigned long pgpgin[2], pgpgout[2], pswpin[2] = {0,0}, pswpout[2];
	unsigned int intr[2], ctxt[2];
	unsigned int sleep_half;
	unsigned long kb_per_page = sysconf(_SC_PAGESIZE) / 1024ul;
	int debt = 0;		/* handle idle ticks running backwards */
	struct tm *tm_ptr;
	time_t the_time;
	char timebuf[32];
	struct procps_vmstat *vm_info;
	struct procps_stat *sys_info;
	struct procps_meminfo *mem_info;

	sleep_half = (sleep_time / 2);
	hz = procps_hertz_get();
	new_header();

	if (procps_vmstat_new(&vm_info) < 0)
	    xerrx(EXIT_FAILURE,
		    _("Unable to create vmstat structure"));
	if (procps_vmstat_read(vm_info) < 0)
	    xerrx(EXIT_FAILURE,
		    _("Unable to read vmstat information"));
	if (procps_stat_new(&sys_info) < 0)
	    xerrx(EXIT_FAILURE,
		    _("Unable to create system stat structure"));
	if (procps_stat_read(sys_info, 0) < 0)
	    xerrx(EXIT_FAILURE,
		    _("Unable to read system stat information"));
	if (procps_meminfo_new(&mem_info) < 0)
	    xerrx(EXIT_FAILURE, _("Unable to create meminfo structure"));
	if (procps_meminfo_read(mem_info) < 0)
	    xerrx(EXIT_FAILURE, _("Unable to read meminfo information"));

	if (t_option) {
		(void) time( &the_time );
		tm_ptr = localtime( &the_time );
		strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
	}
	/* Do the initial fill */
	cpu_use[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_USER) +
	    procps_stat_get_cpu(sys_info, PROCPS_CPU_NICE);
	cpu_sys[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_SYSTEM);
	    procps_stat_get_cpu(sys_info, PROCPS_CPU_IRQ) +
	    procps_stat_get_cpu(sys_info, PROCPS_CPU_SIRQ);
	cpu_idl[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_IDLE);
	cpu_iow[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_IOWAIT);
	cpu_sto[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_STOLEN);
	intr[tog] = procps_stat_get_sys(sys_info, PROCPS_STAT_INTR);
	ctxt[tog] = procps_stat_get_sys(sys_info, PROCPS_STAT_CTXT);
	pgpgin[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGIN);
	pgpgout[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGOUT);
	pswpin[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPIN);
	pswpout[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPOUT);

	Div = cpu_use[tog] + cpu_sys[tog] + cpu_idl[tog] + cpu_iow[tog] + cpu_sto[tog];
	if (!Div) {
	    Div = 1;
	    cpu_idl[tog] = 1;
	}
	divo2 = Div / 2UL;

	printf(w_option ? wide_format : format,
		procps_stat_get_sys(sys_info, PROCPS_STAT_PROCS_RUN),
		procps_stat_get_sys(sys_info, PROCPS_STAT_PROCS_BLK),
		unitConvert(procps_meminfo_get(mem_info, PROCPS_SWAP_USED)),
		unitConvert(procps_meminfo_get(mem_info, PROCPS_MEM_FREE)),
		unitConvert(procps_meminfo_get(mem_info, (a_option?PROCPS_MEM_INACTIVE:PROCPS_MEM_BUFFERS))),
		unitConvert(procps_meminfo_get(mem_info, a_option?PROCPS_MEM_ACTIVE:PROCPS_MEM_CACHED)),
	       (unsigned)( (unitConvert(procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPIN)  * kb_per_page) * hz + divo2) / Div ),
	       (unsigned)( (unitConvert(procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPOUT)  * kb_per_page) * hz + divo2) / Div ),
	       (unsigned)( (procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGIN) * hz + divo2) / Div ),
	       (unsigned)( (procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGOUT) * hz + divo2) / Div ),
	       (unsigned)( (intr[tog]		   * hz + divo2) / Div ),
	       (unsigned)( (ctxt[tog]		   * hz + divo2) / Div ),
	       (unsigned)( (100*cpu_use[tog]		+ divo2) / Div ),
	       (unsigned)( (100*cpu_sys[tog]		+ divo2) / Div ),
	       (unsigned)( (100*cpu_idl[tog]		+ divo2) / Div ),
	       (unsigned)( (100*cpu_iow[tog]		+ divo2) / Div ),
	       (unsigned)( (100*cpu_sto[tog]		+ divo2) / Div )
	);

	if (t_option) {
		printf(" %s", timebuf);
	}

	printf("\n");

	/* main loop */
	for (i = 1; infinite_updates || i < num_updates; i++) {
		sleep(sleep_time);
		if (moreheaders && ((i % height) == 0))
			new_header();
		tog = !tog;

		if (procps_stat_read(sys_info,0) < 0)
		    xerrx(EXIT_FAILURE,
			    _("Unable to read system stat information"));
		if (procps_vmstat_read(vm_info) < 0)
		    xerrx(EXIT_FAILURE,
			    _("Unable to read vmstat information"));

		cpu_use[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_USER) +
		    procps_stat_get_cpu(sys_info, PROCPS_CPU_NICE);
		cpu_sys[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_SYSTEM);
		    procps_stat_get_cpu(sys_info, PROCPS_CPU_IRQ) +
		    procps_stat_get_cpu(sys_info, PROCPS_CPU_SIRQ);
		cpu_idl[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_IDLE);
		cpu_iow[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_IOWAIT);
		cpu_sto[tog] = procps_stat_get_cpu(sys_info, PROCPS_CPU_STOLEN);
		intr[tog] = procps_stat_get_sys(sys_info, PROCPS_STAT_INTR);
		ctxt[tog] = procps_stat_get_sys(sys_info, PROCPS_STAT_CTXT);
		pgpgin[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGIN);
		pgpgout[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGOUT);
		pswpin[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPIN);
		pswpout[tog] = procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPOUT);

		if (procps_meminfo_read(mem_info) < 0)
		    xerrx(EXIT_FAILURE,
			    _("Unable to read memory information"));

		if (t_option) {
			(void) time( &the_time );
			tm_ptr = localtime( &the_time );
			strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
		}

		duse = cpu_use[tog] - cpu_use[!tog];
		dsys = cpu_sys[tog] - cpu_sys[!tog];
		didl = cpu_idl[tog] - cpu_idl[!tog];
		diow = cpu_iow[tog] - cpu_iow[!tog];
		dstl = cpu_sto[tog] - cpu_sto[!tog];

		/* idle can run backwards for a moment -- kernel "feature" */
		if (debt) {
			didl = (int)didl + debt;
			debt = 0;
		}
		if ((int)didl < 0) {
			debt = (int)didl;
			didl = 0;
		}

		Div = duse + dsys + didl + diow + dstl;
		if (!Div) Div = 1, didl = 1;
		divo2 = Div / 2UL;
		printf(w_option ? wide_format : format,
		       procps_stat_get_sys(sys_info, PROCPS_STAT_PROCS_RUN),
		       procps_stat_get_sys(sys_info, PROCPS_STAT_PROCS_BLK),
		       unitConvert(procps_meminfo_get(mem_info, PROCPS_SWAP_USED)),
		       unitConvert(procps_meminfo_get(mem_info, PROCPS_MEM_FREE)),
		       unitConvert(procps_meminfo_get(mem_info,
			       (a_option?PROCPS_MEM_INACTIVE:PROCPS_MEM_BUFFERS))),
		       unitConvert(procps_meminfo_get(mem_info,
			       (a_option?PROCPS_MEM_ACTIVE:PROCPS_MEM_CACHED))),
		       /*si */
		       (unsigned)( ( unitConvert((pswpin [tog] - pswpin [!tog])*kb_per_page)+sleep_half )/sleep_time ),
		       /* so */
		       (unsigned)( ( unitConvert((pswpout[tog] - pswpout[!tog])*kb_per_page)+sleep_half )/sleep_time ),
		       /* bi */
		       (unsigned)( (  pgpgin [tog] - pgpgin [!tog]	       +sleep_half )/sleep_time ),
		       /* bo */
		       (unsigned)( (  pgpgout[tog] - pgpgout[!tog]	       +sleep_half )/sleep_time ),
		       /* in */
		       (unsigned)( (  intr   [tog] - intr   [!tog]	       +sleep_half )/sleep_time ),
		       /* cs */
		       (unsigned)( (  ctxt   [tog] - ctxt   [!tog]	       +sleep_half )/sleep_time ),
		       /* us */
		       (unsigned)( (100*duse+divo2)/Div ),
		       /* sy */
		       (unsigned)( (100*dsys+divo2)/Div ),
		       /* id */
		       (unsigned)( (100*didl+divo2)/Div ),
		       /* wa */
		       (unsigned)( (100*diow+divo2)/Div ),
		       /* st */
		       (unsigned)( (100*dstl+divo2)/Div )
		);

		if (t_option) {
			printf(" %s", timebuf);
		}

		printf("\n");
	}
}

static void diskpartition_header(const char *partition_name)
{
	printf("%-10s %10s %10s %10s %10s\n",
	       partition_name,
       /* Translation Hint: Translating folloging disk partition
	* header fields that follow (marked with max x chars) might
	* not work, unless manual page is translated as well. */
	       /* Translation Hint: max 10 chars. The word is
	        * expected to be centralized, use spaces at the end
	        * to do that. */
	       _("reads  "),
	       /* Translation Hint: max 10 chars */
	       _("read sectors"),
	       /* Translation Hint: max 10 chars. The word is
	        * expected to be centralized, use spaces at the end
	        * to do that. */
	       _("writes   "),
	       /* Translation Hint: max 10 chars */
	       _("requested writes"));
}

static int diskpartition_format(const char *partition_name)
{
#define PARTGET(x) procps_diskstat_dev_get(disk_stat, (x), partid)
    struct procps_diskstat *disk_stat;
    const char format[] = "%20lu %10lu %10lu %10lu\n";
    int i, partid;

    if (procps_diskstat_new(&disk_stat) < 0)
        xerr(EXIT_FAILURE,
             _("Unable to create diskstat structure"));

    if (procps_diskstat_read(disk_stat) < 0)
        xerr(EXIT_FAILURE,
             _("Unable to read diskstat"));
    if ((partid = procps_diskstat_dev_getbyname(disk_stat, partition_name))
        < 0)
        xerrx(EXIT_FAILURE, _("Partition %s not found"), partition_name);

    diskpartition_header(partition_name);
    for (i=0; infinite_updates || i < num_updates ; i++) {
        if (procps_diskstat_read(disk_stat) < 0)
            xerr(EXIT_FAILURE,
                 _("Unable to read diskstat"));
        if ((partid = procps_diskstat_dev_getbyname(disk_stat, partition_name))
            < 0)
            xerrx(EXIT_FAILURE,
                  _("Partition %s not found"), partition_name);

        printf(format,
               PARTGET(PROCPS_DISKSTAT_READS),
               PARTGET(PROCPS_DISKSTAT_READ_SECTORS),
               PARTGET(PROCPS_DISKSTAT_WRITES),
               PARTGET(PROCPS_DISKSTAT_WRITE_SECTORS)
              );

        if (infinite_updates || i+1 < num_updates)
            sleep(sleep_time);
    }
    return 0;
}

static void diskheader(void)
{
    struct tm *tm_ptr;
    time_t the_time;
    char timebuf[32];

    /* Translation Hint: Translating folloging header & fields
     * that follow (marked with max x chars) might not work,
     * unless manual page is translated as well.  */
    const char *header =
        _("disk- ------------reads------------ ------------writes----------- -----IO------");
    const char *wide_header =
        _("disk- -------------------reads------------------- -------------------writes------------------ ------IO-------");
    const char *timestamp_header = _(" -----timestamp-----");

    const char format[] =
        "%5s %6s %6s %7s %7s %6s %6s %7s %7s %6s %6s";
    const char wide_format[] =
        "%5s %9s %9s %11s %11s %9s %9s %11s %11s %7s %7s";

    printf("%s", w_option ? wide_header : header);

    if (t_option) {
        printf("%s", timestamp_header);
    }

    printf("\n");

    printf(w_option ? wide_format : format,
           " ",
           /* Translation Hint: max 6 chars */
           _("total"),
           /* Translation Hint: max 6 chars */
           _("merged"),
           /* Translation Hint: max 7 chars */
           _("sectors"),
           /* Translation Hint: max 7 chars */
           _("ms"),
           /* Translation Hint: max 6 chars */
           _("total"),
           /* Translation Hint: max 6 chars */
           _("merged"),
           /* Translation Hint: max 7 chars */
           _("sectors"),
           /* Translation Hint: max 7 chars */
           _("ms"),
           /* Translation Hint: max 6 chars */
           _("cur"),
           /* Translation Hint: max 6 chars */
           _("sec"));

    if (t_option) {
        (void) time( &the_time );
        tm_ptr = localtime( &the_time );
        if (strftime(timebuf, sizeof(timebuf), "%Z", tm_ptr)) {
            timebuf[strlen(timestamp_header) - 1] = '\0';
        } else {
            timebuf[0] = '\0';
        }
        printf(" %*s", (int)(strlen(timestamp_header) - 1), timebuf);
    }

    printf("\n");
}

static void diskformat(void)
{
#define DSTAT(x) procps_diskstat_dev_get(disk_stat, (x), diskid)
    struct procps_diskstat *disk_stat;
    int i,diskid, disk_count;
    time_t the_time;
    struct tm *tm_ptr;
    char timebuf[32];
    const char format[] = "%-5s %6lu %6lu %7lu %7lu %6lu %6lu %7lu %7lu %6lu %6lu";
    const char wide_format[] = "%-5s %9lu %9lu %lu %1u %9lu %9lu %lu %1u %7lu %7lu";

    if (procps_diskstat_new(&disk_stat) < 0)
        xerr(EXIT_FAILURE,
             _("Unable to create diskstat structure"));

    if (!moreheaders)
        diskheader();
    for (i=0; infinite_updates || i < num_updates ; i++) {
        if (procps_diskstat_read(disk_stat) < 0)
            xerr(EXIT_FAILURE,
                 _("Unable to read diskstat data"));

        if (t_option) {
            (void) time( &the_time );
            tm_ptr = localtime( &the_time );
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
        }
        disk_count = procps_diskstat_dev_count(disk_stat);
        for (diskid = 0; diskid < disk_count; diskid++) {
            if (procps_diskstat_dev_isdisk(disk_stat, diskid) != 1)
                continue; /* not a disk */
            if (moreheaders && ((diskid % height) == 0))
                diskheader();
            printf(w_option ? wide_format : format,
                   procps_diskstat_dev_getname(disk_stat, diskid),
                   DSTAT(PROCPS_DISKSTAT_READS),
                   DSTAT(PROCPS_DISKSTAT_READS_MERGED),
                   DSTAT(PROCPS_DISKSTAT_READ_SECTORS),
                   DSTAT(PROCPS_DISKSTAT_READ_TIME),
                   DSTAT(PROCPS_DISKSTAT_WRITES),
                   DSTAT(PROCPS_DISKSTAT_WRITES_MERGED),
                   DSTAT(PROCPS_DISKSTAT_WRITE_SECTORS),
                   DSTAT(PROCPS_DISKSTAT_WRITE_TIME),
                   DSTAT(PROCPS_DISKSTAT_IO_INPROGRESS) / 1000,
                   DSTAT(PROCPS_DISKSTAT_IO_TIME) / 1000);
            if (t_option)
                printf(" %s\n", timebuf);
            else
                printf("\n");
            fflush(stdout);
        }
        if (infinite_updates || i+1 < num_updates)
            sleep(sleep_time);
    }
#undef DSTAT
}

static void slabheader(void)
{
	printf("%-24s %6s %6s %6s %6s\n",
	/* Translation Hint: Translating folloging slab fields that
	 * follow (marked with max x chars) might not work, unless
	 * manual page is translated as well.  */
	       /* Translation Hint: max 24 chars */
	       _("Cache"),
	       /* Translation Hint: max 6 chars */
	       _("Num"),
	       /* Translation Hint: max 6 chars */
	       _("Total"),
	       /* Translation Hint: max 6 chars */
	       _("Size"),
	       /* Translation Hint: max 6 chars */
	       _("Pages"));
}

static void slabformat (void)
{
 #define CHAINS_ALLOC  150
 #define MAX_ITEMS (int)(sizeof(node_items) / sizeof(node_items[0]))
 #define SLAB_NUM(c,e) (unsigned)c->head[e].result.num
 #define SLAB_STR(c,e) c->head[e].result.str
    struct procps_slabinfo *slab_info;
    struct slabnode_chain **v;
    int i, j, nr_slabs;
    const char format[] = "%-24.24s %6u %6u %6u %6u\n";
    enum slabnode_item node_items[] = {
        PROCPS_SLABNODE_AOBJS,    PROCPS_SLABNODE_OBJS,
        PROCPS_SLABNODE_OBJ_SIZE, PROCPS_SLABNODE_OBJS_PER_SLAB,
        PROCPS_SLABNODE_NAME };
    enum rel_enums {
        slab_AOBJS, slab_OBJS, slab_OSIZE, slab_OPS, slab_NAME };

    if (procps_slabinfo_new(&slab_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create slabinfo structure"));
    if (!(v = procps_slabnode_chains_alloc(slab_info, CHAINS_ALLOC, 0, MAX_ITEMS, node_items)))
        xerrx(EXIT_FAILURE, _("Unable to allocate slabinfo nodes"));

    if (!moreheaders)
        slabheader();

    for (i = 0; infinite_updates || i < num_updates; i++) {
        // this next guy also performs the procps_slabnode_read() call
        if ((nr_slabs = procps_slabnode_chains_fill(slab_info, v, CHAINS_ALLOC)) < 0)
            xerrx(EXIT_FAILURE, _("Unable to get slabinfo node data, requires root permission"));
        if (!(v = procps_slabnode_chains_sort(slab_info, v, nr_slabs, PROCPS_SLABNODE_NAME)))
            xerrx(EXIT_FAILURE, _("Unable to sort slab nodes"));

        for (j = 0; j < nr_slabs; j++) {
            if (moreheaders && ((j % height) == 0))
                slabheader();
            printf(format,
                   SLAB_STR(v[j], slab_NAME),
                   SLAB_NUM(v[j], slab_AOBJS), SLAB_NUM(v[j], slab_OBJS),
                   SLAB_NUM(v[j], slab_OSIZE), SLAB_NUM(v[j], slab_OPS));
        }
        if (infinite_updates || i+1 < num_updates)
            sleep(sleep_time);
    }
    procps_slabinfo_unref(&slab_info);
 #undef CHAINS_ALLOC
 #undef MAX_ITEMS
 #undef SLAB_NUM
 #undef SLAB_STR
}

static void disksum_format(void)
{
#define DSTAT(x) procps_diskstat_dev_get(disk_stat, (x), devid)
    struct procps_diskstat *disk_stat;

    if (procps_diskstat_new(&disk_stat) < 0)
        xerr(EXIT_FAILURE,
             _("Unable to create diskstat structure"));

    if (procps_diskstat_read(disk_stat) < 0)
        xerr(EXIT_FAILURE,
             _("Unable to read diskstat"));

    int devid, dev_count, disk_count, part_count ;
    unsigned long reads, merged_reads, read_sectors, milli_reading, writes,
                  merged_writes, written_sectors, milli_writing, inprogress_IO,
                  milli_spent_IO, weighted_milli_spent_IO;

    reads = merged_reads = read_sectors = milli_reading = writes =
        merged_writes = written_sectors = milli_writing = inprogress_IO =
        milli_spent_IO = weighted_milli_spent_IO = 0;
    disk_count = part_count = 0;

    if ((dev_count = procps_diskstat_dev_count(disk_stat)) < 0)
        xerr(EXIT_FAILURE,
             _("Unable to count diskstat devices"));

    for (devid=0; devid < dev_count; devid++) {
        if (procps_diskstat_dev_isdisk(disk_stat, devid) != 1) {
            part_count++;
            continue; /* not a disk */
        }
        disk_count++;
        reads += DSTAT(PROCPS_DISKSTAT_READS);
        merged_reads += DSTAT(PROCPS_DISKSTAT_READS_MERGED);
        read_sectors += DSTAT(PROCPS_DISKSTAT_READ_SECTORS);
        milli_reading += DSTAT(PROCPS_DISKSTAT_READ_TIME);
        writes += DSTAT(PROCPS_DISKSTAT_WRITES);
        merged_writes += DSTAT(PROCPS_DISKSTAT_WRITES_MERGED);
        written_sectors += DSTAT(PROCPS_DISKSTAT_WRITE_SECTORS);
        milli_writing += DSTAT(PROCPS_DISKSTAT_WRITE_TIME);
        inprogress_IO += DSTAT(PROCPS_DISKSTAT_IO_INPROGRESS) / 1000;
        milli_spent_IO += DSTAT(PROCPS_DISKSTAT_IO_TIME) / 1000;
        weighted_milli_spent_IO += DSTAT(PROCPS_DISKSTAT_IO_TIME) / 1000;
    }
    printf(_("%13d disks\n"), disk_count);
    printf(_("%13d partitions\n"), part_count);
    printf(_("%13lu reads\n"), reads);
    printf(_("%13lu merged reads\n"), merged_reads);
    printf(_("%13lu read sectors\n"), read_sectors);
    printf(_("%13lu milli reading\n"), milli_reading);
    printf(_("%13lu writes\n"), writes);
    printf(_("%13lu merged writes\n"), merged_writes);
    printf(_("%13lu written sectors\n"), written_sectors);
    printf(_("%13lu milli writing\n"), milli_writing);
    printf(_("%13lu inprogress IO\n"), inprogress_IO);
    printf(_("%13lu milli spent IO\n"), milli_spent_IO);
    printf(_("%13lu milli weighted IO\n"), weighted_milli_spent_IO);
#undef DSTAT
}

static void sum_format(void)
{
	struct procps_stat *sys_info;
	struct procps_vmstat *vm_info;
	struct procps_meminfo *mem_info;

	if (procps_stat_new(&sys_info) < 0)
	    xerrx(EXIT_FAILURE,
		_("Unable to create system stat structure"));
	if (procps_stat_read(sys_info,0) < 0)
	    xerrx(EXIT_FAILURE,
		_("Unable to read system stat information"));
	if (procps_vmstat_new(&vm_info) < 0)
	    xerrx(EXIT_FAILURE,
		_("Unable to create vmstat structure"));
	if (procps_vmstat_read(vm_info) < 0)
	    xerrx(EXIT_FAILURE,
		_("Unable to read vmstat information"));

	if (procps_meminfo_new(&mem_info) < 0)
	    xerrx(EXIT_FAILURE, _("Unable to create meminfo structure"));
	if (procps_meminfo_read(mem_info) < 0)
	    xerrx(EXIT_FAILURE, _("Unable to read meminfo information"));

	printf(_("%13lu %s total memory\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_MEM_TOTAL)), szDataUnit);
	printf(_("%13lu %s used memory\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_MEM_USED)), szDataUnit);
	printf(_("%13lu %s active memory\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_MEM_ACTIVE)), szDataUnit);
	printf(_("%13lu %s inactive memory\n"), unitConvert(
		    procps_meminfo_get(mem_info, PROCPS_MEM_INACTIVE)), szDataUnit);
	printf(_("%13lu %s free memory\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_MEM_FREE)), szDataUnit);
	printf(_("%13lu %s buffer memory\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_MEM_BUFFERS)), szDataUnit);
	printf(_("%13lu %s swap cache\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_MEM_CACHED)), szDataUnit);
	printf(_("%13lu %s total swap\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_SWAP_TOTAL)), szDataUnit);
	printf(_("%13lu %s used swap\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_SWAP_USED)), szDataUnit);
	printf(_("%13lu %s free swap\n"), unitConvert(procps_meminfo_get(
			mem_info, PROCPS_SWAP_FREE)), szDataUnit);
	printf(_("%13lld non-nice user cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_USER));
	printf(_("%13lld nice user cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_NICE));
	printf(_("%13lld system cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_SYSTEM));
	printf(_("%13lld idle cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_IDLE));
	printf(_("%13lld IO-wait cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_IOWAIT));
	printf(_("%13lld IRQ cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_IRQ));
	printf(_("%13lld softirq cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_SIRQ));
	printf(_("%13lld stolen cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_STOLEN));
	printf(_("%13lld non-nice guest cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_GUEST));
	printf(_("%13lld nice guest cpu ticks\n"), procps_stat_get_cpu(sys_info, PROCPS_CPU_GNICE));
	printf(_("%13lu pages paged in\n"), procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGIN));
	printf(_("%13lu pages paged out\n"), procps_vmstat_get(vm_info, PROCPS_VMSTAT_PGPGOUT));
	printf(_("%13lu pages swapped in\n"), procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPIN));
	printf(_("%13lu pages swapped out\n"), procps_vmstat_get(vm_info, PROCPS_VMSTAT_PSWPOUT));
	printf(_("%13u interrupts\n"), procps_stat_get_sys(sys_info, PROCPS_STAT_INTR));
	printf(_("%13u CPU context switches\n"), procps_stat_get_sys(sys_info, PROCPS_STAT_CTXT));
	printf(_("%13u boot time\n"), procps_stat_get_sys(sys_info, PROCPS_STAT_BTIME));
	printf(_("%13u forks\n"), procps_stat_get_sys(sys_info, PROCPS_STAT_PROCS));
}

static void fork_format(void)
{
    struct procps_stat *sys_info;

    if (procps_stat_new(&sys_info) < 0)
	xerrx(EXIT_FAILURE,
		_("Unable to create system stat structure"));
    if (procps_stat_read(sys_info,0) < 0)
	xerrx(EXIT_FAILURE,
		_("Unable to read system stat information"));

    printf(_("%13u forks\n"), procps_stat_get_sys(sys_info, PROCPS_STAT_PROCS));
}

static int winhi(void)
{
	struct winsize win;
	int rows = 24;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1 && 0 < win.ws_row)
		rows = win.ws_row;

	return rows;
}

int main(int argc, char *argv[])
{
	char *partition = NULL;
	int c;
	long tmp;

	static const struct option longopts[] = {
		{"active", no_argument, NULL, 'a'},
		{"forks", no_argument, NULL, 'f'},
		{"slabs", no_argument, NULL, 'm'},
		{"one-header", no_argument, NULL, 'n'},
		{"stats", no_argument, NULL, 's'},
		{"disk", no_argument, NULL, 'd'},
		{"disk-sum", no_argument, NULL, 'D'},
		{"partition", required_argument, NULL, 'p'},
		{"unit", required_argument, NULL, 'S'},
		{"wide", no_argument, NULL, 'w'},
		{"timestamp", no_argument, NULL, 't'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((c =
		getopt_long(argc, argv, "afmnsdDp:S:wthV", longopts,
			    NULL)) != EOF)
		switch (c) {
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		case 'd':
			statMode |= DISKSTAT;
			break;
		case 'a':
			/* active/inactive mode */
			a_option = 1;
			break;
		case 'f':
			/* FIXME: check for conflicting args */
			fork_format();
			exit(0);
		case 'm':
			statMode |= SLABSTAT;
			break;
		case 'D':
			statMode |= DISKSUMSTAT;
			break;
		case 'n':
			/* print only one header */
			moreheaders = FALSE;
			break;
		case 'p':
			statMode |= PARTITIONSTAT;
			partition = optarg;
			if (memcmp(partition, "/dev/", 5) == 0)
				partition += 5;
			break;
		case 'S':
			switch (optarg[0]) {
			case 'b':
			case 'B':
				dataUnit = UNIT_B;
				break;
			case 'k':
				dataUnit = UNIT_k;
				break;
			case 'K':
				dataUnit = UNIT_K;
				break;
			case 'm':
				dataUnit = UNIT_m;
				break;
			case 'M':
				dataUnit = UNIT_M;
				break;
			default:
				xerrx(EXIT_FAILURE,
				     /* Translation Hint: do not change argument characters */
				     _("-S requires k, K, m or M (default is KiB)"));
			}
			szDataUnit[0] = optarg[0];
			break;
		case 's':
			statMode |= VMSUMSTAT;
			break;
		case 'w':
			w_option = 1;
			break;
		case 't':
			t_option = 1;
			break;
		default:
			/* no other aguments defined yet. */
			usage(stderr);
		}

	if (optind < argc) {
		tmp = strtol_or_err(argv[optind++], _("failed to parse argument"));
		if (tmp < 1)
			xerrx(EXIT_FAILURE, _("delay must be positive integer"));
		else if (UINT_MAX < tmp)
			xerrx(EXIT_FAILURE, _("too large delay value"));
		sleep_time = tmp;
		infinite_updates = 1;
	}
    num_updates = 1;
	if (optind < argc) {
		num_updates = strtol_or_err(argv[optind++], _("failed to parse argument"));
		infinite_updates = 0;
	}
	if (optind < argc)
		usage(stderr);

	if (moreheaders) {
		int wheight = winhi() - 3;
		height = ((wheight > 0) ? wheight : 22);
	}
	setlinebuf(stdout);
	switch (statMode) {
	case (VMSTAT):
		new_format();
		break;
	case (VMSUMSTAT):
		sum_format();
		break;
	case (DISKSTAT):
		diskformat();
		break;
	case (PARTITIONSTAT):
		if (diskpartition_format(partition) == -1)
			printf(_("partition was not found\n"));
		break;
	case (SLABSTAT):
		slabformat();
		break;
	case (DISKSUMSTAT):
		disksum_format();
		break;
	default:
		usage(stderr);
		break;
	}
	return 0;
}
