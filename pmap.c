#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

void usage(void){
  fprintf(stderr,
    "Usage: pmap [-r] [-x] pid...\n"
    "-r  ignored (compatibility option)\n"
    "-x  show details\n"
  );
  exit(1);
}

int one_proc(unsigned pid){
  char cmdbuf[64];
  char buf[32];
  int fd;
  sprintf(buf,"/proc/%u/cmdline",pid);
  if( ((fd=open(buf)) == -1) return 1;
  count = read(fd, cmdbuf, sizeof(cmdbuf)-1);
  if(count<1) return 1;
  cmdbuf[count] = '\0';
  while(count--) if(!isprint(cmdbuf[count])) cmdbuf[count]=' ';
  close(fd);
  sprintf(buf,"/proc/%u/maps",pid);
  if( ((fd=open(buf)) == -1) return 1;
  printf("%u:   %s\n", pid, cmdbuf);
  if(x_option)
    printf("Address   kB     Resident Shared Private Permissions       Name\n");
@@@ FIXME FIXME FIXME @@@  
}

int main(int argc, char *argv[]){
}
