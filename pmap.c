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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "proc/readproc.h"
#include "proc/version.h"

static void usage(void) NORETURN;
static void usage(void){
  fprintf(stderr,
    "Usage: pmap [-x | -d] [-q] pid...\n"
    "-x  show details\n"
    "-d  show offset and device number\n"
    "-q  quiet; less header/footer info\n"
    "-V  show the version number\n"
  );
  exit(1);
}


static int V_option;
static int r_option;  // ignored -- for SunOS compatibility
static int x_option;
static int d_option;
static int q_option;


static const char *get_args(unsigned pid){
  static char cmdbuf[64];
  char buf[32];
  int fd;
  ssize_t count;

  do{
    sprintf(buf,"/proc/%u/cmdline",pid);
    if( (( fd=open(buf,O_RDONLY) )) == -1) break;
    count = read(fd, cmdbuf, sizeof(cmdbuf)-1);
    close(fd);
    if(count<1) break;
    cmdbuf[count] = '\0';
    if(!isprint(cmdbuf[0])) break;
    while(count--) if(!isprint(cmdbuf[count])) cmdbuf[count]=' ';
    return cmdbuf;
  }while(0);

  do{
    char *cp;
    sprintf(buf,"/proc/%u/stat",pid);
    if( (( fd=open(buf,O_RDONLY) )) == -1) break;
    count = read(fd, cmdbuf, sizeof(cmdbuf)-1);
    close(fd);
    if(count<1) break;
    cmdbuf[count] = '\0';
    while(count--) if(!isprint(cmdbuf[count])) cmdbuf[count]=' ';
    cp = strrchr(cmdbuf,')');
    if(!cp) break;
    cp[0] = ']';
    cp[1] = '\0';
    cp = strchr(cmdbuf,'(');
    if(!cp) break;
    if(!isprint(cp[1])) break;
    cp[0] = '[';
    return cp;
  }while(0);

  return "[]";  // as good as anything
}

static const char *anon_name(int pid, unsigned KLONG addr, unsigned KLONG len){
  const char *cp = "  [ anon ]";
  proc_t proc;
  static int oldpid = -1;
  if (pid==oldpid || get_proc_stats(pid, &proc)){
    oldpid = pid;
    if( (proc.start_stack >= addr) && (proc.start_stack <= addr+len) )  cp = "  [ stack ]";
  }
  return cp;
}

