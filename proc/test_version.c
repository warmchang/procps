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

#include <proc/version.h>
#include "tests.h"

int check_linux_version(void *data)
{
    testname = "procps_linux_version()";
    return (procps_linux_version() > 0);
}

TestFunction test_funcs[] = {
    check_linux_version,
    NULL
};

int main(int argc, char *argv[])
{
    return run_tests(test_funcs, NULL);
}


