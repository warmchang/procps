/*
 * libprocps - Library to read proc filesystem
 * Tests for version library calls
 *
 * Copyright 2016 Craig Small <csmall@enc.com.au>
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

#include <proc/procps.h>
#include "tests.h"

int check_uptime(void *data)
{
    double up=0, idle=0;
    int rc;
    rc = procps_uptime(&up, &idle);
    return (rc > 0 && up > 0 && idle > 0);
}

int check_uptime_nullup(void *data)
{
    double idle=0;
    int rc;
    rc = procps_uptime(NULL, &idle);
    return (rc > 0 && idle > 0);
}

int check_uptime_nullidle(void *data)
{
    double up=0;
    int rc;
    rc = procps_uptime(&up, NULL);
    return (rc > 0 && up > 0);
}

int check_uptime_nullall(void *data)
{
    int rc;
    rc = procps_uptime(NULL, NULL);
    return (rc > 0);
}

int check_uptime_sprint(void *data)
{
    char *str;

    str = procps_uptime_sprint();

    return (str != NULL && str[0] != '\0');
}

int check_uptime_sprint_short(void *data)
{
    char *str;

    str = procps_uptime_sprint_short();

    return (str != NULL && str[0] != '\0');
}

struct test_func tests[] = {
    { check_uptime, "procps_uptime()"},
    { check_uptime_nullup, "procps_uptime() (up=NULL)"},
    { check_uptime_nullidle, "procps_uptime() (idle=NULL)"},
    { check_uptime_nullall, "procps_uptime() (up,idle=NULL)"},
    { check_uptime_sprint, "procps_uptime_sprint()"},
    { check_uptime_sprint_short, "procps_uptime_sprint_short()"},
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


