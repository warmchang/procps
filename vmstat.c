#define PROGNAME "vmstat"
/* Copyright 1994 by Henry Ware <al172@yfn.ysu.edu>. Copyleft same year. */
/* This attempts to display virtual memory statistics.
   vmstat does not need to be run as root.
*/
/* TODO etc.
 * Count the system calls.  How?  I don't even know where to start.
 * add more items- aim at Posix 1003.2 (1003.7?) compliance, if relevant (I.  
   don't think it is, but let me know.)
 * sometimes d(inter)<d(ticks)!!  This is not possible: inter is a sum of 
   positive numbers including ticks.  But it happens.  This doesn't seem to
   affect things much, however.
 * It would be interesting to see when the buffers avert a block io:
   Like SysV4's "sar -b"... it might fit better here, with Linux's variable 
   buffer size.
 * Ideally, blocks in & out would be counted in 1k increments, rather than
   by block: this only makes a difference for CDs and is a problematic fix.
*/
   
#include "proc/sysinfo.h"
#include "proc/version.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/dir.h>
#include <dirent.h>

#define NDEBUG !DEBUG

#define BUFFSIZE 8192
#define FALSE 0
#define TRUE 1

static char buff[BUFFSIZE]; /* used in the procedures */

typedef unsigned long long jiff;

static int a_option; /* "-a" means "show active/inactive" */
static unsigned sleep_time = 1;
static unsigned long num_updates;

static unsigned int height=22;   // window height, reset later if needed
static unsigned int moreheaders=TRUE;

/****************************************************************/


static void usage(void) {
  fprintf(stderr,"usage: %s [-V] [-n] [delay [count]]\n",PROGNAME);
  fprintf(stderr,"              -V prints version.\n");
  fprintf(stderr,"              -n causes the headers not to be reprinted regularly.\n");
  fprintf(stderr,"              -a print inactive/active page stats.\n");
  fprintf(stderr,"              delay is the delay between updates in seconds. \n");
  fprintf(stderr,"              count is the number of updates.\n");
  exit(EXIT_FAILURE);
}

static void crash(const char *filename) {
    perror(filename);
    exit(EXIT_FAILURE);
}

static int winhi(void) {
    struct winsize win;
    int rows = 24;
 
    if (ioctl(1, TIOCGWINSZ, &win) != -1 && win.ws_row > 0)
      rows = win.ws_row;
 
    return rows;
}


static void showheader(void){
  printf("%8s%28s%10s%12s%11s%9s\n",
	 "procs","memory","swap","io","system","cpu");
  printf("%2s %2s %2s %6s %6s %6s %6s %4s %4s %5s %5s %4s %5s %2s %2s %2s\n",
	 "r","b","w","swpd","free",
	 a_option?"inact":"buff", a_option?"active":"cache",
	 "si","so","bi","bo",
	 "in","cs","us","sy","id");
}

static void getstat(jiff *cuse, jiff *cice, jiff *csys, jiff *cide, jiff *ciow,
	     unsigned *pin, unsigned *pout, unsigned *s_in, unsigned *sout,
	     unsigned *itot, unsigned *i1, unsigned *ct) {
  static int Stat;
  int need_extra_file = 0;
  char* b;
  buff[BUFFSIZE-1] = 0;  /* ensure null termination in buffer */

  if(Stat){
    lseek(Stat, 0L, SEEK_SET);
  }else{
    Stat = open("/proc/stat", O_RDONLY, 0);
    if(Stat == -1) crash("/proc/stat");
  }
  read(Stat,buff,BUFFSIZE-1);
  *itot = 0; 
  *i1 = 1;   /* ensure assert below will fail if the sscanf bombs */
  *ciow = 0;  /* not separated out until the 2.5.41 kernel */

  b = strstr(buff, "cpu ");
  if(b) sscanf(b,  "cpu  %Lu %Lu %Lu %Lu %Lu", cuse, cice, csys, cide, ciow);

  b = strstr(buff, "page ");
  if(b) sscanf(b,  "page %u %u", pin, pout);
  else need_extra_file = 1;

  b = strstr(buff, "swap ");
  if(b) sscanf(b,  "swap %u %u", s_in, sout);
  else need_extra_file = 1;

  b = strstr(buff, "intr ");
  if(b) sscanf(b,  "intr %u %u", itot, i1);

  b = strstr(buff, "ctxt ");
  if(b) sscanf(b,  "ctxt %u", ct);

  if(need_extra_file){  /* 2.5.40-bk4 and above */
    vminfo();
    *pin  = vm_pgpgin;
    *pout = vm_pgpgout;
    *s_in = vm_pswpin;
    *sout = vm_pswpout;
  }
}

