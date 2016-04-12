/*
 * skill.c - send a signal to process
 * Copyright 1998-2002 by Albert Cahalan
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "fileutils.h"
#include "nsutils.h"
#include "strutils.h"
#include "nls.h"
#include "xalloc.h"
#include "proc/pwcache.h"
#include "proc/sig.h"
#include "proc/devname.h"
#include "proc/procps.h"	/* char *user_from_uid(uid_t uid) */
#include "proc/readproc.h"
#include "proc/version.h"	/* procps_version */
#include "rpmatch.h"

#define DEFAULT_NICE 4

struct run_time_conf_t {
	int fast;
	int interactive;
	int verbose;
	int warnings;
	int noaction;
	int debugging;
};
static int tty_count, uid_count, cmd_count, pid_count, namespace_count;
static int *ttys;
static uid_t *uids;
static const char **cmds;
static int *pids;
static char **namespaces;
static int ns_pid;
static proc_t ns_task;

#define ENLIST(thing,addme) do{ \
if(!thing##s) thing##s = xmalloc(sizeof(*thing##s)*saved_argc); \
thing##s[thing##_count++] = addme; \
}while(0)

static int my_pid;
static int saved_argc;

static int sig_or_pri;

enum {
	PROG_UNKNOWN,
	PROG_KILL,
	PROG_SKILL,
	PROG_SNICE
};
static int program = PROG_UNKNOWN;

static void display_kill_version(void)
{
	fprintf(stdout, PROCPS_NG_VERSION);
}

static int ns_flags = 0x3f;
static int parse_namespaces(char *optarg)
{
	char *ptr = optarg, *tmp;
	int len, id;

	ns_flags = 0;
	while (1) {
		if (strchr(ptr, ',') == NULL) {
			len = -1;
			tmp = strdup(ptr);
		} else {
			len = strchr(ptr, ',') - ptr;
			tmp = strndup(ptr, len);
		}

		id = get_ns_id(tmp);
		if (id == -1) {
			fprintf(stderr, "%s is not a valid namespace\n", tmp);
			free(tmp);
			return 1;
		}
		ns_flags |= (1 << id);
		ENLIST(namespace, tmp);

		if (len == -1)
			break;

		ptr+= len + 1;
	}
	return 0;
}

/* kill or nice a process */
static void hurt_proc(int tty, int uid, int pid, const char *restrict const cmd,
		      struct run_time_conf_t *run_time)
{
	int failed;
	char dn_buf[1000];
	dev_to_tty(dn_buf, 999, tty, pid, ABBREV_DEV);
	if (run_time->interactive) {
		char *buf;
		size_t len = 0;
		fprintf(stderr, "%-8s %-8s %5d %-16.16s   ? ",
			(char *)dn_buf, user_from_uid(uid), pid, cmd);
		fflush (stdout);
		if (getline(&buf, &len, stdin) == -1)
			return;
		if (rpmatch(buf) < 1) {
			free(buf);
			return;
		}
		free(buf);
	}
	/* do the actual work */
	errno = 0;
	if (program == PROG_SKILL)
		failed = kill(pid, sig_or_pri);
	else
		failed = setpriority(PRIO_PROCESS, pid, sig_or_pri);
	if ((run_time->warnings && failed) || run_time->debugging || run_time->verbose) {
		fprintf(stderr, "%-8s %-8s %5d %-16.16s   ",
			(char *)dn_buf, user_from_uid(uid), pid, cmd);
		perror("");
		return;
	}
	if (run_time->interactive)
		return;
	if (run_time->noaction) {
		printf("%d\n", pid);
		return;
	}
}

