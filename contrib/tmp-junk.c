/*
 * w.c - show logged users and what they are doing
 *
 * Copyright (c) Dec 1993, Oct 1994 Steve "Mr. Bassman" Bryant
 * 		bassman@hpbbi30.bbn.hp.com (Old address)
 *		bassman@muttley.soc.staffs.ac.uk
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

/* An alternative "w" program for Linux.
 * Shows users and their processes.
 *
 * Info:
 *	I starting writing as an improvement of the w program included
 * with linux. The idea was to add in some extra functionality to the
 * program, and see if I could fix a couple of bugs which seemed to
 * occur.
 *						Mr. Bassman, 10/94
 *
 * Acknowledgments:
 *
 * The original version of w:
 *	Copyright (c) 1993 Larry Greenfield  (greenfie@gauss.rutgers.edu)
 *
 * Uptime routine and w mods:
 *	Michael K. Johnson  (johnsonm@stolaf.edu)
 *
 *
 * Distribution:
 *	This program is freely distributable under the terms of copyleft.
 *	No warranty, no support, use at your own risk etc.
 *
 * Compilation:
 *	gcc -O -o w sysinfo.c whattime.c w.c
 *
 * Usage:
 *	w [-hfusd] [user]
 *
 *
 * $Log: tmp-junk.c,v $
 * Revision 1.1  2002/02/01 22:46:37  csmall
 * Initial revision
 *
 * Revision 1.5  1994/10/26  17:57:35  bassman
 * Loads of stuff - see comments.
 *
 * Revision 1.4  1994/01/01  12:57:21  johnsonm
 * Added RCS, and some other fixes.
 *
 * Revision history:
 * Jan 01, 1994 (mkj):	Eliminated GCC warnings, took out unnecessary
 *			dead variables in fscanf, replacing them with
 *			*'d format qualifiers.  Also added RCS stuff.
 * Oct 26, 1994 (bass):	Tidied up the code, fixed bug involving corrupt
 *			utmp records.  Added switch for From field;
 *			default is compile-time set.  Added -d option
 *			as a remnant from BSD 'w'.  Fixed bug so it now
 *			behaves if the first process on a tty isn't owned
 *			by the person first logged in on that tty, and
 *			also detects su'd users.  Changed the tty format
 *			to the short one.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <time.h>
#include <utmp.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include "proc/whattime.h"


#define TRUE		1
#define FALSE		0
/*
 * Default setting for whether to have a From field.  The -f switch
 * toggles this - if the default is to have it, using -f will turn
 * it off; if the default is not to have it, the -f switch will put
 * it in.  Possible values are TRUE (to have the field by default),
 * and FALSE.
 */
#define DEFAULT_FROM	TRUE
#define ZOMBIE		"<zombie>"


void put_syntax();
char *idletime();
char *logintime();

static char rcsid[]="$Id: tmp-junk.c,v 1.1 2002/02/01 22:46:37 csmall Exp $";


void main (argc, argv)

int argc;
char *argv[];

