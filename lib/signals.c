/*
 * signals.c - signal name, and number, conversions
 * Copyright 1998-2003 by Albert Cahalan
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
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "signals.h"
#include "c.h"

/* Linux signals:
 *
 * SIGSYS is required by Unix98.
 * SIGEMT is part of SysV, BSD, and ancient UNIX tradition.
 *
 * They are provided by these Linux ports: alpha, mips, sparc, and sparc64.
 * You get SIGSTKFLT and SIGUNUSED instead on i386, m68k, ppc, and arm.
 * (this is a Linux & libc bug -- both must be fixed)
 *
 * Total garbage: SIGIO SIGINFO SIGIOT SIGLOST SIGCLD
 *                 (popular ones are handled as aliases)
 * Nearly garbage: SIGSTKFLT SIGUNUSED (nothing else to fill slots)
 */

/* Linux 2.3.29 replaces SIGUNUSED with the standard SIGSYS signal */
#ifndef SIGSYS
#  warning Standards require that <signal.h> define SIGSYS
#  define SIGSYS SIGUNUSED
#endif

/* If we see both, it is likely SIGSTKFLT (junk) was replaced. */
#ifdef SIGEMT
#  undef SIGSTKFLT
#endif

#ifndef SIGRTMIN
#  warning Standards require that <signal.h> define SIGRTMIN; assuming 32
#  define SIGRTMIN 32
#endif

/* It seems the SPARC libc does not know the kernel supports SIGPWR. */
#ifndef SIGPWR
#  warning Your header files lack SIGPWR. (assuming it is number 29)
#  define SIGPWR 29
#endif

typedef struct mapstruct {
  const char *name;
  int num;
} mapstruct;


static const mapstruct sigtable[] = {
  {"ABRT",   SIGABRT},  /* IOT */
  {"ALRM",   SIGALRM},
  {"BUS",    SIGBUS},
  {"CHLD",   SIGCHLD},  /* CLD */
  {"CONT",   SIGCONT},
#ifdef SIGEMT
  {"EMT",    SIGEMT},
#endif
  {"FPE",    SIGFPE},
  {"HUP",    SIGHUP},
  {"ILL",    SIGILL},
  {"INT",    SIGINT},
  {"KILL",   SIGKILL},
  {"PIPE",   SIGPIPE},
  {"POLL",   SIGPOLL},  /* IO */
  {"PROF",   SIGPROF},
  {"PWR",    SIGPWR},
  {"QUIT",   SIGQUIT},
  {"SEGV",   SIGSEGV},
#ifdef SIGSTKFLT
  {"STKFLT", SIGSTKFLT},
#endif
  {"STOP",   SIGSTOP},
  {"SYS",    SIGSYS},   /* UNUSED */
  {"TERM",   SIGTERM},
  {"TRAP",   SIGTRAP},
  {"TSTP",   SIGTSTP},
  {"TTIN",   SIGTTIN},
  {"TTOU",   SIGTTOU},
  {"URG",    SIGURG},
  {"USR1",   SIGUSR1},
  {"USR2",   SIGUSR2},
  {"VTALRM", SIGVTALRM},
  {"WINCH",  SIGWINCH},
  {"XCPU",   SIGXCPU},
  {"XFSZ",   SIGXFSZ}
};

const int number_of_signals = sizeof(sigtable)/sizeof(mapstruct);

static int compare_signal_names(const void *a, const void *b){
  return strcasecmp( ((const mapstruct*)a)->name, ((const mapstruct*)b)->name );
}


const char *get_sigtable_name(int row)
{
    if (row < 0 || row >= number_of_signals)
        return NULL;
    return sigtable[row].name;
}

const int get_sigtable_num(int row)
{
    if (row < 0 || row >= number_of_signals)
        return -1;
    return sigtable[row].num;
}

/* return -1 on failure */
int signal_name_to_number(const char *restrict name){
    long val;
    int offset;

    /* clean up name */
    if(!strncasecmp(name,"SIG",3))
        name += 3;

    if(!strcasecmp(name,"CLD"))
        return SIGCHLD;
    if(!strcasecmp(name,"IO"))
        return SIGPOLL;
    if(!strcasecmp(name,"IOT"))
        return SIGABRT;
    /* search the table */
    {
        const mapstruct ms = {name,0};
        const mapstruct *restrict const ptr = bsearch(
                                                      &ms,
                                                      sigtable,
                                                      number_of_signals,
                                                      sizeof(mapstruct),
                                                      compare_signal_names);
        if(ptr)
            return ptr->num;
    }

    if(!strcasecmp(name,"RTMIN"))
        return SIGRTMIN;
    if(!strcasecmp(name,"EXIT"))
        return 0;
    if(!strcasecmp(name,"NULL"))
        return 0;

    offset = 0;
    if(!strncasecmp(name,"RTMIN+",6)) {
        name += 6;
        offset = SIGRTMIN;
    }

    /* not found, so try as a number */
    {
        char *endp;
        val = strtol(name,&endp,10);
        if(*endp || endp==name)
            return -1; /* not valid */
    }
    if(val+SIGRTMIN>127)
        return -1; /* not valid */
    return val+offset;
}

const char *signal_number_to_name(int signo)
{
    static char buf[32];
    int n = number_of_signals;
    signo &= 0x7f; /* need to process exit values too */
    while (n--) {
        if(sigtable[n].num==signo)
            return sigtable[n].name;
    }
    if (signo == SIGRTMIN)
        return "RTMIN";
    if (signo)
        sprintf(buf, "RTMIN+%d", signo-SIGRTMIN);
    else
        strcpy(buf,"0");  /* AIX has NULL; Solaris has EXIT */
    return buf;
}