static void getrunners(unsigned int *running, unsigned int *blocked, 
		unsigned int *swapped) {
  static struct direct *ent;
  static char filename[80];
  static int fd;
  static unsigned size;
  static char c;
  DIR *proc;

  *running=0;
  *blocked=0;
  *swapped=0;

  if ((proc=opendir("/proc"))==NULL) crash("/proc");

  while((ent=readdir(proc))) {
    if (isdigit(ent->d_name[0])) {  /*just to weed out the obvious junk*/
      sprintf(filename, "/proc/%s/stat", ent->d_name);
      if ((fd = open(filename, O_RDONLY, 0)) != -1) { /*this weeds the rest*/
	read(fd,buff,BUFFSIZE-1);
	sscanf(
	  buff,
	  "%*d %*s %c "
	  "%*d %*d %*d %*d %*d %*u %*u"
	  " %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*d %*u %u"
	  /*  " %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u\n"  */ ,
	  &c,
	  &size
	);
	close(fd);

	if (c=='R') {
	  if (size>0) (*running)++;
	  else (*swapped)++;
        }
	else if (c=='D') {
	  if (size>0) (*blocked)++;
	  else (*swapped)++;
        }
      }
    }
  }
  closedir(proc);

#if 1
  /* is this next line a good idea?  It removes this thing which
     uses (hopefully) little time, from the count of running processes */
  (*running)--;
#endif
}