{
    int header=TRUE, long_format=TRUE, ignore_user=TRUE,
	from_switch=DEFAULT_FROM, show_pid=FALSE, line_length;
    int i, j;
    struct utmp *utmp_rec;
    struct stat stat_rec;
    struct passwd *passwd_entry;
    uid_t uid;
    char username[9], tty[13], rhost[17], login_time[27];
    char idle_time[7], what[1024], pid[10];
    char out_line[1024], file_name[256];
    char search_name[9];
    int  jcpu, pcpu, tpgid, curr_pid, utime, stime, cutime, cstime;
    char /*ch,*/ state, comm[1024], *columns_ptr;
    FILE *fp;


    search_name[0] = '\0';


    /*
     * Process the command line
     */
    if (argc > 1)
    {
	/*
	 * Args that start with '-'
	 */
	for (i = 1; ((i < argc) && (argv[i][0] == '-')); i ++)
	{
	    for (j = 1; argv[i][j] != '\0'; j++)
	    {
		switch (argv[i][j])
		{
		    case 'h':
			header = FALSE;
			break;
		    case 's':
			long_format = FALSE;
			break;
		    case 'u':
			ignore_user = FALSE;
			break;
		    case 'd':
			show_pid = TRUE;
			break;
		    case 'f':
			if (DEFAULT_FROM == TRUE)
			    from_switch = FALSE;
			else
			    from_switch = TRUE;
			break;
		    default:
			fprintf (stderr, "w: unknown option: '%c'\n",
			    argv[i][j]);
			put_syntax ();
			break;
		}
	    }
	}


	/*
	 * Check for arg not starting with '-' (ie: username)
	 */
	if (argc > i)
	{
	    strncpy (search_name, argv[i], 8);
	    search_name[8] = '\0';
	    i ++;

	    if (argc > i)
	    {
		fprintf (stderr, "w: syntax error\n");
		put_syntax ();
	    }
	}
    }



    /*
     * Check that /proc is actually there, or else we can't
     * get all the information.
     */
    if (chdir ("/proc"))
    {
	fprintf (stderr, "w: fatal error: cannot access /proc\n");
	perror (strerror(errno));
	exit (-1);
    }



    /*
     * Find out our screen width from $COLUMNS
     */
    columns_ptr = getenv ("COLUMNS");
    if (columns_ptr == NULL)
    {
	struct winsize window;

	/*
	 * Try getting it directly
	 */
	if ((ioctl (1, TIOCGWINSZ, &window) != 1) && (window.ws_col > 0))
	    line_length = window.ws_col;
	else
	    line_length = 80;		/* Default length assumed */
    }
    else
	line_length = atoi (columns_ptr);

    /*
     * Maybe we should check whether there is enough space on
     * the lines for the options selected...
     */
    if (line_length < 60)
	long_format = FALSE;

    line_length --;


    /*
     * Print whatever headers
     */
    if (header == TRUE)
    {
	/*
	 * uptime: from MKJ's uptime routine,
	 * found in whattime.c
	 */
	print_uptime();


	/*
	 * Print relevant header bits
	 */
	printf ("User     tty     ");

	if (long_format == TRUE)
	{
	    if (from_switch == TRUE)
		printf ("From             ");

	    printf (" login@   idle  JCPU  PCPU  ");

	    if (show_pid == TRUE)
		printf (" PID  ");

	    printf ("what\n");
	}
	else
	{
	    printf (" idle  ");

	    if (show_pid == TRUE)
		printf (" PID  ");

	    printf ("what\n");
	}
    }




    /*
     * Process user information.
     */
    while ((utmp_rec = getutent()))
    {
	/*
	 * Check we actually want to see this record.
	 * It must be a valid active user process,
	 * and match a specified search name.
	 */
	if ( (utmp_rec->ut_type == USER_PROCESS)
	  && (strcmp(utmp_rec->ut_user, ""))
	  && ( (search_name[0] == '\0')
	    || ( (search_name[0] != '\0')
	    && !strncmp(search_name, utmp_rec->ut_user, 8) ) ) )
	{
	    /*
	     * Get the username
	     */
	    strncpy (username, utmp_rec->ut_user, 8);
	    username[8] = '\0';		/* Set end terminator */


	    /*
	     * Find out the uid of that user (from their
	     * passwd entry)
	     */
	    uid = -1;
	    if ((passwd_entry = getpwnam (username)) != NULL)
	    {
	     uid = passwd_entry->pw_uid;
	    }

	    /*
	     * Get (and clean up) the tty line
	     */
	    for (i = 0; (utmp_rec->ut_line[i] > 32) && (i < 6); i ++)
		tty[i] = utmp_rec->ut_line[i];

	    utmp_rec->ut_line[i] = '\0';
	    tty[i] = '\0';


	    /*
	     * Don't bother getting info if it's not asked for
	     */
	    if (long_format == TRUE)
	    {

		/*
		 * Get the remote hostname; this can be up to 16 chars,
		 * but if any chars are invalid (ie: [^a-zA-Z0-9\.])
		 * then the char is changed to a string terminator.
		 */
		if (from_switch == TRUE)
		{
		    strncpy (rhost, utmp_rec->ut_host, 16);
		    rhost[16] = '\0';

		}


		/*
		 * Get the login time
		 * (Calculated by LG's routine, below)
		 */
		strcpy (login_time, logintime(utmp_rec->ut_time));
	    }



	    /*
	     * Get the idle time.
	     * (Calculated by LG's routine, below)
	     */
	    strcpy (idle_time, idletime (tty));



	    /*
	     * That's all the info out of /etc/utmp.
	     * The rest is more difficult.  We use the pid from
	     * utmp_rec->ut_pid to look in /proc for the info.
	     * NOTE: This is not necessarily the active pid, so we chase
	     * down the path of parent -> child pids until we find it,
	     * according to the information given in /proc/<pid>/stat.
	     */

	    sprintf (pid, "%d", utmp_rec->ut_pid);

	    what[0] = '\0';
	    strcpy (file_name, pid);
	    strcat (file_name, "/stat");
	    jcpu = 0;
	    pcpu = 0;

	    if ((fp = fopen(file_name, "r")))
	    {
		while (what[0] == '\0')
		{
		    /*
		     * Check /proc/<pid>/stat to see if the process
		     * controlling the tty is the current one
		     */
		    fscanf (fp, "%d %s %c %*d %*d %*d %*d %d "
			"%*u %*u %*u %*u %*u %d %d %d %d",
			&curr_pid, comm, &state, &tpgid,
			&utime, &stime, &cutime, &cstime);

		    fclose (fp);

		    if (comm[0] == '\0')
			strcpy (comm, "-");

		    /*
		     * Calculate jcpu and pcpu.
		     * JCPU is the time used by all processes and their
		     * children, attached to the tty.
		     * PCPU is the time used by the current process
		     * (calculated once after the loop, using last
		     * obtained values).
		     */
		    if (!jcpu)
			jcpu = cutime + cstime;

		    /*
		     * Check for a zombie first...
		     */
		    if (state == 'Z')
			strcpy (what, ZOMBIE);
		    else if (curr_pid == tpgid)
		    {
			/*
			 * If it is the current process, read cmdline
			 * If that's empty, then the process is swapped out,
			 * or is a zombie, so we use the command given in stat
			 * which is in normal round brackets, ie: "()".
			 */
			strcpy (file_name, pid);
			strcat (file_name, "/cmdline");
			if ((fp = fopen(file_name, "r")))
			{
			    i = 0;
			    j = fgetc (fp);
			    while ((j != EOF) && (i < 256))
			    {
				if (j == '\0')
				    j = ' ';

				what[i] = j;
				i++;
				j = fgetc (fp);
			    }
			    what[i] = '\0';
			    fclose (fp);
			}

			if (what[0] == '\0')
			    strcpy (what, comm);
		    }
		    else
		    {
			/*
			 * Check out the next process
			 * If we can't open it, use info from this process,
			 * so we have to check out cmdline first.
			 *
			 * If we're not using "-u" then should we just
			 * say "-" (or "-su") instead of a command line ?
			 * If so, we should strpcy(what, "-"); when we
			 * fclose() in the if after the stat() below.
			 */
			strcpy (file_name, pid);
			strcat (file_name, "/cmdline");

			if ((fp = fopen (file_name, "r")))
			{
			    i = 0;
			    j = fgetc (fp);
			    while ((j != EOF) && (i < 256))
			    {
				if (j == '\0')
				    j = ' ';

				what[i] = j;
				i++;
				j = fgetc (fp);
			    }
			    what[i] = '\0';
			    fclose (fp);
			}

			if (what[0] == '\0')
			    strcpy (what, comm);

			/*
			 * Now we have something in the what variable,
			 * in case we can't open the next process.
			 */
			sprintf (pid, "%d", tpgid);
			strcpy (file_name, pid);
			strcat (file_name, "/stat");

			fp = fopen (file_name, "r");

			if (fp && (ignore_user == FALSE))
			{
			    /*
			     * We don't necessarily go onto the next process,
			     * unless we are either ignoring who the effective
			     * user is, or it's the same uid
			     */
			    stat (file_name, &stat_rec);

			    /*
			     * If the next process is not owned by this
			     * user finish the loop.
			     */
			    if (stat_rec.st_uid != uid)
			    {
				fclose (fp);

				strcpy (what, "-su");
				/*
				 * See comment above somewhere;  I've used
				 * "-su" here, as the next process is owned
				 * by someone else; this is generally
				 * because the user has done an "su" which
				 * then exec'd something else.
				 */
			    }
			    else
				what[0] = '\0';
			}
			else if (fp)	/* else we are ignoring uid's */
			    what[0] = '\0';
		    }
		}
	    }
	    else	/* Could not open first process for user */
		strcpy (what, "?");


	    /*
	     * There is a bug somewhere in my version of linux
	     * which means that utmp records are not cleaned
	     * up properly when users log out. However, we
	     * can detect this, by the users first process
	     * not being there when we look in /proc.
	     */


	    /*
	     * Don't output a line for "dead" users.
	     * This gets round a bug which doesn't update utmp/wtmp
	     * when users log out.
	     */
	    if (what[0] != '?')
	    {
#ifdef 0
/* This makes unix98 pty's not line up, so has been disabled - JEH. */
		/*
		 * Remove the letters 'tty' from the tty id
		 */
		if (!strncmp (tty, "tty", 3))
		{
		    for (i = 3; tty[i - 1] != '\0'; i ++)
			tty[i - 3] = tty[i];
		}
#endif

		/*
		 * Common fields
		 */
		sprintf (out_line, "%-9.8s%-6.7s ", username, tty);


		/*
		 * Format the line for output
		 */
		if (long_format == TRUE)
		{
		    /*
		     * Calculate CPU usage
		     */
		    pcpu = utime + stime;
		    jcpu /= 100;
		    pcpu /= 100;

		    if (from_switch == TRUE)
			sprintf (out_line, "%s %-16.15s", out_line, rhost);

		    sprintf (out_line, "%s%8.8s ", out_line, login_time);

		}

		sprintf (out_line, "%s%6s", out_line, idle_time);


		if (long_format == TRUE)
		{
		    if (!jcpu)
			strcat (out_line, "      ");
		    else if (jcpu/60)
			sprintf (out_line, "%s%3d:%02d", out_line,
				jcpu/60, jcpu%60);
		    else
			sprintf (out_line, "%s    %2d", out_line, jcpu);

		    if (!pcpu)
			strcat (out_line, "      ");
                    else if (pcpu/60)
			sprintf (out_line, "%s%3d:%02d", out_line,
				pcpu/60, pcpu%60);
		    else
			sprintf (out_line, "%s    %2d", out_line, pcpu);
		}

		if (show_pid == TRUE)
		    sprintf (out_line, "%s %5.5s", out_line, pid);


		strcat (out_line, "  ");
		strcat (out_line, what);


		/*
		 * Try not to exceed the line length
		 */
		out_line[line_length] = '\0';

		printf ("%s\n", out_line);
	    }
	}
    }
}