/* check one process */
static void check_proc(int pid, struct run_time_conf_t *run_time)
{
	char buf[128];
	struct stat statbuf;
	proc_t task;
	char *tmp;
	int tty;
	int fd;
	int i;
	if (pid == my_pid || pid == 0)
		return;
	/* pid (cmd) state ppid pgrp session tty */
	sprintf(buf, "/proc/%d/stat", pid);
	fd = open(buf, O_RDONLY);
	if (fd == -1) {
		/* process exited maybe */
		if (run_time->warnings)
			xwarn(_("cannot open file %s"), buf);
		return;
	}
	fstat(fd, &statbuf);
	if (uids) {
		/* check the EUID */
		i = uid_count;
		while (i--)
			if (uids[i] == statbuf.st_uid)
				break;
		if (i == -1)
			goto closure;
	}
	if (read(fd, buf, 128) <= 0)
	    goto closure;
	buf[127] = '\0';
	tmp = strrchr(buf, ')');
	*tmp++ = '\0';
	i = 5;
	while (i--)
		while (*tmp++ != ' ')
			/* scan to find tty */ ;
	tty = atoi(tmp);
	if (ttys) {
		i = tty_count;
		while (i--)
			if (ttys[i] == tty)
				break;
		if (i == -1)
			goto closure;
	}
	tmp = strchr(buf, '(') + 1;
	if (cmds) {
		i = cmd_count;
		/* fast comparison trick -- useful? */
		while (i--)
			if (cmds[i][0] == *tmp && !strcmp(cmds[i], tmp))
				break;
		if (i == -1)
			goto closure;
	}
	if (ns_pid) {
		if (ns_read(pid, &task))
			goto closure;
		for (i = 0; i < NUM_NS; i++) {
			if (ns_flags & (1 << i)) {
				if (task.ns[i] != ns_task.ns[i])
					goto closure;
			}
		}
	}
	/* This is where we kill/nice something. */
	/* for debugging purposes?
	fprintf(stderr, "PID %d, UID %d, TTY %d,%d, COMM %s\n",
		pid, statbuf.st_uid, tty >> 8, tty & 0xf, tmp);
	*/
	hurt_proc(tty, statbuf.st_uid, pid, tmp, run_time);
 closure:
	/* kill/nice _first_ to avoid PID reuse */
	close(fd);
}

/* debug function */
static void show_lists(void)
{
	int i;

	fprintf(stderr, "signal: %d\n", sig_or_pri);

	fprintf(stderr, "%d TTY: ", tty_count);
	if (ttys) {
		i = tty_count;
		while (i--) {
			fprintf(stderr, "%d,%d%c", (ttys[i] >> 8) & 0xff,
				ttys[i] & 0xff, i ? ' ' : '\n');
		}
	} else
		fprintf(stderr, "\n");

	fprintf(stderr, "%d UID: ", uid_count);
	if (uids) {
		i = uid_count;
		while (i--)
			fprintf(stderr, "%d%c", uids[i], i ? ' ' : '\n');
	} else
		fprintf(stderr, "\n");

	fprintf(stderr, "%d PID: ", pid_count);
	if (pids) {
		i = pid_count;
		while (i--)
			fprintf(stderr, "%d%c", pids[i], i ? ' ' : '\n');
	} else
		fprintf(stderr, "\n");

	fprintf(stderr, "%d CMD: ", cmd_count);
	if (cmds) {
		i = cmd_count;
		while (i--)
			fprintf(stderr, "%s%c", cmds[i], i ? ' ' : '\n');
	} else
		fprintf(stderr, "\n");
}

/* iterate over all PIDs */
static void iterate(struct run_time_conf_t *run_time)
{
	int pid;
	DIR *d;
	struct dirent *de;
	if (pids) {
		pid = pid_count;
		while (pid--)
			check_proc(pids[pid], run_time);
		return;
	}
#if 0
	/* could setuid() and kill -1 to have the kernel wipe out a user */
	if (!ttys && !cmds && !pids && !run_time->interactive) {
	}
#endif
	d = opendir("/proc");
	if (!d)
		xerr(EXIT_FAILURE, "/proc");
	while ((de = readdir(d))) {
		if (de->d_name[0] > '9')
			continue;
		if (de->d_name[0] < '1')
			continue;
		pid = atoi(de->d_name);
		if (pid)
			check_proc(pid, run_time);
	}
	closedir(d);
}