static int one_proc(unsigned pid){
  char buf[32];
  char mapbuf[9600];
  unsigned long total_shared = 0ul;
  unsigned long total_private_readonly = 0ul;
  unsigned long total_private_writeable = 0ul;

  sprintf(buf,"/proc/%u/maps",pid);
  if(!freopen(buf, "r", stdin)) return 1;
  printf("%u:   %s\n", pid, get_args(pid));

  if(x_option && !q_option)
    printf("Address   Kbytes     RSS    Anon  Locked Mode   Mapping\n");
  if(d_option && !q_option)
    printf("Address   Kbytes Mode  Offset           Device     Mapping\n");

  while(fgets(mapbuf,sizeof mapbuf,stdin)){
    char flags[32];
    char *tmp; // to clean up unprintables
    unsigned KLONG start, end, diff;
    unsigned long long file_offset;
    unsigned dev_major, dev_minor;
    sscanf(mapbuf,"%"KLF"x-%"KLF"x %31s %Lx %x:%x", &start, &end, flags, &file_offset, &dev_major, &dev_minor);
    tmp = strchr(mapbuf,'\n');
    if(tmp) *tmp='\0';
    tmp = mapbuf;
    while(*tmp){
      if(!isprint(*tmp)) *tmp='?';
      tmp++;
    }
    
    diff = end-start;
    if(flags[3]=='s') total_shared  += diff;
    if(flags[3]=='p'){
      if(flags[1]=='w') total_private_writeable += diff;
      else              total_private_readonly  += diff;
    }

    // format used by Solaris 9 and procps-3.2.0+
    if(flags[3] == 'p') flags[3] = '-';
    flags[4] = '-';  // an 'R' if swap not reserved (MAP_NORESERVE, SysV ISM shared mem, etc.)
    flags[5] = '\0';

    if(x_option){
      const char *cp = strrchr(mapbuf,'/');
      if(cp && cp[1]) cp++;
      if(!cp) cp = anon_name(pid, start, diff);
      printf(
        (sizeof(KLONG)==8)
          ? "%016"KLF"x %7lu       -       -       - %s  %s\n"
          :      "%08lx %7lu       -       -       - %s  %s\n",
        start,
        (unsigned long)(diff>>10),
        flags,
        cp
      );
    }
    if(d_option){
      const char *cp = strrchr(mapbuf,'/');
      if(cp && cp[1]) cp++;
      if(!cp) cp = anon_name(pid, start, diff);
      printf(
        (sizeof(KLONG)==8)
          ? "%016"KLF"x %7lu %s %016Lx %03x:%05x  %s\n"
          :      "%08lx %7lu %s %016Lx %03x:%05x  %s\n",
        start,
        (unsigned long)(diff>>10),
        flags,
        file_offset,
        dev_major, dev_minor,
        cp
      );
    }
    if(!x_option && !d_option){
      const char *cp = strchr(mapbuf,'/');
      if(!cp) cp = anon_name(pid, start, diff);
      printf(
        (sizeof(KLONG)==8)
          ? "%016"KLF"x %6luK %s  %s\n"
          :      "%08lx %6luK %s  %s\n",
        start,
        (unsigned long)(diff>>10),
        flags,
        cp
      );
    }
    
  }

  if(x_option && !q_option){
    if(sizeof(KLONG)==8){
      printf("----------------  ------  ------  ------  ------\n");
      printf(
        "total kB %15ld       -       -       -\n",
        (total_shared + total_private_writeable + total_private_readonly) >> 10
      );
    }else{
      printf("-------- ------- ------- ------- -------\n");
      printf(
        "total kB %7ld       -       -       -\n",
        (total_shared + total_private_writeable + total_private_readonly) >> 10
      );
    }
  }
  if(d_option && !q_option){
      printf(
        "mapped %ldK    writeable/private: %ldK    shared: %ldK\n",
        (total_shared + total_private_writeable + total_private_readonly) >> 10,
        total_private_writeable >> 10,
        total_shared >> 10
      );
  }
  if(!x_option && !d_option && !q_option){
    if(sizeof(KLONG)==8) printf(" total %16ldK\n", (total_shared + total_private_writeable + total_private_readonly) >> 10);
    else                 printf(" total %8ldK\n",  (total_shared + total_private_writeable + total_private_readonly) >> 10);
  }

  return 0;
}


int main(int argc, char *argv[]){
  unsigned *pidlist;
  unsigned count = 0;
  unsigned u;
  int ret = 0;

  if(argc<2) usage();
  pidlist = malloc(sizeof(unsigned)*argc);  // a bit more than needed perhaps

  while(*++argv){
    if(!strcmp("--version",*argv)){
      V_option++;
      continue;
    }
    if(**argv=='-'){
      char *walk = *argv;
      if(!walk[1]) usage();
      while(*++walk){
        switch(*walk){
        case 'V':
          V_option++;
          break;
        case 'x':
          x_option++;
          break;
        case 'r':
          r_option++;
          break;
        case 'd':
          d_option++;
          break;
        case 'q':
          q_option++;
          break;
        default:
          usage();
        }
      }
    }else{
      char *walk = *argv;
      char *endp;
      unsigned long pid;
      if(!strncmp("/proc/",walk,6)){
        walk += 6;
        // user allowed to do: pmap /proc/*
        if(*walk<'0' || *walk>'9') continue;
      }
      if(*walk<'0' || *walk>'9') usage();
      pid = strtoul(walk, &endp, 0);
      if(pid<1ul || pid>0x7ffffffful || *endp) usage();
      pidlist[count++] = pid;
    }
  }

  if( (x_option|V_option|r_option|d_option|q_option) >> 1 ) usage(); // dupes
  if(V_option){
    if(count|x_option|r_option|d_option|q_option) usage();
    fprintf(stdout, "pmap (%s)\n", procps_version);
    return 0;
  }
  if(count<1) usage();   // no processes
  if(d_option && x_option) usage();

  u=0;
  while(u<count) ret |= one_proc(pidlist[u++]);

  return ret;
}
