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
#include "proc/version.h"

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
static unsigned long num_updates;
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

#if 0
/* produce: "  6  ", "123  ", "123k ", etc. */
static int format_1024(unsigned long long val64, char *restrict dst)
{
	unsigned oldval;
	const char suffix[] = " kmgtpe";
	unsigned level = 0;
	unsigned val32;

	if (val64 < 1000) {
		/* special case to avoid "6.0  " when plain "  6  " would do */
		val32 = val64;
		return sprintf(dst, "%3u  ", val32);
	}

	while (val64 > 0xffffffffull) {
		level++;
		val64 /= 1024;
	}

	val32 = val64;

	while (val32 > 999) {
		level++;
		oldval = val32;
		val32 /= 1024;
	}

	if (val32 < 10) {
		unsigned fract = (oldval % 1024) * 10 / 1024;
		return sprintf(dst, "%u.%u%c ", val32, fract, suffix[level]);
	}
	return sprintf(dst, "%3u%c ", val32, suffix[level]);
}

/* produce: "  6  ", "123  ", "123k ", etc. */
static int format_1000(unsigned long long val64, char *restrict dst)
{
	unsigned oldval;
	const char suffix[] = " kmgtpe";
	unsigned level = 0;
	unsigned val32;

	if (val64 < 1000) {
		/* special case to avoid "6.0  " when plain "  6  " would do */
		val32 = val64;
		return sprintf(dst, "%3u  ", val32);
	}

	while (val64 > 0xffffffffull) {
		level++;
		val64 /= 1000;
	}

	val32 = val64;

	while (val32 > 999) {
		level++;
		oldval = val32;
		val32 /= 1000;
	}

	if (val32 < 10) {
		unsigned fract = (oldval % 1000) / 100;
		return sprintf(dst, "%u.%u%c ", val32, fract, suffix[level]);
	}
	return sprintf(dst, "%3u%c ", val32, suffix[level]);
}
#endif

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
	unsigned int hz = Hertz;
	unsigned int running, blocked, dummy_1, dummy_2;
	jiff cpu_use[2], cpu_nic[2], cpu_sys[2], cpu_idl[2], cpu_iow[2],
	    cpu_xxx[2], cpu_yyy[2], cpu_zzz[2];
	jiff duse, dsys, didl, diow, dstl, Div, divo2;
	unsigned long pgpgin[2], pgpgout[2], pswpin[2], pswpout[2];
	unsigned int intr[2], ctxt[2];
	unsigned int sleep_half;
	unsigned long kb_per_page = sysconf(_SC_PAGESIZE) / 1024ul;
	int debt = 0;		/* handle idle ticks running backwards */
	struct tm *tm_ptr;
	time_t the_time;
	char timebuf[32];

	sleep_half = (sleep_time / 2);
	new_header();
	meminfo();

	getstat(cpu_use, cpu_nic, cpu_sys, cpu_idl, cpu_iow, cpu_xxx, cpu_yyy,
		cpu_zzz, pgpgin, pgpgout, pswpin, pswpout, intr, ctxt, &running,
		&blocked, &dummy_1, &dummy_2);

	if (t_option) {
		(void) time( &the_time );
		tm_ptr = localtime( &the_time );
		strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
	}

	duse = *cpu_use + *cpu_nic;
	dsys = *cpu_sys + *cpu_xxx + *cpu_yyy;
	didl = *cpu_idl;
	diow = *cpu_iow;
	dstl = *cpu_zzz;
	Div = duse + dsys + didl + diow + dstl;
	if (!Div) Div = 1, didl = 1;
	divo2 = Div / 2UL;
	printf(w_option ? wide_format : format,
	       running, blocked,
	       unitConvert(kb_swap_used), unitConvert(kb_main_free),
	       unitConvert(a_option?kb_inactive:kb_main_buffers),
	       unitConvert(a_option?kb_active:kb_main_cached),
	       (unsigned)( (unitConvert(*pswpin  * kb_per_page) * hz + divo2) / Div ),
	       (unsigned)( (unitConvert(*pswpout * kb_per_page) * hz + divo2) / Div ),
	       (unsigned)( (*pgpgin		   * hz + divo2) / Div ),
	       (unsigned)( (*pgpgout		   * hz + divo2) / Div ),
	       (unsigned)( (*intr		   * hz + divo2) / Div ),
	       (unsigned)( (*ctxt		   * hz + divo2) / Div ),
	       (unsigned)( (100*duse			+ divo2) / Div ),
	       (unsigned)( (100*dsys			+ divo2) / Div ),
	       (unsigned)( (100*didl			+ divo2) / Div ),
	       (unsigned)( (100*diow			+ divo2) / Div ),
	       (unsigned)( (100*dstl			+ divo2) / Div )
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

		meminfo();

		getstat(cpu_use + tog, cpu_nic + tog, cpu_sys + tog,
			cpu_idl + tog, cpu_iow + tog, cpu_xxx + tog,
			cpu_yyy + tog, cpu_zzz + tog, pgpgin + tog,
			pgpgout + tog, pswpin + tog, pswpout + tog, intr + tog,
			ctxt + tog, &running, &blocked, &dummy_1, &dummy_2);

		if (t_option) {
			(void) time( &the_time );
			tm_ptr = localtime( &the_time );
			strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
		}

		duse =
		    cpu_use[tog] - cpu_use[!tog] + cpu_nic[tog] - cpu_nic[!tog];
		dsys =
		    cpu_sys[tog] - cpu_sys[!tog] + cpu_xxx[tog] -
		    cpu_xxx[!tog] + cpu_yyy[tog] - cpu_yyy[!tog];
		didl = cpu_idl[tog] - cpu_idl[!tog];
		diow = cpu_iow[tog] - cpu_iow[!tog];
		dstl = cpu_zzz[tog] - cpu_zzz[!tog];

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
		       running,
		       blocked,
		       unitConvert(kb_swap_used),unitConvert(kb_main_free),
		       unitConvert(a_option?kb_inactive:kb_main_buffers),
		       unitConvert(a_option?kb_active:kb_main_cached),
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
	FILE *fDiskstat;
	struct disk_stat *disks;
	struct partition_stat *partitions, *current_partition = NULL;
	unsigned long ndisks, j, k, npartitions;
	const char format[] = "%20u %10llu %10u %10llu\n";

	fDiskstat = fopen("/proc/diskstats", "rb");
	if (!fDiskstat)
		xerrx(EXIT_FAILURE,
		     _("your kernel does not support diskstat. (2.5.70 or above required)"));

	fclose(fDiskstat);
	ndisks = getdiskstat(&disks, &partitions);
	npartitions = getpartitions_num(disks, ndisks);
	for (k = 0; k < npartitions; k++) {
		if (!strcmp(partition_name, partitions[k].partition_name)) {
			current_partition = &(partitions[k]);
		}
	}
	if (!current_partition) {
		free(disks);
		free(partitions);
		return -1;
	}
	diskpartition_header(partition_name);
	printf(format,
	       current_partition->reads, current_partition->reads_sectors,
	       current_partition->writes, current_partition->requested_writes);
	fflush(stdout);
	free(disks);
	free(partitions);
	for (j = 1; infinite_updates || j < num_updates; j++) {
		if (moreheaders && ((j % height) == 0))
			diskpartition_header(partition_name);
		sleep(sleep_time);
		ndisks = getdiskstat(&disks, &partitions);
		npartitions = getpartitions_num(disks, ndisks);
		current_partition = NULL;
		for (k = 0; k < npartitions; k++) {
			if (!strcmp
			    (partition_name, partitions[k].partition_name)) {
				current_partition = &(partitions[k]);
			}
		}
		if (!current_partition) {
			free(disks);
			free(partitions);
			return -1;
		}
		printf(format,
		       current_partition->reads,
		       current_partition->reads_sectors,
		       current_partition->writes,
		       current_partition->requested_writes);
		fflush(stdout);
		free(disks);
		free(partitions);
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
	const char format[] =
	    "%-5s %6u %6u %7llu %7u %6u %6u %7llu %7u %6u %6u";
	const char wide_format[] =
	    "%-5s %9u %9u %11llu %11u %9u %9u %11llu %11u %7u %7u";

	FILE *fDiskstat;
	struct disk_stat *disks;
	struct partition_stat *partitions;
	unsigned long ndisks, i, j, k;
	struct tm *tm_ptr;
	time_t the_time;
	char timebuf[32];


	if ((fDiskstat = fopen("/proc/diskstats", "rb"))) {
		fclose(fDiskstat);
		ndisks = getdiskstat(&disks, &partitions);

		if (t_option) {
			(void) time( &the_time );
			tm_ptr = localtime( &the_time );
			strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
		}

		if (!moreheaders)
			diskheader();
		for (k = 0; k < ndisks; k++) {
			if (moreheaders && ((k % height) == 0))
				diskheader();
			printf(w_option ? wide_format : format,
			       disks[k].disk_name,
			       disks[k].reads,
			       disks[k].merged_reads,
			       disks[k].reads_sectors,
			       disks[k].milli_reading,
			       disks[k].writes,
			       disks[k].merged_writes,
			       disks[k].written_sectors,
			       disks[k].milli_writing,
			       disks[k].inprogress_IO ? disks[k].inprogress_IO / 1000 : 0,
			       disks[k].milli_spent_IO ? disks[k].
			       milli_spent_IO / 1000 : 0);

			if (t_option) {
				printf(" %s", timebuf);
			}

			printf("\n");
			fflush(stdout);
		}
		free(disks);
		free(partitions);

		for (j = 1; infinite_updates || j < num_updates; j++) {
			sleep(sleep_time);
			ndisks = getdiskstat(&disks, &partitions);

			if (t_option) {
				(void) time( &the_time );
				tm_ptr = localtime( &the_time );
				strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_ptr);
			}

			for (i = 0; i < ndisks; i++, k++) {
				if (moreheaders && ((k % height) == 0))
					diskheader();
				printf(w_option ? wide_format : format,
				       disks[i].disk_name,
				       disks[i].reads,
				       disks[i].merged_reads,
				       disks[i].reads_sectors,
				       disks[i].milli_reading,
				       disks[i].writes,
				       disks[i].merged_writes,
				       disks[i].written_sectors,
				       disks[i].milli_writing,
				       disks[i].inprogress_IO ? disks[i].inprogress_IO / 1000 : 0,
				       disks[i].milli_spent_IO ? disks[i].
				       milli_spent_IO / 1000 : 0);

				if (t_option) {
					printf(" %s", timebuf);
				}

				printf("\n");
				fflush(stdout);
			}
			free(disks);
			free(partitions);
		}
	} else
		xerrx(EXIT_FAILURE,
		     _("your kernel does not support diskstat (2.5.70 or above required)"));
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

static void slabformat(void)
{
	FILE *fSlab;
	struct slab_cache *slabs;
	unsigned long nSlab, i, j, k;
	const char format[] = "%-24s %6u %6u %6u %6u\n";

	fSlab = fopen("/proc/slabinfo", "rb");
	if (!fSlab) {
		xwarnx(_("your kernel does not support slabinfo or your permissions are insufficient"));
		return;
	}

	if (!moreheaders)
		slabheader();
	nSlab = getslabinfo(&slabs);
	for (k = 0; k < nSlab; k++) {
		if (moreheaders && ((k % height) == 0))
			slabheader();
		printf(format,
		       slabs[k].name,
		       slabs[k].active_objs,
		       slabs[k].num_objs,
		       slabs[k].objsize, slabs[k].objperslab);
	}
	free(slabs);
	for (j = 1, k = 1; infinite_updates || j < num_updates; j++) {
		sleep(sleep_time);
		nSlab = getslabinfo(&slabs);
		for (i = 0; i < nSlab; i++, k++) {
			if (moreheaders && ((k % height) == 0))
				slabheader();
			printf(format,
			       slabs[i].name,
			       slabs[i].active_objs,
			       slabs[i].num_objs,
			       slabs[i].objsize, slabs[i].objperslab);
		}
		free(slabs);
	}
	fclose(fSlab);
}

static void disksum_format(void)
{

	FILE *fDiskstat;
	struct disk_stat *disks;
	struct partition_stat *partitions;
	int ndisks, i;
	unsigned long reads, merged_reads, read_sectors, milli_reading, writes,
	    merged_writes, written_sectors, milli_writing, inprogress_IO,
	    milli_spent_IO, weighted_milli_spent_IO;

	reads = merged_reads = read_sectors = milli_reading = writes =
	    merged_writes = written_sectors = milli_writing = inprogress_IO =
	    milli_spent_IO = weighted_milli_spent_IO = 0;

	if ((fDiskstat = fopen("/proc/diskstats", "rb"))) {
		fclose(fDiskstat);
		ndisks = getdiskstat(&disks, &partitions);
		printf(_("%13d disks \n"), ndisks);
		printf(_("%13d partitions \n"),
		       getpartitions_num(disks, ndisks));

		for (i = 0; i < ndisks; i++) {
			reads		+= disks[i].reads;
			merged_reads	+= disks[i].merged_reads;
			read_sectors	+= disks[i].reads_sectors;
			milli_reading	+= disks[i].milli_reading;
			writes		+= disks[i].writes;
			merged_writes	+= disks[i].merged_writes;
			written_sectors	+= disks[i].written_sectors;
			milli_writing	+= disks[i].milli_writing;
			inprogress_IO	+= disks[i].inprogress_IO ? disks[i].inprogress_IO / 1000 : 0;
			milli_spent_IO	+= disks[i].milli_spent_IO ? disks[i].milli_spent_IO / 1000 : 0;
		}

		printf(_("%13lu total reads\n"), reads);
		printf(_("%13lu merged reads\n"), merged_reads);
		printf(_("%13lu read sectors\n"), read_sectors);
		printf(_("%13lu milli reading\n"), milli_reading);
		printf(_("%13lu writes\n"), writes);
		printf(_("%13lu merged writes\n"), merged_writes);
		printf(_("%13lu written sectors\n"), written_sectors);
		printf(_("%13lu milli writing\n"), milli_writing);
		printf(_("%13lu inprogress IO\n"), inprogress_IO);
		printf(_("%13lu milli spent IO\n"), milli_spent_IO);

		free(disks);
		free(partitions);
	}
}

static void sum_format(void)
{
	unsigned int running, blocked, btime, processes;
	jiff cpu_use, cpu_nic, cpu_sys, cpu_idl, cpu_iow, cpu_xxx, cpu_yyy, cpu_zzz;
	unsigned long pgpgin, pgpgout, pswpin, pswpout;
	unsigned int intr, ctxt;

	meminfo();

	getstat(&cpu_use, &cpu_nic, &cpu_sys, &cpu_idl,
		&cpu_iow, &cpu_xxx, &cpu_yyy, &cpu_zzz,
		&pgpgin, &pgpgout, &pswpin, &pswpout,
		&intr, &ctxt, &running, &blocked, &btime, &processes);

	printf(_("%13lu %s total memory\n"), unitConvert(kb_main_total), szDataUnit);
	printf(_("%13lu %s used memory\n"), unitConvert(kb_main_used), szDataUnit);
	printf(_("%13lu %s active memory\n"), unitConvert(kb_active), szDataUnit);
	printf(_("%13lu %s inactive memory\n"), unitConvert(kb_inactive), szDataUnit);
	printf(_("%13lu %s free memory\n"), unitConvert(kb_main_free), szDataUnit);
	printf(_("%13lu %s buffer memory\n"), unitConvert(kb_main_buffers), szDataUnit);
	printf(_("%13lu %s swap cache\n"), unitConvert(kb_main_cached), szDataUnit);
	printf(_("%13lu %s total swap\n"), unitConvert(kb_swap_total), szDataUnit);
	printf(_("%13lu %s used swap\n"), unitConvert(kb_swap_used), szDataUnit);
	printf(_("%13lu %s free swap\n"), unitConvert(kb_swap_free), szDataUnit);
	printf(_("%13lld non-nice user cpu ticks\n"), cpu_use);
	printf(_("%13lld nice user cpu ticks\n"), cpu_nic);
	printf(_("%13lld system cpu ticks\n"), cpu_sys);
	printf(_("%13lld idle cpu ticks\n"), cpu_idl);
	printf(_("%13lld IO-wait cpu ticks\n"), cpu_iow);
	printf(_("%13lld IRQ cpu ticks\n"), cpu_xxx);
	printf(_("%13lld softirq cpu ticks\n"), cpu_yyy);
	printf(_("%13lld stolen cpu ticks\n"), cpu_zzz);
	printf(_("%13lu pages paged in\n"), pgpgin);
	printf(_("%13lu pages paged out\n"), pgpgout);
	printf(_("%13lu pages swapped in\n"), pswpin);
	printf(_("%13lu pages swapped out\n"), pswpout);
	printf(_("%13u interrupts\n"), intr);
	printf(_("%13u CPU context switches\n"), ctxt);
	printf(_("%13u boot time\n"), btime);
	printf(_("%13u forks\n"), processes);
}

static void fork_format(void)
{
	unsigned int running, blocked, btime, processes;
	jiff cpu_use, cpu_nic, cpu_sys, cpu_idl, cpu_iow, cpu_xxx, cpu_yyy, cpu_zzz;
	unsigned long pgpgin, pgpgout, pswpin, pswpout;
	unsigned int intr, ctxt;

	getstat(&cpu_use, &cpu_nic, &cpu_sys, &cpu_idl,
		&cpu_iow, &cpu_xxx, &cpu_yyy, &cpu_zzz,
		&pgpgin, &pgpgout, &pswpin, &pswpout,
		&intr, &ctxt, &running, &blocked, &btime, &processes);

	printf(_("%13u forks\n"), processes);
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
