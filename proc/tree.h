/* for oldps.c and proc/compare.c */
struct tree_node {
    proc_t *proc;
    pid_t pid;
    pid_t ppid;
    char *line;
    char *cmd;
    char **cmdline;
    char **environ;
    int children;
    int maxchildren;
    int *child;
    int have_parent;
};

