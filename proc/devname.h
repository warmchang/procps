#include "procps.h"

#define ABBREV_DEV  1     /* remove /dev/         */
#define ABBREV_TTY  2     /* remove tty           */
#define ABBREV_PTS  4     /* remove pts/          */

extern unsigned dev_to_tty(char *restrict ret, unsigned chop, int dev, int pid, unsigned int flags);

extern int tty_to_dev(const char *restrict const name);
