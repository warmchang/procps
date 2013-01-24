/*
 * pmap.c - print process memory mapping
 * Copyright 2002 Albert Cahalan
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "proc/escape.h"
#include "xalloc.h"
#include "proc/readproc.h"
#include "proc/version.h"

static void __attribute__ ((__noreturn__))
    usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
		_(" %s [options] pid [pid ...]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_("  -x, --extended              show details\n"
		"  -X                          show even more details\n"
		"            WARNING: format changes according to /proc/PID/smaps\n"
		"  -XX                         show everything the kernel provides\n"
		"  -d, --device                show the device format\n"
		"  -q, --quiet                 do not display header and footer\n"
		"  -A, --range=<low>[,<high>]  limit results to the given range\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("pmap(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static char mapbuf[1024];
static char cmdbuf[512];

static unsigned KLONG range_low;
static unsigned KLONG range_high = ~0ull;

static int d_option;
static int q_option;
static int x_option;
static int X_option;

static unsigned shm_minor = ~0u;

static void discover_shm_minor(void)
{
	void *addr;
	int shmid;
	char mapbuf_b[256];

	if (!freopen("/proc/self/maps", "r", stdin))
		return;

	/* create */
	shmid = shmget(IPC_PRIVATE, 42, IPC_CREAT | 0666);
	if (shmid == -1)
		/* failed; oh well */
		return;
	/* attach */
	addr = shmat(shmid, NULL, SHM_RDONLY);
	if (addr == (void *)-1)
		goto out_destroy;

	while (fgets(mapbuf_b, sizeof mapbuf_b, stdin)) {
		char flags[32];
		/* to clean up unprintables */
		char *tmp;
		unsigned KLONG start, end;
		unsigned long long file_offset, inode;
		unsigned dev_major, dev_minor;
		sscanf(mapbuf_b, "%" KLF "x-%" KLF "x %31s %llx %x:%x %llu", &start,
		       &end, flags, &file_offset, &dev_major, &dev_minor,
		       &inode);
		tmp = strchr(mapbuf_b, '\n');
		if (tmp)
			*tmp = '\0';
		tmp = mapbuf_b;
		while (*tmp) {
			if (!isprint(*tmp))
				*tmp = '?';
			tmp++;
		}
		if (start > (unsigned long)addr)
			continue;
		if (dev_major)
			continue;
		if (flags[3] != 's')
			continue;
		if (strstr(mapbuf_b, "/SYSV")) {
			shm_minor = dev_minor;
			break;
		}
	}

	if (shmdt(addr))
		perror(_("shared memory detach"));

 out_destroy:
	if (shmctl(shmid, IPC_RMID, NULL))
		perror(_("shared memory remove"));

	return;
}

static char *mapping_name(proc_t * p, unsigned KLONG addr,
				unsigned KLONG len, const char *mapbuf_b,
				unsigned showpath, unsigned dev_major,
				unsigned dev_minor, unsigned long long inode)
{
	char *cp;

	if (!dev_major && dev_minor == shm_minor && strstr(mapbuf_b, "/SYSV")) {
		static char shmbuf[64];
		snprintf(shmbuf, sizeof shmbuf, "  [ shmid=0x%llx ]", inode);
		return shmbuf;
	}

	cp = strrchr(mapbuf_b, '/');
	if (cp) {
		if (showpath)
			return strchr(mapbuf_b, '/');
		return cp[1] ? cp + 1 : cp;
	}

	cp = strchr(mapbuf_b, '/');
	if (cp) {
		if (showpath)
			return cp;
		/* it WILL succeed */
		return strrchr(cp, '/') + 1;
	}

	cp = _("  [ anon ]");
	if ((p->start_stack >= addr) && (p->start_stack <= addr + len))
		cp = _("  [ stack ]");
	return cp;
}


#define DETAIL_LENGTH 32
#define DETL "31"		/* for format strings */
#define NUM_LENGTH 21		/* python says: len(str(2**64)) == 20 */
#define NUML "20"		/* for format strings */
#define VMFLAGS_LENGTH 81 /* There are 27 posible 2 character vmflags 
								as of this patch */

struct listnode {
	char description[DETAIL_LENGTH];
	char value_str[NUM_LENGTH];
	unsigned KLONG value;
	unsigned KLONG total;
	int max_width;
	struct listnode *next;
};

static struct listnode *listhead=NULL, *listtail=NULL;


