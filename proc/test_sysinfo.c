/*
 * libprocps - Library to read proc filesystem
 * Tests for sysinfo library calls
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdlib.h>
#include <stdio.h>

#include <proc/sysinfo.h>

struct test_func {
    int (*func)(void *data);
    char *name;
};

int check_hertz(void *data)
{
    long hz;

    hz =  procps_hertz_get();
    return (hz > 0);
}

int check_loadavg(void *data)
{
    double a,b,c;

    if (procps_loadavg(&a, &b, &c) == 0)
        return 1;
    return (a>0 && b>0 && c>0);
}

int check_loadavg_null(void *data)
{
    if (procps_loadavg(NULL, NULL, NULL) == 0)
        return 1;
    return 0;
}

struct test_func tests[] = {
    { check_hertz, "procps_hertz_get()"},
    { check_loadavg, "procps_loadavg()"},
    { check_loadavg, "procps_loadavg() with NULLs"},
    { NULL, NULL}
};

int main(int argc, char *argv[])
{
    int i;
    struct test_func *current;

    for(i=0; tests[i].func != NULL; i++) {
        current = &tests[i];
        if (!current->func(NULL)) {
            fprintf(stderr, "FAIL: %s\n", current->name);
            return EXIT_FAILURE;
        } else {
            fprintf(stderr, "PASS: %s\n", current->name);
        }
    }
    return EXIT_SUCCESS;
}


