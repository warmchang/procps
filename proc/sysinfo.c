/*
 * File for parsing top-level /proc entities.
 * Copyright (C) 1992-1998 by Michael K. Johnson, johnsonm@redhat.com
 * Copyright 1998-2003 Albert Cahalan
 * June 2003, Fabian Frederick, slab info
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include "alloc.h"
#include "version.h"
#include "sysinfo.h" /* include self to verify prototypes */
#include "procps-private.h"


#define BAD_OPEN_MESSAGE					\
"Error: /proc must be mounted\n"				\
"  To mount /proc at boot you need an /etc/fstab line like:\n"	\
"      proc   /proc   proc    defaults\n"			\
"  In the meantime, run \"mount proc /proc -t proc\"\n"

#define LOADAVG_FILE "/proc/loadavg"
static int loadavg_fd = -1;

// As of 2.6.24 /proc/meminfo seems to need 888 on 64-bit,
// and would need 1258 if the obsolete fields were there.
// As of 3.13 /proc/vmstat needs 2623,
// and /proc/stat needs 3076.
static char buf[8192];

/* This macro opens filename only if necessary and seeks to 0 so
 * that successive calls to the functions are more efficient.
 * It also reads the current contents of the file into the global buf.
 */
#define FILE_TO_BUF(filename, fd) do{				\
    static int local_n;						\
    if (fd == -1 && (fd = open(filename, O_RDONLY)) == -1) {	\
	fputs(BAD_OPEN_MESSAGE, stderr);			\
	fflush(NULL);						\
	_exit(102);						\
    }								\
    lseek(fd, 0L, SEEK_SET);					\
    if ((local_n = read(fd, buf, sizeof buf - 1)) < 0) {	\
	perror(filename);					\
	fflush(NULL);						\
	_exit(103);						\
    }								\
    buf[local_n] = '\0';					\
}while(0)

/* evals 'x' twice */
#define SET_IF_DESIRED(x,y) do{  if(x) *(x) = (y); }while(0)

/* return minimum of two values */
#define MIN(x,y) ((x) < (y) ? (x) : (y))

/*
 * procps_hertz_get:
 *
 *
 * Some values in /proc are expressed in units of 1/HZ seconds, where HZ
 * is the kernel clock tick rate. One of these units is called a jiffy.
 * The HZ value used in the kernel may vary according to hacker desire.
 *
 * On some architectures, the kernel provides an ELF note to indicate
 * HZ.
 *
 * Returns:
 *  The discovered or assumed hertz value
 */
PROCPS_EXPORT long procps_hertz_get(void)
{
    long hz;

#ifdef _SC_CLK_TCK
    if ((hz = sysconf(_SC_CLK_TCK)) > 0)
        return hz;
#endif
#ifdef HZ
    return(HZ);
#endif
    /* Last resort, assume 100 */
    return 100;
}

/*
 * procps_loadavg:
 * @av1: location to store 1 minute load average
 * @av5: location to store 5 minute load average
 * @av15: location to store 15 minute load average
 *
 * Find the 1,5 and 15 minute load average of the system
 *
 * Returns: 0 on success <0 on error
 */
PROCPS_EXPORT int procps_loadavg(
        double *restrict av1,
        double *restrict av5,
        double *restrict av15)
{
    double avg_1=0, avg_5=0, avg_15=0;
    char *savelocale;
    int retval=0;

    FILE_TO_BUF(LOADAVG_FILE,loadavg_fd);
    savelocale = strdup(setlocale(LC_NUMERIC, NULL));
    setlocale(LC_NUMERIC, "C");
    if (sscanf(buf, "%lf %lf %lf", &avg_1, &avg_5, &avg_15) < 3) {
        retval = -ERANGE;
    }
    setlocale(LC_NUMERIC, savelocale);
    free(savelocale);
    SET_IF_DESIRED(av1,  avg_1);
    SET_IF_DESIRED(av5,  avg_5);
    SET_IF_DESIRED(av15, avg_15);
    return retval;
}

  static char buff[BUFFSIZE]; /* used in the procedures */
/***********************************************************************/

static void crash(const char *filename) {
    perror(filename);
    exit(EXIT_FAILURE);
}

/////////////////////////////////////////////////////////////////////////////
// based on Fabian Frederick's /proc/slabinfo parser

unsigned int getslabinfo (struct slab_cache **slab){
  FILE* fd;
  int cSlab = 0;
  buff[BUFFSIZE-1] = 0;
  *slab = NULL;
  fd = fopen("/proc/slabinfo", "rb");
  if(!fd) crash("/proc/slabinfo");
  while (fgets(buff,BUFFSIZE-1,fd)){
    if(!memcmp("slabinfo - version:",buff,19)) continue; // skip header
    if(*buff == '#')                           continue; // skip comments
    (*slab) = xrealloc(*slab, (cSlab+1)*sizeof(struct slab_cache));
    sscanf(buff,  "%47s %u %u %u %u",  // allow 47; max seen is 24
      (*slab)[cSlab].name,
      &(*slab)[cSlab].active_objs,
      &(*slab)[cSlab].num_objs,
      &(*slab)[cSlab].objsize,
      &(*slab)[cSlab].objperslab
    ) ;
    cSlab++;
  }
  fclose(fd);
  return cSlab;
}

///////////////////////////////////////////////////////////////////////////

unsigned get_pid_digits(void){
  char pidbuf[24];
  char *endp;
  long rc;
  int fd;
  static unsigned ret;

  if(ret) goto out;
  ret = 5;
  fd = open("/proc/sys/kernel/pid_max", O_RDONLY);
  if(fd==-1) goto out;
  rc = read(fd, pidbuf, sizeof pidbuf);
  close(fd);
  if(rc<3) goto out;
  pidbuf[rc] = '\0';
  rc = strtol(pidbuf,&endp,10);
  if(rc<42) goto out;
  if(*endp && *endp!='\n') goto out;
  rc--;  // the pid_max value is really the max PID plus 1
  ret = 0;
  while(rc){
    rc /= 10;
    ret++;
  }
out:
  return ret;
}

///////////////////////////////////////////////////////////////////////////

/* procps_cpu_count:
 *
 * Returns the number of CPUs that are currently online.
 *
 */
long procps_cpu_count(void)
{
    long cpus=1;

    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1)
        return 1;
    return cpus;
}