static int is_unimportant (char *s)
{
	if (strcmp(s, "AnonHugePages") == 0) return 1;
	if (strcmp(s, "KernelPageSize") == 0) return 1;
	if (strcmp(s, "MMUPageSize") == 0) return 1;
	if (strcmp(s, "Shared_Dirty") == 0) return 1;
	if (strcmp(s, "Private_Dirty") == 0) return 1;
	if (strcmp(s, "Shared_Clean") == 0) return 1;
	if (strcmp(s, "Private_Clean") == 0) return 1;
	return 0;
}

static void print_extended_maps (FILE *f)
{
	struct listnode *listnode;
	char flags[DETAIL_LENGTH], map_desc[128],
	     detail_desc[DETAIL_LENGTH], value_str[NUM_LENGTH],
	     start[NUM_LENGTH], end[NUM_LENGTH],
	     offset[NUM_LENGTH], inode[NUM_LENGTH],
	     dev[64], fmt_str[64],
		 vmflags[VMFLAGS_LENGTH];
	int maxw1=0, maxw2=0, maxw3=0, maxw4=0, maxw5=0, maxwv=0;
	int nfields, firstmapping, footer_gap, i;
	unsigned KLONG value;
	char *ret;
	char c;
	char has_vmflags = 0;

	ret = fgets(mapbuf, sizeof mapbuf, f);
	firstmapping = 1;
	while (ret != NULL) {
		/* === READ MAPPING === */
		map_desc[0] = '\0';
		nfields = sscanf(mapbuf,
				 "%"NUML"[0-9a-f]-%"NUML"[0-9a-f] "
				 "%"DETL"s %"NUML"[0-9a-f] "
				 "%63[0-9a-f:] %"NUML"s %127[^\n]%c",
				 start, end, flags, offset,
				 dev, inode, map_desc, &c);
		/* Must read at least up to inode, else something has changed! */
		if (nfields < 6)
			xerrx(EXIT_FAILURE, _("Unknown format in smaps file!"));
		/* If line too long we dump everything else. */
		while (c != '\n') {
			ret = fgets(mapbuf, sizeof mapbuf, f);
			c = mapbuf[strlen(mapbuf) - 1];
		}
		/* Store maximum widths for printing nice later */
		if (strlen(start) > maxw1) 	maxw1 = strlen(start);
		if (strlen(flags) > maxw2) 	maxw2 = strlen(flags);
		if (strlen(offset) > maxw3) 	maxw3 = strlen(offset);
		if (strlen(dev) > maxw4) 	maxw4 = strlen(dev);
		if (strlen(inode) > maxw5) 	maxw5 = strlen(inode);

		if (7 > maxw5) 	maxw5 = 7;

		ret = fgets(mapbuf, sizeof mapbuf, f);
		nfields = sscanf(mapbuf, "%"DETL"[^:]: %"NUML"[0-9] kB %c",
				 detail_desc, value_str, &c);
		listnode = listhead;
		/* === READ MAPPING DETAILS === */
		while (ret != NULL && nfields == 2) {

			if (X_option < 2 && is_unimportant(detail_desc))
				goto loop_end;
			/* === CREATE LIST AND FILL description FIELD === */
			if (listnode == NULL) {
				assert(firstmapping == 1);
				listnode = calloc(1, sizeof *listnode);
				if (listhead == NULL) {
					assert(listtail == NULL);
					listhead = listnode;
				} else {
					listtail->next = listnode;
				}
				listtail = listnode;
				/* listnode was calloc()ed so all fields are already NULL! */
				strcpy(listnode->description, detail_desc);
				if (strlen(detail_desc) > 7)
					listnode->max_width = strlen(detail_desc);
				else
					listnode->max_width = 7;
			} else {
			/* === LIST EXISTS  === */
				if ((listnode == NULL) ||
				    (strcmp(listnode->description, detail_desc) != 0))
					xerrx(EXIT_FAILURE, "ERROR: %s %s",
					      _("inconsistent detail field in smaps file, line:\n"),
					      mapbuf);
			}
			strcpy(listnode->value_str, value_str);
			sscanf(value_str, "%"KLF"u", &listnode->value);
			listnode->total += listnode->value;
			if (strlen(value_str) > listnode->max_width)
				listnode->max_width = strlen(value_str);
			listnode = listnode->next;
loop_end:
			ret = fgets(mapbuf, sizeof mapbuf, f);
			nfields = sscanf(mapbuf, "%"DETL"[^:]: %"NUML"[0-9] kB %c",
					 detail_desc, value_str, &c);
		}

		/* === GET VMFLAGS === */
		nfields = sscanf(mapbuf, "VmFlags: %[a-z ]", vmflags);
		if (nfields == 1) {
			if (! has_vmflags) has_vmflags = 1;
			ret = fgets(mapbuf, sizeof mapbuf, f);
			if (strlen(vmflags) > maxwv) maxwv = strlen(vmflags);
		}

		/* === PRINT THIS MAPPING === */
		/* Print header */
		if (firstmapping && !q_option) {
			if (strlen("Address") > maxw1) 	maxw1 = strlen("Address");
			if (strlen("Flags") > maxw2) 	maxw2 = strlen("Flags");
			if (strlen("Offset") > maxw3) 	maxw3 = strlen("Offset");
			if (strlen("Device") > maxw4) 	maxw4 = strlen("Device");
			if (strlen("Inode") > maxw5) 	maxw5 = strlen("Inode");
			if (has_vmflags && strlen("VmFlags") > maxwv)	maxwv = strlen("VmFlags");
			sprintf(fmt_str, "%%%ds %%%ds %%%ds %%%ds %%%ds",
				maxw1, maxw2, maxw3, maxw4, maxw5);
			printf(fmt_str, "Address", "Flags", "Offset", "Device", "Inode");

			for (listnode=listhead; listnode=listnode->next;
			     listnode!=NULL) {
				sprintf(fmt_str, " %%%ds", listnode->max_width);
				printf(fmt_str, listnode->description);
			}

			if (has_vmflags) {
				sprintf(fmt_str, " %%%ds", maxwv);
				printf(fmt_str, "VmFlags");
			}

			printf(" %s\n", "Description");
		}
		/* Print data */
		sprintf(fmt_str, "%%%ds %%%ds %%%ds %%%ds %%%ds",
			maxw1, maxw2, maxw3, maxw4, maxw5);
		printf(fmt_str, start, flags, offset, dev, inode);

		for (listnode=listhead; listnode=listnode->next;
		     listnode!=NULL) {
			sprintf(fmt_str, " %%%ds", listnode->max_width);
			printf(fmt_str, listnode->value_str);
		}

		if (has_vmflags) {
			sprintf(fmt_str, " %%%ds", maxwv);
			printf(fmt_str, vmflags);
		}

		printf(" %s\n", map_desc);

		firstmapping = 0;
	}
	/* === PRINT TOTALS === */
	if (!q_option && listhead!=NULL) {
		footer_gap = maxw1+maxw2+maxw3+maxw4+maxw5+5;
		for (i=0; i<footer_gap; i++)
			putc(' ', stdout);
		for (listnode=listhead; listnode=listnode->next;
		     listnode!=NULL) {
			for (i=0; i<listnode->max_width; i++)
				putc('=', stdout);
			putc(' ', stdout);
		}
		putc('\n', stdout);
		for (i=0; i<footer_gap; i++)
			putc(' ', stdout);
		for (listnode=listhead; listnode=listnode->next;
		     listnode!=NULL) {
			sprintf(fmt_str, "%%%dd ", listnode->max_width);
			printf(fmt_str, listnode->total);
		}
		fputs("KB \n", stdout);
	}
	/* We don't free() the list, it's used for all PIDs passed as arguments */
}

