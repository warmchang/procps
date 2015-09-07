/*
 * libprocps - Library to read proc filesystem
 * Tests for namespace library calls
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
#include <string.h>

#include <proc/namespace.h>

struct test_func {
    int (*func)(void *data);
    char *name;
};

int check_name_minus(void *data)
{
    return (procps_ns_get_name(-1) == NULL);
}

int check_name_over(void *data)
{
    return (procps_ns_get_name(999) == NULL);
}

int check_name_ipc(void *data)
{
    return (strcmp(procps_ns_get_name(PROCPS_NS_IPC),"ipc")==0);
}

int check_id_null(void *data)
{
    return (procps_ns_get_id(NULL) < 0);
}

int check_id_unfound(void *data)
{
    return (procps_ns_get_id("foobar") < 0);
}

int check_id_mnt(void *data)
{
    return (procps_ns_get_id("mnt") == PROCPS_NS_MNT);
}

struct test_func tests[] = {
    { check_name_minus, "procps_ns_get_name() negative id"},
    { check_name_over, "procps_ns_get_name() id over limit"},
    { check_name_ipc, "procps_ns_get_name() ipc"},
    { check_id_null, "procps_ns_get_id(NULL)"},
    { check_id_unfound, "procps_ns_get_id(unknown)"},
    { check_id_mnt, "procps_ns_get_id(mnt)"},
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


