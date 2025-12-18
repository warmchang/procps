/*
 * libproc2 - Library to read proc filesystem
 * Tests for escape library calls
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
#include <string.h>
#include <stdio.h>

#include "tests.h"

#include "library/escape.c"

#define MAXTBL(t) (int)( sizeof(t) / sizeof(t[0]) )


int check_ascii_untouched (void *data) {
    const char test_chars[][5] = {
        "A", "Z", "a", "z", "]", "~", "\x40"  };
    char test_dst[5];
    int i, n;

    testname = "escape: check ascii untouched";
    for (i = 0; i < MAXTBL(test_chars); i++) {
        n = snprintf(test_dst, sizeof(test_dst), "%s", test_chars[i]);
        u8charlen((unsigned char *)test_dst, n);
//printf("%s: src:%s  dst:%s\n", __func__, test_chars[i], test_dst);
        if (strcmp(test_chars[i], test_dst) != 00)
            return 0;
    }
//printf("\n\n");
    return 1;
}


int check_none_escaped (void *data) {
    const char test_chars[][5] = {
        "\x44", "\u0188", "\u07ff",
        "\u0800", "\u8888", "\ud7ff", "\uffff",
        "\U00010000", "\U00018888", "\U0010ffff",
        "\xe2\x99\xa5", "\xf0\x9f\x8c\x8e",
        "\xf0\x9f\x9a\xab" };
    char test_dst[5];
    int i, n;

    testname = "escape: check none escaped";
    for (i = 0; i < MAXTBL(test_chars); i++) {
        n = snprintf(test_dst, sizeof(test_dst), "%s", test_chars[i]);
        u8charlen((unsigned char *)test_dst, n);
//printf("%s: src:%s  dst:%s\n", __func__, test_chars[i], test_dst);
        if (strcmp(test_chars[i], test_dst))
            return 0;
    }
//printf("\n\n");
    return 1;
}


int check_all_escaped (void *data) {
    const char test_chars[][5] = {
        "\x7f", "\x80", "\xbf", "\xc0", "\xc1", "\xc1\xbf",
        "\xc2\x80",
        "\xe0\x9f\xbf", "\xed\xa0\x80", "\xed\xbf\xbf",
        "\xee\x00\x80","\xee\x80", "\xdf\x7f",
        "\xfc\x8f\xbf\xbf", "\xfc\xbf\xbf", "\xf0\x80\xa0\xa0",
        "\xf0\x8f\xbf\xbf", "\xf4\x90\x80\x80",
        "\ue000", "\uf8ff",                    // main PUA    begin/end
        "\U000F0000", "\U000FFFFD",            // supp PUA-A  begin/end
        "\U00100000", "\U0010FFFD" };          // supp PUA-B  begin/end
    char test_dst[50];
    int i, j, n;

    testname = "escape: check all escaped";
    for (i = 0; i < MAXTBL(test_chars); i++) {
        n = snprintf(test_dst, sizeof(test_dst), "%s", test_chars[i]);
        u8charlen((unsigned char *)test_dst, n);
//printf("%s: src:%s  dst:%s\n", __func__, test_chars[i], test_dst);
        for (j = 0; j < n; j++)
            if (test_dst[j] != '?')
                return 0;
    }
//printf("\n\n");
    return 1;
}


int check_some_escaped (void *data) {
    // Array of input,expected_output pairs
    char test_strs[][2][20] = {
        { "A", "A" },                               // A
        { "\x7f B", "? B" },                        // DEL
        { "\xe2\x82\xac C", "\u20ac C"},            // Euro symbol
        { "\x90\x20\x70 D", "?\x20\x70 D"},         // C1 controls
        { "\xF0\x9F\x98\x8A E",  "\U0001f60a E" },  // smilie
        { "\x90\x24\x71\x30\x76\xc2\x9c\xc2\x24\x71\x20\x69 F",
            "?$q0v???$q i F"}, //C1 control from perl example
        { "\e[1;31m G", "?[1;31m G"}, // ANSI color sequence
        { "\xdf\xbf H", "\u07ff H"}, // end boundary
        { "\x80\xbf\x41 I", "??A I"} // wrong continuation bytes
    };
    char test_dst[50];
    int i, n;

    testname = "escape: check some escaped";
    for (i = 0; i < MAXTBL(test_strs); i++) {
        n = snprintf(test_dst, sizeof(test_dst), "%s", test_strs[i][0]);
        u8charlen((unsigned char *)test_dst, n);
//printf("%s: inout \"%s\"  -->  output \"%s\"\n", __func__, test_strs[i][0], test_dst);
        if (strcmp(test_strs[i][1], test_dst) != 0)
            return 0;
    }
//printf("\n");
    return 1;
};

TestFunction test_funcs[] = {
    check_ascii_untouched,
    check_none_escaped,
    check_all_escaped,
    check_some_escaped,
    NULL
};

int main(int argc, char *argv[])
{
    return run_tests(test_funcs, NULL);
}