static int one_proc(proc_t * p)
{
	char buf[32];
	FILE *fp;
	unsigned long total_shared = 0ul;
	unsigned long total_private_readonly = 0ul;
	unsigned long total_private_writeable = 0ul;
    KLONG diff = 0;

	const char *cp2 = NULL;
	unsigned long long rss = 0ull;
	unsigned long long private_dirty = 0ull;
	unsigned long long shared_dirty = 0ull;
	unsigned long long total_rss = 0ull;
	unsigned long long total_private_dirty = 0ull;
	unsigned long long total_shared_dirty = 0ull;

	/* Overkill, but who knows what is proper? The "w" prog uses
	 * the tty width to determine this.
	 */
	int maxcmd = 0xfffff;

	if (x_option || X_option) {
		sprintf(buf, "/proc/%u/smaps", p->tgid);
		if ((fp = fopen(buf, "r")) == NULL)
			return 1;
	} else {
		sprintf(buf, "/proc/%u/maps", p->tgid);
		if ((fp = fopen(buf, "r")) == NULL)
			return 1;
	}

	escape_command(cmdbuf, p, sizeof cmdbuf, &maxcmd,
		       ESC_ARGS | ESC_BRACKETS);
	printf("%u:   %s\n", p->tgid, cmdbuf);

	if (X_option) {
		print_extended_maps(fp);
		return 0;
	}

	if (!q_option && (x_option | d_option)) {
		if (x_option) {
			if (sizeof(KLONG) == 4)
				/* Translation Hint: Please keep
				 * alignment of the following four
				 * headers intact. */
				printf
				    (_("Address   Kbytes     RSS   Dirty Mode   Mapping\n"));
			else
				printf
				    (_("Address           Kbytes     RSS   Dirty Mode   Mapping\n"));
		}
		if (d_option) {
			if (sizeof(KLONG) == 4)
				printf
				    (_("Address   Kbytes Mode  Offset           Device    Mapping\n"));
			else
				printf
				    (_("Address           Kbytes Mode  Offset           Device    Mapping\n"));
		}
	}

	while (fgets(mapbuf, sizeof mapbuf, fp)) {
		char flags[32];
		/* to clean up unprintables */
		char *tmp;
		unsigned KLONG start, end;
		unsigned long long file_offset, inode;
		unsigned dev_major, dev_minor;
		unsigned long long smap_value;
		char smap_key[20];

		/* hex values are lower case or numeric, keys are upper */
		if (mapbuf[0] >= 'A' && mapbuf[0] <= 'Z') {
			/* Its a key */
			if (sscanf
			    (mapbuf, "%20[^:]: %llu", smap_key,
			     &smap_value) == 2) {
				if (strncmp("Rss", smap_key, 3) == 0) {
					rss = smap_value;
					total_rss += smap_value;
					continue;
				}
				if (strncmp("Shared_Dirty", smap_key, 12) == 0) {
					shared_dirty = smap_value;
					total_shared_dirty += smap_value;
					continue;
				}
				if (strncmp("Private_Dirty", smap_key, 13) == 0) {
					private_dirty = smap_value;
					total_private_dirty += smap_value;
					continue;
				}
				if (strncmp("Swap", smap_key, 4) == 0) {
					/*doesnt matter as long as last */
					printf((sizeof(KLONG) == 8)
					       ? "%016" KLF
					       "x %7lu %7llu %7llu %s  %s\n" :
					       "%08lx %7lu %7llu %7llu %s  %s\n",
					       start,
					       (unsigned long)(diff >> 10), rss,
					       (private_dirty + shared_dirty),
					       flags, cp2);
					/* reset some counters */
					rss = shared_dirty = private_dirty =
					    0ull;
                    diff = 0;
					continue;
				}
				/* Other keys */
				continue;
			}
		}
		sscanf(mapbuf, "%" KLF "x-%" KLF "x %31s %llx %x:%x %llu", &start,
		       &end, flags, &file_offset, &dev_major, &dev_minor,
		       &inode);

		if (end - 1 < range_low)
			continue;
		if (range_high < start)
			break;

		tmp = strchr(mapbuf, '\n');
		if (tmp)
			*tmp = '\0';
		tmp = mapbuf;
		while (*tmp) {
			if (!isprint(*tmp))
				*tmp = '?';
			tmp++;
		}

		diff = end - start;
		if (flags[3] == 's')
			total_shared += diff;
		if (flags[3] == 'p') {
			flags[3] = '-';
			if (flags[1] == 'w')
				total_private_writeable += diff;
			else
				total_private_readonly += diff;
		}
		/* format used by Solaris 9 and procps-3.2.0+ an 'R'
		 * if swap not reserved (MAP_NORESERVE, SysV ISM
		 * shared mem, etc.)
		 */
		flags[4] = '-';
		flags[5] = '\0';

		if (x_option) {
			cp2 =
			    mapping_name(p, start, diff, mapbuf, 0, dev_major,
					 dev_minor, inode);
			/* printed with the keys */
			continue;
		}
		if (d_option) {
			const char *cp =
			    mapping_name(p, start, diff, mapbuf, 0, dev_major,
					 dev_minor, inode);
			printf((sizeof(KLONG) == 8)
			       ? "%016" KLF "x %7lu %s %016llx %03x:%05x %s\n"
			       : "%08lx %7lu %s %016llx %03x:%05x %s\n",
			       start,
			       (unsigned long)(diff >> 10),
			       flags, file_offset, dev_major, dev_minor, cp);
		}
		if (!x_option && !d_option) {
			const char *cp =
			    mapping_name(p, start, diff, mapbuf, 1, dev_major,
					 dev_minor, inode);
			printf((sizeof(KLONG) == 8)
			       ? "%016" KLF "x %6luK %s  %s\n"
			       : "%08lx %6luK %s  %s\n",
			       start, (unsigned long)(diff >> 10), flags, cp);
		}

	}
    fclose(fp);
	if (!q_option) {
		if (x_option) {
			if (sizeof(KLONG) == 8) {
				printf
				    ("----------------  ------  ------  ------\n");
				printf(_("total kB %15ld %7llu %7llu\n"),
				       (total_shared + total_private_writeable +
					total_private_readonly) >> 10,
				       total_rss,
				       (total_shared_dirty +
					total_private_dirty)

				    );
			} else {
				printf
				    ("-------- ------- ------- ------- -------\n");
				printf
				    (_("total kB %7ld %7llu %7llu -\n"),
				     (total_shared + total_private_writeable +
				      total_private_readonly) >> 10,
					 total_rss, (total_shared_dirty+total_private_dirty));

			}
		}
		if (d_option) {
			printf
			    (_("mapped: %ldK    writeable/private: %ldK    shared: %ldK\n"),
			     (total_shared + total_private_writeable +
			      total_private_readonly) >> 10,
			     total_private_writeable >> 10, total_shared >> 10);
		}
		if (!x_option && !d_option) {
			if (sizeof(KLONG) == 8)
				/* Translation Hint: keep total string length
				 * as 24 characters. Adjust %16 if needed*/
				printf(_(" total %16ldK\n"),
				       (total_shared + total_private_writeable +
					total_private_readonly) >> 10);
			else
				/* Translation Hint: keep total string length
				 * as 16 characters. Adjust %8 if needed*/
				printf(_(" total %8ldK\n"),
				       (total_shared + total_private_writeable +
					total_private_readonly) >> 10);
		}
	}

	return 0;
}

