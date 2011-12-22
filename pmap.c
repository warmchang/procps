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

#include <sys/ipc.h>
#include <sys/shm.h>

#include "proc/readproc.h"
#include "proc/version.h"
#include "proc/escape.h"

static void usage(void) NORETURN;
static void usage(void){
  fprintf(stderr,
    "Usage: pmap [-x | -d] [-q] [-A low,high] pid...\n"
    "-x  show details\n"
    "-d  show offset and device number\n"
    "-q  quiet; less header/footer info\n"
    "-V  show the version number\n"
    "-A  limit results to the given range\n"
  );
  exit(1);
}


static unsigned KLONG range_low;
static unsigned KLONG range_high = ~0ull;

static int V_option;
static int r_option;  // ignored -- for SunOS compatibility
static int x_option;
static int d_option;
static int q_option;

static unsigned shm_minor = ~0u;

static void discover_shm_minor(void){
  void *addr;
  int shmid;
  char mapbuf[256];

  if(!freopen("/proc/self/maps", "r", stdin)) return;

  // create
  shmid = shmget(IPC_PRIVATE, 42, IPC_CREAT | 0666);
  if(shmid==-1) return; // failed; oh well
  // attach
  addr = shmat(shmid, NULL, SHM_RDONLY);
  if(addr==(void*)-1) goto out_destroy;

  while(fgets(mapbuf, sizeof mapbuf, stdin)){
    char flags[32];
    char *tmp; // to clean up unprintables
    unsigned KLONG start, end;
    unsigned long long file_offset, inode;
    unsigned dev_major, dev_minor;
    sscanf(mapbuf,"%"KLF"x-%"KLF"x %31s %Lx %x:%x %Lu", &start, &end, flags, &file_offset, &dev_major, &dev_minor, &inode);
    tmp = strchr(mapbuf,'\n');
    if(tmp) *tmp='\0';
    tmp = mapbuf;
    while(*tmp){
      if(!isprint(*tmp)) *tmp='?';
      tmp++;
    }
    if(start > (unsigned long)addr) continue;
    if(dev_major) continue;
    if(flags[3] != 's') continue;
    if(strstr(mapbuf,"/SYSV")){
      shm_minor = dev_minor;
      break;
    }
  }

  if(shmdt(addr)) perror("shmdt");

out_destroy:
  if(shmctl(shmid, IPC_RMID, NULL)) perror("IPC_RMID");

  return;
}


static const char *mapping_name(proc_t *p, unsigned KLONG addr, unsigned KLONG len, const char *mapbuf, unsigned showpath, unsigned dev_major, unsigned dev_minor, unsigned long long inode){
  const char *cp;

  if(!dev_major && dev_minor==shm_minor && strstr(mapbuf,"/SYSV")){
    static char shmbuf[64];
    snprintf(shmbuf, sizeof shmbuf, "  [ shmid=0x%Lx ]", inode);
    return shmbuf;
  }

  cp = strrchr(mapbuf,'/');
  if(cp){
    if(showpath) return strchr(mapbuf,'/');
    return cp[1] ? cp+1 : cp;
  }

  cp = strchr(mapbuf,'/');
  if(cp){
    if(showpath) return cp;
    return strrchr(cp,'/') + 1;  // it WILL succeed
  }

  cp = "  [ anon ]";
  if( (p->start_stack >= addr) && (p->start_stack <= addr+len) )  cp = "  [ stack ]";
  return cp;
}

