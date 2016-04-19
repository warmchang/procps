/*
 * libprocps - Library to read proc filesystem
 * Tests for pids library calls
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
#include <errno.h>

#include <proc/procps.h>
#include "tests.h"

int check_pids_new_nullinfo(void *data)
{
    testname = "procps_pids_new() info=NULL returns -EINVAL";
    return (procps_pids_new(NULL, 0, NULL) == -EINVAL);
}

int check_pids_new_enum(void *data)
{
    testname = "procps_pids_new() items=enum returns -EINVAL";
    struct procps_pidsinfo *info;
    return (procps_pids_new(&info, 3, PROCPS_PIDS_noop) == -EINVAL);
}

int check_pids_new_toomany(void *data)
{
    struct procps_pidsinfo *info;
    enum pids_item items[] = { PROCPS_PIDS_ID_PID, PROCPS_PIDS_ID_PID };
    testname = "procps_pids_new() too many items returns -EINVAL";
    return (procps_pids_new(&info, 1, items) == -EINVAL);
}

TestFunction test_funcs[] = {
    check_pids_new_nullinfo,
    check_pids_new_enum,
    check_pids_new_toomany,
    NULL };

int main(int argc, char *argv[])
{
    return run_tests(test_funcs, NULL);
}