/*
 * put_syntax()
 *
 * Routine to print the correct syntax to call this program,
 * and then exit out appropriately
 */
void put_syntax ()
{
    fprintf (stderr, "usage: w [-hfsud] [user]\n");
    exit (-1);
}



/*
 * idletime()
 *
 * Routine which returns a string containing
 * the idle time of a given user.
 *
 * This routine was lifted from the original w program
 * by Larry Greenfield  (greenfie@gauss.rutgers.edu)
 * Copyright (c) 1993 Larry Greenfield
 *
 */
char *idletime (tty)

char *tty;

{
    struct stat terminfo;
    unsigned long idle;
    char ttytmp[40];
    static char give[20];
    time_t curtime;

    curtime = time (NULL);

    sprintf (ttytmp, "/dev/%s", tty);
    stat (ttytmp, &terminfo);
    idle = (unsigned long) curtime - (unsigned long) terminfo.st_atime;

    if (idle >= (60 * 60))		/* more than an hour */
    {
	if (idle >= (60 * 60 * 48))	/* more than two days */
	    sprintf (give, "%2ludays", idle / (60 * 60 * 24));
	else
	    sprintf (give, " %2lu:%02u", idle / (60 * 60),
	      (unsigned) ((idle / 60) % 60));
    }
    else
    {
	if (idle / 60)
	    sprintf (give, "%6lu", idle / 60);
	else
	    give[0]=0;
    }

    return give;
}