static int one_proc(proc_t *p){
  char buf[32];
  char mapbuf[9600];
  char cmdbuf[512];
  FILE *fp;
  unsigned long total_shared = 0ul;
  unsigned long total_private_readonly = 0ul;
  unsigned long total_private_writeable = 0ul;

  char *cp2=NULL;
  unsigned long long rss = 0ull;
  unsigned long long private_dirty = 0ull;
  unsigned long long shared_dirty = 0ull;
  unsigned long long total_rss = 0ull;
  unsigned long long total_private_dirty = 0ull;
  unsigned long long total_shared_dirty = 0ull;
  unsigned KLONG diff=0;

  // Overkill, but who knows what is proper? The "w" prog
  // uses the tty width to determine this.
  int maxcmd = 0xfffff;

  sprintf(buf,"/proc/%u/maps",p->tgid);
  if ( (fp = fopen(buf, "r")) == NULL) return 1;
  if (x_option) {
    sprintf(buf,"/proc/%u/smaps",p->tgid);
    if ( (fp = freopen(buf, "r", fp)) == NULL) return 1;
  }

  escape_command(cmdbuf, p, sizeof cmdbuf, &maxcmd, ESC_ARGS|ESC_BRACKETS);
  printf("%u:   %s\n", p->tgid, cmdbuf);

  if(!q_option && (x_option|d_option)){
    if(x_option){
      if(sizeof(KLONG)==4) printf("Address   Kbytes     RSS   Dirty Mode   Mapping\n");
      else         printf("Address           Kbytes     RSS   Dirty Mode   Mapping\n");
    }
    if(d_option){
      if(sizeof(KLONG)==4) printf("Address   Kbytes Mode  Offset           Device    Mapping\n");
      else         printf("Address           Kbytes Mode  Offset           Device    Mapping\n");
    }
  }

  while(fgets(mapbuf,sizeof mapbuf,fp)){
    char flags[32];
    char *tmp; // to clean up unprintables
    unsigned KLONG start, end;
    unsigned long long file_offset, inode;
    unsigned dev_major, dev_minor;
    unsigned long long smap_value;
    char smap_key[20];

    /* hex values are lower case or numeric, keys are upper */
    if (mapbuf[0] >= 'A' && mapbuf[0] <= 'Z') {
      /* Its a key */
      if (sscanf(mapbuf,"%20[^:]: %llu", smap_key, &smap_value) == 2) {
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
        if (strncmp("Swap", smap_key, 4) == 0) { /*doesnt matter as long as last*/
          printf(
            (sizeof(KLONG)==8)
              ? "%016"KLF"x %7lu %7llu %7llu %s  %s\n"
              :      "%08lx %7lu %7llu %7llu %s  %s\n",
            start,
            (unsigned long)(diff>>10),
            rss,
            (private_dirty + shared_dirty),
            flags,
            cp2
          );
          /* reset some counters */
          rss = shared_dirty = private_dirty = 0ull;
          diff=0;
          continue;
        }
        /* Other keys */
        continue;
      }
    }
    sscanf(mapbuf,"%"KLF"x-%"KLF"x %31s %Lx %x:%x %Lu", &start, &end, flags, &file_offset, &dev_major, &dev_minor, &inode);

    if(start > range_high)
      break;
    if(end < range_low)
      continue;

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
      flags[3] = '-';
      if(flags[1]=='w') total_private_writeable += diff;
      else              total_private_readonly  += diff;
    }

    // format used by Solaris 9 and procps-3.2.0+
    // an 'R' if swap not reserved (MAP_NORESERVE, SysV ISM shared mem, etc.)
    flags[4] = '-';
    flags[5] = '\0';

    if(x_option){
          cp2 = mapping_name(p, start, diff, mapbuf, 0, dev_major, dev_minor, inode);
      /* printed with the keys */
      continue;
    }
    if(d_option){
      const char *cp = mapping_name(p, start, diff, mapbuf, 0, dev_major, dev_minor, inode);
      printf(
        (sizeof(KLONG)==8)
          ? "%016"KLF"x %7lu %s %016Lx %03x:%05x %s\n"
          :      "%08lx %7lu %s %016Lx %03x:%05x %s\n",
        start,
        (unsigned long)(diff>>10),
        flags,
        file_offset,
        dev_major, dev_minor,
        cp
      );
    }
    if(!x_option && !d_option){
      const char *cp = mapping_name(p, start, diff, mapbuf, 1, dev_major, dev_minor, inode);
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




  if(!q_option){
    if(x_option){
      if(sizeof(KLONG)==8){
        printf("----------------  ------  ------  ------\n");
        printf(
          "total kB %15ld %7llu %7llu\n",
          (total_shared + total_private_writeable + total_private_readonly) >> 10,
          total_rss, (total_shared_dirty+total_private_dirty)

        );
      }else{
        printf("-------- ------- ------- ------- -------\n");
        printf(
          "total kB %7ld       -       -       -\n",
          (total_shared + total_private_writeable + total_private_readonly) >> 10
        );
      }
    }
    if(d_option){
        printf(
          "mapped: %ldK    writeable/private: %ldK    shared: %ldK\n",
          (total_shared + total_private_writeable + total_private_readonly) >> 10,
          total_private_writeable >> 10,
          total_shared >> 10
        );
    }
    if(!x_option && !d_option){
      if(sizeof(KLONG)==8) printf(" total %16ldK\n", (total_shared + total_private_writeable + total_private_readonly) >> 10);
      else                 printf(" total %8ldK\n",  (total_shared + total_private_writeable + total_private_readonly) >> 10);
    }
  }

  return 0;
}


int main(int argc, char *argv[]){
  unsigned *pidlist;
  unsigned count = 0;
  PROCTAB* PT;
  proc_t p;
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
        case 'A':{
            char *arg1;
            if(walk[1]){
              arg1 = walk+1;
              walk += strlen(walk)-1;
            }else{
              arg1 = *++argv;
              if(!arg1)
                usage();
            }
            char *arg2 = strchr(arg1,',');
            if(arg2)
              *arg2 = '\0';
            if(arg2) ++arg2;
            else arg2 = arg1;
            
            if(*arg1)
              range_low = STRTOUKL(arg1,&arg1,16);
            if(*arg2)
              range_high = STRTOUKL(arg2,&arg2,16);
            if(*arg1 || *arg2)
              usage();
          }
          break;
        case 'a': // Sun prints anon/swap reservations
        case 'F': // Sun forces hostile ptrace-like grab
        case 'l': // Sun shows unresolved dynamic names
        case 'L': // Sun shows lgroup info
        case 's': // Sun shows page sizes
        case 'S': // Sun shows swap reservations
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

  discover_shm_minor();

  memset(&p, '\0', sizeof(p));
  pidlist[count] = 0;  // old libproc interface is zero-terminated
  PT = openproc(PROC_FILLSTAT|PROC_FILLARG|PROC_PID, pidlist);
  while(readproc(PT, &p)){
    ret |= one_proc(&p);
    count--;
  }
  closeproc(PT);

  if(count) ret |= 42;  // didn't find all processes asked for
  return ret;
}
