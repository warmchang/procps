/* The shadow of the original with only common prototypes now. */
#include <stdio.h>
#include <sys/types.h>

/* The HZ constant from <asm/param.h> is replaced by the Hertz variable
 * available from "proc/sysinfo.h".
 */

/* get page info */
#include <asm/page.h>

extern void *xrealloc(void *oldp, unsigned int size);
extern void *xmalloc(unsigned int size);
extern void *xcalloc(void *pointer, int size);
       
extern int   mult_lvl_cmp(void* a, void* b);
extern int   node_mult_lvl_cmp(void* a, void* b);
       
extern char *user_from_uid(uid_t uid);
extern char *group_from_gid(gid_t gid);

extern const char * wchan(unsigned long address);
extern int   open_psdb(const char *override);
extern int   open_psdb_message(const char *override, void (*message)(const char *, ...));

extern unsigned print_str    (FILE* file, char *s, unsigned max);
extern unsigned print_strlist(FILE* file, char **strs, char* sep, unsigned max);