/*
 * logintime()
 *
 * Returns the time given in a suitable format
 *
 * This routine was lifted from the original w program
 * by Larry Greenfield  (greenfie@gauss.rutgers.edu)
 * Copyright (c) 1993 Larry Greenfield
 *
 */

#undef ut_time

char *logintime(ut_time)

time_t ut_time;

{
    time_t curtime;
    struct tm *logintime, *curtm;
    int hour, am, curday, logday;
    static char give[20];
    static char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri",
				"Sat" };
    static char *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
				"Aug", "Sep", "Oct", "Nov", "Dec" };

    curtime = time(NULL);
    curtm = localtime(&curtime);
    curday = curtm->tm_yday;
    logintime = localtime(&ut_time);
    hour = logintime->tm_hour;
    logday = logintime->tm_yday;
    am = (hour < 12);

    if (!am)
	hour -= 12;

    if (hour == 0)
	hour = 12;

    /*
     * This is a newer behavior: it waits 12 hours and the next day, and then
     * goes to the 2nd time format. This should reduce confusion.
     * It then waits only 6 days (not till the last moment) to go the last
     * time format.
     */
    if ((curtime > (ut_time + (60 * 60 * 12))) && (logday != curday))
    {
	if (curtime > (ut_time + (60 * 60 * 24 * 6)))
	    sprintf(give, "%2d%3s%2d", logintime->tm_mday,
		month[logintime->tm_mon], (logintime->tm_year % 100));
	else
	    sprintf(give, "%*s%2d%s", 3, weekday[logintime->tm_wday],
		hour, am ? "am" : "pm");
    }
    else
	sprintf(give, "%2d:%02d%s", hour, logintime->tm_min, am ? "am" : "pm");

    return give;
}