/* kill help */
static void __attribute__ ((__noreturn__)) kill_usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
              _(" %s [options] <pid> [...]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" <pid> [...]            send signal to every <pid> listed\n"), out);
	fputs(_(" -<signal>, -s, --signal <signal>\n"
		"                        specify the <signal> to be sent\n"), out);
	fputs(_(" -l, --list=[<signal>]  list all signal names, or convert one to a name\n"), out);
	fputs(_(" -L, --table            list all signal names in a nice table\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("kill(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* skill and snice help */
static void __attribute__ ((__noreturn__)) skillsnice_usage(FILE * out)
{
	fputs(USAGE_HEADER, out);

	if (program == PROG_SKILL) {
		fprintf(out,
			_(" %s [signal] [options] <expression>\n"),
			program_invocation_short_name);
	} else {
		fprintf(out,
			_(" %s [new priority] [options] <expression>\n"),
			program_invocation_short_name);
	}
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -f, --fast         fast mode (not implemented)\n"), out);
	fputs(_(" -i, --interactive  interactive\n"), out);
	fputs(_(" -l, --list         list all signal names\n"), out);
	fputs(_(" -L, --table        list all signal names in a nice table\n"), out);
	fputs(_(" -n, --no-action    do not actually kill processes; just print what would happen\n"), out);
	fputs(_(" -v, --verbose      explain what is being done\n"), out);
	fputs(_(" -w, --warnings     enable warnings (not implemented)\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("Expression can be: terminal, user, pid, command.\n"
		"The options below may be used to ensure correct interpretation.\n"), out);
	fputs(_(" -c, --command <command>  expression is a command name\n"), out);
	fputs(_(" -p, --pid <pid>          expression is a process id number\n"), out);
	fputs(_(" -t, --tty <tty>          expression is a terminal\n"), out);
	fputs(_(" -u, --user <username>    expression is a username\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(_("Alternatively, expression can be:\n"), out);
	fputs(_(" --ns <pid>               match the processes that belong to the same\n"
		"                          namespace as <pid>\n"), out);
	fputs(_(" --nslist <ns,...>        list which namespaces will be considered for\n"
		"                          the --ns option; available namespaces are\n:"
	        "                          ipc, mnt, net, pid, user, uts\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	if (program == PROG_SKILL) {
		fprintf(out,
			_("\n"
			  "The default signal is TERM. Use -l or -L to list available signals.\n"
			  "Particularly useful signals include HUP, INT, KILL, STOP, CONT, and 0.\n"
			  "Alternate signals may be specified in three ways: -SIGKILL -KILL -9\n"));
		fprintf(out, USAGE_MAN_TAIL("skill(1)"));
	} else {
		fprintf(out,
			_("\n"
			  "The default priority is +4. (snice +4 ...)\n"
			  "Priority numbers range from +20 (slowest) to -20 (fastest).\n"
			  "Negative priority numbers are restricted to administrative users.\n"));
		fprintf(out, USAGE_MAN_TAIL("snice(1)"));
	}
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int skill_sig_option(int *argc, char **argv)
{
	int i;
	int signo = -1;
	for (i = 1; i < *argc; i++) {
		if (argv[i][0] == '-') {
			signo = signal_name_to_number(argv[i] + 1);
			if (-1 < signo) {
				memmove(argv + i, argv + i + 1,
					sizeof(char *) * (*argc - i));
				(*argc)--;
				return signo;
			}
		}
	}
	return signo;
}

/* kill */
static void __attribute__ ((__noreturn__))
    kill_main(int argc, char **argv)
{
	int signo, i;
	int loop = 1;
	long pid;
	int exitvalue = EXIT_SUCCESS;

	static const struct option longopts[] = {
		{"list", optional_argument, NULL, 'l'},
		{"table", no_argument, NULL, 'L'},
		{"signal", required_argument, NULL, 's'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	if (argc < 2)
		kill_usage(stderr);

	signo = skill_sig_option(&argc, argv);
	if (signo < 0)
		signo = SIGTERM;

	opterr=0; /* suppress errors on -123 */
	while (loop == 1 && (i = getopt_long(argc, argv, "l::Ls:hV", longopts, NULL)) != -1)
		switch (i) {
		case 'l':
			if (optarg) {
				char *s;
				s = strtosig(optarg);
				if (s)
					printf("%s\n", s);
				else
					xwarnx(_("unknown signal name %s"),
					      optarg);
				free(s);
			} else {
				unix_print_signals();
			}
			exit(EXIT_SUCCESS);
		case 'L':
			pretty_print_signals();
			exit(EXIT_SUCCESS);
		case 's':
			signo = signal_name_to_number(optarg);
			break;
		case 'h':
			kill_usage(stdout);
		case 'V':
			display_kill_version();
			exit(EXIT_SUCCESS);
		case '?':
			if (!isdigit(optopt)) {
				xwarnx(_("invalid argument %c"), optopt);
				kill_usage(stderr);
			} else {
			    /* Special case for signal digit negative
			     * PIDs */
			    pid = atoi(argv[optind]);
			    if (kill((pid_t)pid, signo) != 0)
				exitvalue = EXIT_FAILURE;
			    exit(exitvalue);
			}
			loop=0;
			break;
		default:
			kill_usage(stderr);
		}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		kill_usage(stderr);

	for (i = 0; i < argc; i++) {
		pid = strtol_or_err(argv[i], _("failed to parse argument"));
		if (!kill((pid_t) pid, signo))
			continue;
		exitvalue = EXIT_FAILURE;
		continue;
	}

	exit(exitvalue);
}

#if 0
static void _skillsnice_usage(int line)
{
	fprintf(stderr, _("something at line %d\n"), line);
	skillsnice_usage(stderr);
}

#define skillsnice_usage() _skillsnice_usage(__LINE__)
#endif

#define NEXTARG (argc?( argc--, ((argptr=*++argv)) ):NULL)

/* common skill/snice argument parsing code */

static int snice_prio_option(int *argc, char **argv)
{
	int i = 1, nargs = *argc;
	long prio = DEFAULT_NICE;

	while (i < nargs) {
		if ((argv[i][0] == '-' || argv[i][0] == '+')
		    && isdigit(argv[i][1])) {
			prio = strtol_or_err(argv[i],
					     _("failed to parse argument"));
			if (prio < INT_MIN || INT_MAX < prio)
				xerrx(EXIT_FAILURE,
				     _("priority %lu out of range"), prio);
			nargs--;
			if (nargs - i)
				memmove(argv + i, argv + i + 1,
					sizeof(char *) * (nargs - i));
		} else
			i++;
	}
	*argc = nargs;
	return (int)prio;
}

static void skillsnice_parse(int argc,
			     char **argv, struct run_time_conf_t *run_time)
{
	int signo = -1;
	int prino = DEFAULT_NICE;
	int ch, i;

	enum {
		NS_OPTION = CHAR_MAX + 1,
		NSLIST_OPTION,
	};

	static const struct option longopts[] = {
		{"command", required_argument, NULL, 'c'},
		{"debug", no_argument, NULL, 'd'},
		{"fast", no_argument, NULL, 'f'},
		{"interactive", no_argument, NULL, 'i'},
		{"list", no_argument, NULL, 'l'},
		{"no-action", no_argument, NULL, 'n'},
		{"pid", required_argument, NULL, 'p'},
		{"table", no_argument, NULL, 'L'},
		{"tty", required_argument, NULL, 't'},
		{"user", required_argument, NULL, 'u'},
		{"ns", required_argument, NULL, NS_OPTION},
		{"nslist", required_argument, NULL, NSLIST_OPTION},
		{"verbose", no_argument, NULL, 'v'},
		{"warnings", no_argument, NULL, 'w'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	if (argc < 2)
		skillsnice_usage(stderr);

	sig_or_pri = -1;

	if (program == PROG_SNICE)
		prino = snice_prio_option(&argc, argv);
	else if (program == PROG_SKILL) {
		signo = skill_sig_option(&argc, argv);
		if (-1 < signo)
			sig_or_pri = signo;
	}

	pid_count = 0;

	while ((ch =
		getopt_long(argc, argv, "c:dfilnp:Lt:u:vwhV", longopts,
			    NULL)) != -1)
		switch (ch) {
		case 'c':
			ENLIST(cmd, optarg);
			break;
		case 'd':
			run_time->debugging = 1;
			break;
		case 'f':
			run_time->fast = 1;
			break;
		case 'i':
			run_time->interactive = 1;
			break;
		case 'l':
			unix_print_signals();
			exit(EXIT_SUCCESS);
		case 'n':
			run_time->noaction = 1;
			break;
		case 'p':
			ENLIST(pid,
			       strtol_or_err(optarg,
					     _("failed to parse argument")));
			pid_count++;
			break;
		case 'L':
			pretty_print_signals();
			exit(EXIT_SUCCESS);
		case 't':
			{
				struct stat sbuf;
				char path[32];
				snprintf(path, 32, "/dev/%s", optarg);
				if (stat(path, &sbuf) >= 0
				    && S_ISCHR(sbuf.st_mode)) {
					ENLIST(tty, sbuf.st_rdev);
				}
			}
			break;
		case 'u':
			{
				struct passwd *passwd_data;
				passwd_data = getpwnam(optarg);
				if (passwd_data) {
					ENLIST(uid, passwd_data->pw_uid);
				}
			}
			break;
		case NS_OPTION:
			ns_pid = atoi(optarg);
			if (ns_pid == 0) {
				xwarnx(_("invalid pid number %s"), optarg);
				kill_usage(stderr);
			}
			if (ns_read(ns_pid, &ns_task)) {
				xwarnx(_("error reading reference namespace "
					 "information"));
				kill_usage(stderr);
			}

			break;
		case NSLIST_OPTION:
			if (parse_namespaces(optarg)) {
				xwarnx(_("invalid namespace list"));
				kill_usage(stderr);
			}
			break;
		case 'v':
			run_time->verbose = 1;
			break;
		case 'w':
			run_time->warnings = 1;
			break;
		case 'h':
			skillsnice_usage(stdout);
		case 'V':
			display_kill_version();
			exit(EXIT_SUCCESS);
		default:
			skillsnice_usage(stderr);
		}

	argc -= optind;
	argv += optind;

	for (i = 0; i < argc; i++) {
		long num;
		char *end = NULL;
		errno = 0;
		num = strtol(argv[0], &end, 10);
		if (errno == 0 && argv[0] != end && end != NULL && *end == '\0') {
			ENLIST(pid, num);
			pid_count++;
		} else {
			ENLIST(cmd, argv[0]);
		}
		argv++;
	}

	/* No more arguments to process. Must sanity check. */
	if (!tty_count && !uid_count && !cmd_count && !pid_count && !ns_pid)
		xerrx(EXIT_FAILURE, _("no process selection criteria"));
	if ((run_time->fast | run_time->interactive | run_time->
	     verbose | run_time->warnings | run_time->noaction) & ~1)
		xerrx(EXIT_FAILURE, _("general flags may not be repeated"));
	if (run_time->interactive
	    && (run_time->verbose | run_time->fast | run_time->noaction))
		xerrx(EXIT_FAILURE, _("-i makes no sense with -v, -f, and -n"));
	if (run_time->verbose && (run_time->interactive | run_time->fast))
		xerrx(EXIT_FAILURE, _("-v makes no sense with -i and -f"));
	if (run_time->noaction) {
		program = PROG_SKILL;
		/* harmless */
		sig_or_pri = 0;
	}
	if (program == PROG_SNICE)
		sig_or_pri = prino;
	else if (sig_or_pri < 0)
		sig_or_pri = SIGTERM;
}

/* main body */
int main(int argc, char ** argv)
{
#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	struct run_time_conf_t run_time;
	memset(&run_time, 0, sizeof(struct run_time_conf_t));
	my_pid = getpid();

	if (strcmp(program_invocation_short_name, "kill") == 0 ||
	    strcmp(program_invocation_short_name, "lt-kill") == 0)
		program = PROG_KILL;
	else if (strcmp(program_invocation_short_name, "skill") == 0 ||
		 strcmp(program_invocation_short_name, "lt-skill") == 0)
		program = PROG_SKILL;
	else if (strcmp(program_invocation_short_name, "snice") == 0 ||
		 strcmp(program_invocation_short_name, "lt-snice") == 0)
		program = PROG_SNICE;

	switch (program) {
	case PROG_SNICE:
	case PROG_SKILL:
		setpriority(PRIO_PROCESS, my_pid, -20);
		skillsnice_parse(argc, argv, &run_time);
		if (run_time.debugging)
			show_lists();
		iterate(&run_time);
		break;
	case PROG_KILL:
		kill_main(argc, argv);
		break;
	default:
		fprintf(stderr, _("skill: \"%s\" is not supported\n"),
			program_invocation_short_name);
		fprintf(stderr, USAGE_MAN_TAIL("skill(1)"));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
