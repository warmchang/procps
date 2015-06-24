#ifndef PROCPS_PROC_PWCACHE_H
#define PROCPS_PROC_PWCACHE_H

#include <sys/types.h>
#include <proc/procps.h>

__BEGIN_DECLS

// used in pwcache and in readproc to set size of username or groupname
#define P_G_SZ 33

char *user_from_uid(uid_t uid);
char *group_from_gid(gid_t gid);

__END_DECLS

#endif