//////////////////////////////////////////////////////////////////////////////////////
void old_format(void) {
  const char format[]="%2u %2u %2u %6u %6u %6u %6u %4u %4u %5u %5u %4u %5u %2u %2u %2u\n";
  unsigned int tog=0; /* toggle switch for cleaner code */
  unsigned int i;
  unsigned int hz = Hertz;
  unsigned int running,blocked,swapped;
  jiff cpu_use[2], cpu_nic[2], cpu_sys[2], cpu_idl[2], cpu_iow[2];
  jiff duse,dsys,didl,Div,divo2;
  unsigned int pgpgin[2], pgpgout[2], pswpin[2], pswpout[2];
  unsigned int inter[2],ticks[2],ctxt[2];
  unsigned int sleep_half; 
  unsigned int kb_per_page = sysconf(_SC_PAGESIZE) / 1024;

  sleep_half=(sleep_time/2);
  showheader();

  getrunners(&running,&blocked,&swapped);
  meminfo();
  getstat(cpu_use,cpu_nic,cpu_sys,cpu_idl,cpu_iow,
	  pgpgin,pgpgout,pswpin,pswpout,
	  inter,ticks,ctxt);
  duse= *cpu_use + *cpu_nic; 
  dsys= *cpu_sys;
  didl= *cpu_idl + *cpu_iow;
  Div= duse+dsys+didl;
  divo2= Div/2UL;
  printf(format,
	 running,blocked,swapped,
	 kb_swap_used,kb_main_free,
	 a_option?kb_inactive:kb_main_buffers,
	 a_option?kb_active:kb_main_cached,
	 (unsigned)( (*pswpin  * kb_per_page * hz + divo2) / Div ),
	 (unsigned)( (*pswpout * kb_per_page * hz + divo2) / Div ),
	 (unsigned)( (*pgpgin                * hz + divo2) / Div ),
	 (unsigned)( (*pgpgout               * hz + divo2) / Div ),
	 (unsigned)( (*inter                 * hz + divo2) / Div ),
	 (unsigned)( (*ctxt                  * hz + divo2) / Div ),
	 (unsigned)( (100*duse                    + divo2) / Div ),
	 (unsigned)( (100*dsys                    + divo2) / Div ),
	 (unsigned)( (100*didl                    + divo2) / Div )
  );

  for(i=1;i<num_updates;i++) { /* \\\\\\\\\\\\\\\\\\\\ main loop ////////////////// */
    sleep(sleep_time);
    if (moreheaders && ((i%height)==0)) showheader();
    tog= !tog;

    getrunners(&running,&blocked,&swapped);
    meminfo();
    getstat(cpu_use+tog,cpu_nic+tog,cpu_sys+tog,cpu_idl+tog,cpu_iow+tog,
	  pgpgin+tog,pgpgout+tog,pswpin+tog,pswpout+tog,
	  inter+tog,ticks+tog,ctxt+tog);
    duse= cpu_use[tog]-cpu_use[!tog] + cpu_nic[tog]-cpu_nic[!tog];
    dsys= cpu_sys[tog]-cpu_sys[!tog];
    didl= cpu_idl[tog]-cpu_idl[!tog] + cpu_iow[tog]-cpu_iow[!tog];
    /* idle can run backwards for a moment -- kernel "feature" */
    if(cpu_idl[tog]<cpu_idl[!tog]) didl=0;
    Div= duse+dsys+didl;
    divo2= Div/2UL;
    printf(format,
	   running,blocked,swapped,
	   kb_swap_used,kb_main_free,
	   a_option?kb_inactive:kb_main_buffers,
	   a_option?kb_inactive:kb_main_cached,
	   (unsigned)( ( (pswpin [tog] - pswpin [!tog])*kb_per_page+sleep_half )/sleep_time ),
	   (unsigned)( ( (pswpout[tog] - pswpout[!tog])*kb_per_page+sleep_half )/sleep_time ),
	   (unsigned)( (  pgpgin [tog] - pgpgin [!tog]             +sleep_half )/sleep_time ),
	   (unsigned)( (  pgpgout[tog] - pgpgout[!tog]             +sleep_half )/sleep_time ),
	   (unsigned)( (  inter  [tog] - inter  [!tog]             +sleep_half )/sleep_time ),
	   (unsigned)( (  ctxt   [tog] - ctxt   [!tog]             +sleep_half )/sleep_time ),
	   (unsigned)( (100*duse+divo2)/Div ),
	   (unsigned)( (100*dsys+divo2)/Div ),
	   (unsigned)( (100*didl+divo2)/Div )
    );
  }
  exit(EXIT_SUCCESS);
}


//////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
  argc=0; /* redefined as number of integer arguments */
  for (argv++;*argv;argv++) {
    if ('-' ==(**argv)) {
      switch (*(++(*argv))) {
      case 'V':
	display_version();
	exit(0);
      case 'a':
	/* active/inactive mode */
	a_option=1;
        break;
      case 'n':
	/* print only one header */
	moreheaders=FALSE;
        break;
      default:
	/* no other aguments defined yet. */
	usage();
      }
    } else {
      argc++;
      switch (argc) {
      case 1:
        if ((sleep_time = atoi(*argv)) == 0)
         usage();
       num_updates = ULONG_MAX;
       break;
      case 2:
        num_updates = atol(*argv);
       break;
      default:
       usage();
      } /* switch */
    }
  }

  if (moreheaders) {
      int tmp=winhi()-3;
      height=((tmp>0)?tmp:22);
  }    

  setlinebuf(stdout);

  old_format();
  return 0;
}


