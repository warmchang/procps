
#ifndef PROCPS_NG_TESTS_H
#define PROCPS_NG_TESTS_H

struct test_func {
    int (*func)(void *data);
    char *name;
};

#endif
