/* The shadow of the original with only common prototypes now. */
#include <stdio.h>
#include <sys/types.h>

/* The HZ constant from <asm/param.h> is replaced by the Hertz variable
 * available from "proc/sysinfo.h".
 */

/* get page info */
#include <asm/page.h>

void *xrealloc(void *oldp, unsigned int size);
void *xmalloc(unsigned int size);
void *xcalloc(void *pointer, int size);
       
int   mult_lvl_cmp(void* a, void* b);
int   node_mult_lvl_cmp(void* a, void* b);
       
char *user_from_uid(uid_t uid);
char *group_from_gid(gid_t gid);

const char * wchan(unsigned long address);
int   open_psdb(const char *override);
int   open_psdb_message(const char *override, void (*message)(const char *, ...));

unsigned print_str    (FILE* file, char *s, unsigned max);
unsigned print_strlist(FILE* file, char **strs, char* sep, unsigned max);
