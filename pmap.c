/*
 * Copyright 2002 by Albert Cahalan; all rights reserved.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */

#include <ctype.h>
#include <err.h>
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

#include "c.h"
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
	fputs(_("  -x, --extended              show details\n"), out);
	fputs(_("  -d, --device                show the device format\n"), out);
	fputs(_("  -q, --quiet                 do not display header and footer\n"), out);
	fputs(_("  -A, --range=<low>[,<high>]  limit results to the given range\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("pmap(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static unsigned KLONG range_low;
static unsigned KLONG range_high = ~0ull;

static int d_option;
static int q_option;
static int x_option;

static unsigned shm_minor = ~0u;

static void discover_shm_minor(void)
{
	void *addr;
	int shmid;
	char mapbuf[256];

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

	while (fgets(mapbuf, sizeof mapbuf, stdin)) {
		char flags[32];
		/* to clean up unprintables */
		char *tmp;
		unsigned KLONG start, end;
		unsigned long long file_offset, inode;
		unsigned dev_major, dev_minor;
		sscanf(mapbuf, "%" KLF "x-%" KLF "x %31s %Lx %x:%x %Lu", &start,
		       &end, flags, &file_offset, &dev_major, &dev_minor,
		       &inode);
		tmp = strchr(mapbuf, '\n');
		if (tmp)
			*tmp = '\0';
		tmp = mapbuf;
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
		if (strstr(mapbuf, "/SYSV")) {
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

static const char *mapping_name(proc_t * p, unsigned KLONG addr,
				unsigned KLONG len, const char *mapbuf,
				unsigned showpath, unsigned dev_major,
				unsigned dev_minor, unsigned long long inode)
{
	const char *cp;

	if (!dev_major && dev_minor == shm_minor && strstr(mapbuf, "/SYSV")) {
		static char shmbuf[64];
		snprintf(shmbuf, sizeof shmbuf, "  [ shmid=0x%Lx ]", inode);
		return shmbuf;
	}

	cp = strrchr(mapbuf, '/');
	if (cp) {
		if (showpath)
			return strchr(mapbuf, '/');
		return cp[1] ? cp + 1 : cp;
	}

	cp = strchr(mapbuf, '/');
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

static int one_proc(proc_t * p)
{
	char buf[32];
	char mapbuf[9600];
	char cmdbuf[512];
	FILE *fp;
	unsigned long total_shared = 0ul;
	unsigned long total_private_readonly = 0ul;
	unsigned long total_private_writeable = 0ul;
    KLONG diff = 0;

	char *cp2 = NULL;
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

	sprintf(buf, "/proc/%u/maps", p->tgid);
	if ((fp = fopen(buf, "r")) == NULL)
		return 1;
	if (x_option) {
		sprintf(buf, "/proc/%u/smaps", p->tgid);
		if ((fp = freopen(buf, "r", fp)) == NULL)
			return 1;
	}

	escape_command(cmdbuf, p, sizeof cmdbuf, &maxcmd,
		       ESC_ARGS | ESC_BRACKETS);
	printf("%u:   %s\n", p->tgid, cmdbuf);

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
		sscanf(mapbuf, "%" KLF "x-%" KLF "x %31s %Lx %x:%x %Lu", &start,
		       &end, flags, &file_offset, &dev_major, &dev_minor,
		       &inode);

		if (start > range_high)
			break;
		if (end < range_low)
			continue;

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
			       ? "%016" KLF "x %7lu %s %016Lx %03x:%05x %s\n"
			       : "%08lx %7lu %s %016Lx %03x:%05x %s\n",
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
				    (_("total kB %7ld       -       -       -\n"),
				     (total_shared + total_private_writeable +
				      total_private_readonly) >> 10);
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

int main(int argc, char **argv)
{
	unsigned *pidlist;
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

	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	x_option = d_option = q_option = 0;

	while ((c = getopt_long(argc, argv, "xrdqA:hV", longopts, NULL)) != -1)
		switch (c) {
		case 'x':
			x_option = 1;
			break;
		case 'r':
			warnx(_("option -r is ignored as SunOS compatibility"));
			break;
		case 'd':
			d_option = 1;
			break;
		case 'q':
			q_option = 1;
			break;
		case 'A':
			{
				/* FIXME: this should be a function. */
				char *walk = optarg;
				char *arg1;
				char *arg2;
				if (walk[1]) {
					arg1 = walk + 1;
					walk += strlen(walk) - 1;
				} else {
					arg1 = *++argv;
					if (!arg1)
						usage(stderr);
				}
				arg2 = strchr(arg1, ',');
				if (arg2)
					*arg2 = '\0';
				if(arg2) ++arg2;
				else arg2 = arg1;

				if (*arg1)
					range_low = STRTOUKL(arg1, &arg1, 16);
				if (*arg2)
					range_high = STRTOUKL(arg2, &arg2, 16);
				if (*arg1 || *arg2)
					usage(stderr);
			}
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
		errx(EXIT_FAILURE, _("argument missing"));
	if (d_option && x_option)
		errx(EXIT_FAILURE, _("options -d and -x cannot coexist"));

	pidlist = xmalloc(sizeof(unsigned) * argc);

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

	if (count)
		/* didn't find all processes asked for */
		ret |= 42;
	return ret;
}
