
#include <stdio.h>
#include <stdlib.h>
#include "strutils.h"

struct strtod_tests {
    char *string;
    double result;
};

struct strtod_tests tests[] = {
    {"123",     123.0},
    {"-123",    -123.0},
    {"12.34",   12.34},
    {"-12.34",  -12.34},
    {".34",     0.34},
    {"-.34",    -0.34},
    {"12,34",   12.34},
    {"-12,34",  -12.34},
    {",34",     0.34},
    {"-,34",    -0.34},
    {"0",       0.0},
    {".0",      0.0},
    {"0.0",     0.0},
    {NULL, 0.0}
};



int main(int argc, char *argv[])
{
    int i;
    double val;

    for(i=0; tests[i].string != NULL; i++) {
        if(strtod_nol_or_err(tests[i].string, "Cannot parse number") !=
           tests[i].result) {
            fprintf(stderr, "FAIL: strtod_nol_or_err(\"%s\") != %f\n",
                    tests[i].string, tests[i].result);
            return EXIT_FAILURE;
        }
        //fprintf(stderr, "PASS: strtod_nol for %s\n", tests[i].string);
    }
    return EXIT_SUCCESS;
}