static void range_arguments(char *optarg)
{
	char *arg1;
	char *arg2;

	arg1 = xstrdup(optarg);
	arg2 = strchr(arg1, ',');
	if (arg2)
		*arg2 = '\0';
	if (arg2)
		++arg2;
	else
		arg2 = arg1;
	if (arg1 && *arg1)
		range_low = STRTOUKL(arg1, &arg1, 16);
	if (*arg2)
		range_high = STRTOUKL(arg2, &arg2, 16);
	if (arg1 && (*arg1 || *arg2))
		xerrx(EXIT_FAILURE, "%s: '%s'", _("failed to parse argument"),
		      optarg);
}

int main(int argc, char **argv)
{
	unsigned long *pidlist;
	unsigned count = 0;
	PROCTAB *PT;
	proc_t p;
	int ret = 0, c;

	static const struct option longopts[] = {
		{"extended", no_argument, NULL, 'x'},
		{"device", no_argument, NULL, 'd'},
		{"quiet", no_argument, NULL, 'q'},
		{"range", required_argument, NULL, 'A'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

    program_invocation_name = program_invocation_short_name;
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	x_option = d_option = q_option = 0;

	while ((c = getopt_long(argc, argv, "xXrdqA:hV", longopts, NULL)) != -1)
		switch (c) {
		case 'x':
			x_option = 1;
			break;
		case 'X':
			X_option++;
			break;
		case 'r':
			xwarnx(_("option -r is ignored as SunOS compatibility"));
			break;
		case 'd':
			d_option = 1;
			break;
		case 'q':
			q_option = 1;
			break;
		case 'A':
			range_arguments(optarg);
			break;
		case 'h':
			usage(stdout);
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		case 'a':	/* Sun prints anon/swap reservations */
		case 'F':	/* Sun forces hostile ptrace-like grab */
		case 'l':	/* Sun shows unresolved dynamic names */
		case 'L':	/* Sun shows lgroup info */
		case 's':	/* Sun shows page sizes */
		case 'S':	/* Sun shows swap reservations */
		default:
			usage(stderr);
		}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		xerrx(EXIT_FAILURE, _("argument missing"));
	if (d_option && (x_option || X_option) ||
	    x_option && (d_option || X_option))
		xerrx(EXIT_FAILURE, _("options -d, -x, -X are mutually exclusive"));

	pidlist = xmalloc(sizeof(unsigned long) * argc);

	while (*argv) {
		char *walk = *argv++;
		char *endp;
		unsigned long pid;
		if (!strncmp("/proc/", walk, 6)) {
			walk += 6;
			/*  user allowed to do: pmap /proc/PID */
			if (*walk < '0' || *walk > '9')
				continue;
		}
		if (*walk < '0' || *walk > '9')
			usage(stderr);
		pid = strtoul(walk, &endp, 0);
		if (pid < 1ul || pid > 0x7ffffffful || *endp)
			usage(stderr);
		pidlist[count++] = pid;
	}

	discover_shm_minor();

	memset(&p, '\0', sizeof(p));
	/* old libproc interface is zero-terminated */
	pidlist[count] = 0;
	PT = openproc(PROC_FILLSTAT | PROC_FILLARG | PROC_PID, pidlist);
	while (readproc(PT, &p)) {
		ret |= one_proc(&p);
		count--;
	}
	closeproc(PT);
	free(pidlist);

	if (count)
		/* didn't find all processes asked for */
		ret |= 42;
	return ret;
}
