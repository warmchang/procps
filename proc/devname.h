#define ABBREV_DEV  1     /* remove /dev/         */
#define ABBREV_TTY  2     /* remove tty           */
#define ABBREV_PTS  4     /* remove pts/          */

extern int dev_to_tty(char *ret, int chop, int dev, int pid, unsigned int flags);

extern int tty_to_dev(char *name);
