/*
 * utmp.c - utmp printing command
 * Copyright (C) Albert Cahalan
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
#include <sys/ioctl.h>
#include <time.h>
#include <utmp.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* examine and fix utmp entries.  Note that the code for fixing entries
   is not complete, and indeed does nothing at all at this time.  No bug
   reports, please, as I am still actively working on this.  It is not
   here for general use, but only so that I can ferret out any bugs that
   exist on other peoples systems without having to log in to their systems
   myself ;-)  */


int main (int argc, char **argv) {

  FILE *ut; /* /var/run/utmp */
  struct utmp uts; /* utmp record */
  char user[UT_NAMESIZE + 1];
  char host[17];
  char ch;
  int print_all = 0, list = 0, fix = 0;

/* get options */
  while ((ch = getopt(argc, argv, "laf")) != EOF)
    switch (ch) {
    case 'a':
      print_all = 1;
      break;
    case 'l':
      list = 1;
      break;
    case 'f':
      fix = 1;
      break;
    }

/* check argument options */
  if ( (!list && !print_all && !fix)) {
    fprintf(stderr, "You must specify a command line option:\n\tl = list\n\
\tf = fix\n\ta = all (requires l or f)\n");
    exit(1);
  }


  if (list) {
    ut = fopen(UTMP_FILE, "r");
    while (fread(&uts, sizeof(uts), 1, ut))
      if (((uts.ut_type == USER_PROCESS) && (uts.ut_name[0] != '\000'))
	  || print_all) {
	strncpy(user, uts.ut_user, UT_NAMESIZE);
	user[UT_NAMESIZE]=0;
	strncpy(host, uts.ut_host, 16);
	host[16]=0;
	printf("ut_type: %d\n", uts.ut_type);
	printf("ut_pid:  %d\n", uts.ut_pid);
	printf("ut_line: %s\n", uts.ut_line);
	printf("ut_id:   %2s\n", uts.ut_id);
	printf("ut_time: %d\n", uts.ut_time);
	printf("ut_user: %s\n", user);
	printf("ut_host: %s\n", host);
	printf("ut_addr: %d\n\n", uts.ut_addr);
      }
    fclose(ut);
  }


  if (fix) {
    ut = fopen(UTMP_FILE, "r");
    while (fread(&uts, sizeof(uts), 1, ut))
      if (((uts.ut_type == USER_PROCESS) && (uts.ut_name[0] != '\000'))
	  || print_all) {
	/* Display entry in utmp */
	strncpy(user, uts.ut_user, UT_NAMESIZE);
	user[UT_NAMESIZE]=0;
	strncpy(host, uts.ut_host, 16);
	host[16]=0;
	printf("ut_type: %d\n", uts.ut_type);
	printf("ut_pid:  %d\n", uts.ut_pid);
	printf("ut_line: %s\n", uts.ut_line);
	printf("ut_id:   %s2\n", uts.ut_id);
	printf("ut_time: %d\n", uts.ut_time);
	printf("ut_user: %s\n", user);
	printf("ut_host: %s\n", host);
	printf("ut_addr: %d\n\n", uts.ut_addr);

	printf("Modify this record? (y/N): "); fflush(stdout);
	/* Ask if to delete or no */
	if ((ch = getchar()) == 'y' || ch == 'Y') {
	  while (getchar() != '\n');
	  printf("Change ut_type? "); fflush(stdout);
	  if ((ch = getchar()) == 'y' || ch == 'Y') {
	    while (getchar() != '\n');
	    printf("INIT, LOGIN, USER, or DEAD_PROCESS? (I/L/U/D): ");
	    fflush(stdout);
	    ch = getchar();
	    switch (ch) {
	    case 'i':
	    case 'I':
	      uts.ut_type = INIT_PROCESS;
	      break;
	    case 'l':
	    case 'L':
	      uts.ut_type = LOGIN_PROCESS;
	      break;
	    case 'u':
	    case 'U':
	      uts.ut_type = USER_PROCESS;
	      break;
	    case 'd':
	    case 'D':
	      uts.ut_type = DEAD_PROCESS;
	      break;
	    default:
	      printf("Invalid choice: %c\n", ch);
	    }
	    if (ch != '\n') while ((ch = getchar()) != '\n');
	  }
	  if (ch != '\n') while ((ch = getchar()) != '\n');
	  printf("Change ut_id field? (y/N): "); fflush(stdout);
	  if ((ch = getchar()) == 'y' || ch == 'Y') {
	    while (getchar() != '\n');
	    printf("Please enter the two characters for ut_id: ");
	    fflush(stdout);
	    uts.ut_id[0] = getchar();
	    uts.ut_id[1] = getchar();
	    while ((ch = getchar()) != '\n');
	  }
	  if (ch != '\n') while ((ch = getchar()) != '\n');
	  printf("Change the ut_user field? (y/N): "); fflush(stdout);
	  if ((ch = getchar()) == 'y' || ch == 'Y') {
	    int i;
	    while (getchar() != '\n');
	    printf("Please enter the new ut_name, up to %c characters: ",
		   UT_NAMESIZE);
	    fflush(stdout);
	    for (i=0; i<UT_NAMESIZE; i++) {
	      ch = getchar();
	      uts.ut_user[i] = (ch != '\n') ? ch : i = UT_NAMESIZE, (char) 0;
	    }
	  }
	  if (ch != '\n') while ((ch = getchar()) != '\n');
	  printf("Change the ut_host field? (y/N): "); fflush(stdout);
	  if ((ch = getchar()) == 'y' || ch == 'Y') {
	    int i;
	    while (getchar() != '\n');
	    printf("Please enter the new ut_host, up to 16 characters: ");
	    fflush(stdout);
	    for (i=0; i<16; i++) {
	      ch = getchar();
	      uts.ut_user[i] = (ch != '\n') ? ch : i = 16, (char) 0;
	    }
	    if (ch != '\n') while ((ch = getchar()) != '\n');
	  }

	  /* Here go the changes...*/
/*	  utmpname(UTMP_FILE);
	  setutent();
	  pututline(&uts);
	  endutent(); */
/* But they don't work... */

	}
	if (ch != '\n') while ((ch = getchar()) != '\n');
	/* here we should write the utmp entry */
      }
    fclose(ut);
  }


  return 0;


}
