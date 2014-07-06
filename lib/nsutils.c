#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "proc/readproc.h"
#include "nsutils.h"

#ifdef TEST_PROGRAM
const char *get_ns_name(int id)
{
	return NULL;
}
#endif		/* TEST_PROGRAM */

/* we need to fill in only namespace information */
int ns_read(pid_t pid, proc_t *ns_task)
{
	struct stat st;
	char buff[50];
	int i, rc = 0;

	for (i = 0; i < NUM_NS; i++) {
		snprintf(buff, sizeof(buff), "/proc/%i/ns/%s", pid,
			get_ns_name(i));
		if (stat(buff, &st)) {
			if (errno != ENOENT)
				rc = errno;
			ns_task->ns[i] = 0;
			continue;
		}
		ns_task->ns[i] = st.st_ino;
	}
	return rc;
}

#ifdef TEST_PROGRAM
#include <stdio.h>
int main(int argc, char *argv[])
{
	printf("Hello, World!\n");
	return EXIT_SUCCESS;
}
#endif			    /* TEST_PROGRAM */
