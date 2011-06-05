
/*
 * Sysctl 1.01 - A utility to read and manipulate the sysctl parameters
 *
 *
 * "Copyright 1999 George Staikos
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details."
 *
 * Changelog:
 *            v1.01:
 *                   - added -p <preload> to preload values from a file
 *            Horms: 
 *                   - added -q to be quiet when modifying values
 *
 * Changes by Albert Cahalan, 2002.
 */


#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "proc/procps.h"
#include "proc/version.h"


/* Proof that C++ causes brain damage: */
typedef int bool;
static bool true  = 1;
static bool false = 0;

/*
 *    Globals...
 */

static const char PROC_PATH[] = "/proc/sys/";
static const char DEFAULT_PRELOAD[] = "/etc/sysctl.conf";
static bool NameOnly;
static bool PrintName;
static bool PrintNewline;
static bool IgnoreError;
static bool Quiet;

/* error messages */
static const char ERR_MALFORMED_SETTING[] = "error: Malformed setting \"%s\"\n";
static const char ERR_NO_EQUALS[] = "error: \"%s\" must be of the form name=value\n";
static const char ERR_INVALID_KEY[] = "error: \"%s\" is an unknown key\n";
static const char ERR_UNKNOWN_WRITING[] = "error: \"%s\" setting key \"%s\"\n";
static const char ERR_UNKNOWN_READING[] = "error: \"%s\" reading key \"%s\"\n";
static const char ERR_PERMISSION_DENIED[] = "error: permission denied on key '%s'\n";
static const char ERR_OPENING_DIR[] = "error: unable to open directory \"%s\"\n";
static const char ERR_PRELOAD_FILE[] = "error: unable to open preload file \"%s\"\n";
static const char WARN_BAD_LINE[] = "warning: %s(%d): invalid syntax, continuing...\n";


static void slashdot(char *restrict p, char old, char new){
  p = strpbrk(p,"/.");
  if(!p) return;            /* nothing -- can't be, but oh well */
  if(*p==new) return;       /* already in desired format */
  while(p){
    char c = *p;
    if(c==old) *p=new;
    if(c==new) *p=old;
    p = strpbrk(p+1,"/.");
  }
}



/*
 *     Display the usage format
 *
 */
static void __attribute__ ((__noreturn__))
    Usage(FILE * out)
{
   fprintf(out,
	   "\nUsage: %s [options] [variable[=value] ...]\n"
	   "\nOptions:\n", program_invocation_short_name);
   fprintf(out,
	   "  -a, --all            display all variables\n"
	   "  -A                   alias of -a\n"
	   "  -X                   alias of -a\n"
	   "  -b, --binary         print value without new line\n"
	   "  -e, --ignore         ignore unknown variables errors\n"
	   "  -N, --names          print variable names without values\n"
	   "  -n, --values         print only values of a variables\n"
	   "  -p, --load[=<file>]  read values from file\n"
	   "  -f                   alias of -p\n"
	   "  -q, --quiet          do not echo variable set\n"
	   "  -w, --write          enable writing a value to variable\n"
	   "  -o                   does nothing\n"
	   "  -x                   does nothing\n"
	   "  -h, --help           display this help text\n"
	   "  -d                   alias of -h\n"
	   "  -V, --version        display version information and exit\n");
   fprintf(out, "\nFor more information see sysctl(8).\n");

   exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 *     Strip the leading and trailing spaces from a string
 *
 */
static char *StripLeadingAndTrailingSpaces(char *oneline) {
   char *t;

   if (!oneline || !*oneline)
      return oneline;

   t = oneline;
   t += strlen(oneline)-1;

   while ((*t==' ' || *t=='\t' || *t=='\n' || *t=='\r') && t!=oneline)
      *t-- = 0;

   t = oneline;

   while ((*t==' ' || *t=='\t') && *t!=0)
      t++;

   return t;
}

static int DisplayAll(const char *restrict const path);

/*
 *     Read a sysctl setting 
 *
 */
static int ReadSetting(const char *restrict const name) {
   int rc = 0;
   char *restrict tmpname;
   char *restrict outname;
   char inbuf[1025];
   FILE *restrict fp;
   struct stat ts;

   if (!name || !*name) {
      fprintf(stderr, ERR_INVALID_KEY, name);
      return -1;
   }

   /* used to open the file */
   tmpname = malloc(strlen(name)+strlen(PROC_PATH)+2);
   strcpy(tmpname, PROC_PATH);
   strcat(tmpname, name); 
   slashdot(tmpname+strlen(PROC_PATH),'.','/'); /* change . to / */

   /* used to display the output */
   outname = strdup(name);
   slashdot(outname,'/','.'); /* change / to . */

   if (stat(tmpname, &ts) < 0) {
      if (!IgnoreError) {
         perror(tmpname);
         rc = -1;
      }
      goto out;
   }
   if ((ts.st_mode & S_IRUSR) == 0)
      goto out;

   if (S_ISDIR(ts.st_mode)) {
      size_t len;
      len = strlen(tmpname);
      tmpname[len] = '/';
      tmpname[len+1] = '\0';
      rc = DisplayAll(tmpname);
      goto out;
   }

   fp = fopen(tmpname, "r");

   if (!fp) {
      switch(errno) {
      case ENOENT:
         if (!IgnoreError) {
            fprintf(stderr, ERR_INVALID_KEY, outname);
            rc = -1;
         }
         break;
      case EACCES:
         fprintf(stderr, ERR_PERMISSION_DENIED, outname);
         rc = -1;
         break;
      default:
         fprintf(stderr, ERR_UNKNOWN_READING, strerror(errno), outname);
         rc = -1;
         break;
      }
   } else {
      errno = 0;
      if(fgets(inbuf, sizeof inbuf - 1, fp)) {
         // this loop is required, see
         // /sbin/sysctl -a | egrep -6 dev.cdrom.info
         do {
            if (NameOnly) {
               fprintf(stdout, "%s\n", outname);
            } else {
               /* already has the \n in it */
               if (PrintName) {
                  fprintf(stdout, "%s = %s", outname, inbuf);
               } else {
                  if (!PrintNewline) {
                    char *nlptr = strchr(inbuf,'\n');
                    if(nlptr) *nlptr='\0';
                  }
                  fprintf(stdout, "%s", inbuf);
               }
            }
         } while(fgets(inbuf, sizeof inbuf - 1, fp));
      } else {
         switch(errno) {
         case EACCES:
            fprintf(stderr, ERR_PERMISSION_DENIED, outname);
            rc = -1;
            break;
         case EISDIR:{
            size_t len;
            len = strlen(tmpname);
            tmpname[len] = '/';
            tmpname[len+1] = '\0';
            fclose(fp);
            rc = DisplayAll(tmpname);
            goto out;
         }
         default:
            fprintf(stderr, ERR_UNKNOWN_READING, strerror(errno), outname);
            rc = -1;
         case 0:
            break;
         }
      }
      fclose(fp);
   }
out:
   free(tmpname);
   free(outname);
   return rc;
}



/*
 *     Display all the sysctl settings 
 *
 */
static int DisplayAll(const char *restrict const path) {
   int rc = 0;
   int rc2;
   DIR *restrict dp;
   struct dirent *restrict de;
   struct stat ts;

   dp = opendir(path);

   if (!dp) {
      fprintf(stderr, ERR_OPENING_DIR, path);
      rc = -1;
   } else {
      readdir(dp);  // skip .
      readdir(dp);  // skip ..
      while (( de = readdir(dp) )) {
         char *restrict tmpdir;
         tmpdir = (char *restrict)malloc(strlen(path)+strlen(de->d_name)+2);
         sprintf(tmpdir, "%s%s", path, de->d_name);
         rc2 = stat(tmpdir, &ts);
         if (rc2 != 0) {
            perror(tmpdir);
         } else {
            if (S_ISDIR(ts.st_mode)) {
               strcat(tmpdir, "/");
               DisplayAll(tmpdir);
            } else {
               rc |= ReadSetting(tmpdir+strlen(PROC_PATH));
            }
         }
         free(tmpdir);
      }
      closedir(dp);
   }
   return rc;
}


/*
 *     Write a sysctl setting 
 *
 */
static int WriteSetting(const char *setting) {
   int rc = 0;
   const char *name = setting;
   const char *value;
   const char *equals;
   char *tmpname;
   char *outname;
   FILE *fp;
   struct stat ts;

   if (!name) {        /* probably don't want to display this err */
      return 0;
   } /* end if */

   equals = strchr(setting, '=');
 
   if (!equals) {
      fprintf(stderr, ERR_NO_EQUALS, setting);
      return -1;
   }

   value = equals + 1;      /* point to the value in name=value */   

   if (!*name || !*value || name == equals) { 
      fprintf(stderr, ERR_MALFORMED_SETTING, setting);
      return -2;
   }

   /* used to open the file */
   tmpname = malloc(equals-name+1+strlen(PROC_PATH));
   strcpy(tmpname, PROC_PATH);
   strncat(tmpname, name, (int)(equals-name)); 
   tmpname[equals-name+strlen(PROC_PATH)] = 0;
   slashdot(tmpname+strlen(PROC_PATH),'.','/'); /* change . to / */

   /* used to display the output */
   outname = malloc(equals-name+1);                       
   strncpy(outname, name, (int)(equals-name)); 
   outname[equals-name] = 0;
   slashdot(outname,'/','.'); /* change / to . */
 
   if (stat(tmpname, &ts) < 0) {
      if (!IgnoreError) {
         perror(tmpname);
         rc = -1;
      }
      goto out;
   }

   if ((ts.st_mode & S_IWUSR) == 0) {
      fprintf(stderr, ERR_UNKNOWN_WRITING, strerror(EACCES), outname);
      goto out;
   }

   if (S_ISDIR(ts.st_mode)) {
      fprintf(stderr, ERR_UNKNOWN_WRITING, strerror(EACCES), outname);
      goto out;
   }

   fp = fopen(tmpname, "w");

   if (!fp) {
      switch(errno) {
      case ENOENT:
         if (!IgnoreError) {
            fprintf(stderr, ERR_INVALID_KEY, outname);
            rc = -1;
         }
         break;
      case EACCES:
         fprintf(stderr, ERR_PERMISSION_DENIED, outname);
         rc = -1;
         break;
      default:
         fprintf(stderr, ERR_UNKNOWN_WRITING, strerror(errno), outname);
         rc = -1;
         break;
      }
   } else {
      rc = fprintf(fp, "%s\n", value);
      if (rc < 0) {
         fprintf(stderr, ERR_UNKNOWN_WRITING, strerror(errno), outname);
         fclose(fp);
      } else {
         rc=fclose(fp);
         if (rc != 0) 
            fprintf(stderr, ERR_UNKNOWN_WRITING, strerror(errno), outname);
      }
      if (rc==0 && !Quiet) {
         if (NameOnly) {
            fprintf(stdout, "%s\n", outname);
         } else {
            if (PrintName) {
               fprintf(stdout, "%s = %s\n", outname, value);
            } else {
               if (PrintNewline)
                  fprintf(stdout, "%s\n", value);
               else
                  fprintf(stdout, "%s", value);
            }
         }
      }
   }
out:
   free(tmpname);
   free(outname);
   return rc;
}



/*
 *     Preload the sysctl's from the conf file
 *           - we parse the file and then reform it (strip out whitespace)
 *
 */
static int Preload(const char *restrict const filename) {
   char oneline[256];
   char buffer[256];
   FILE *fp;
   char *t;
   int n = 0;
   int rc = 0;
   char *name, *value;

   fp = (filename[0]=='-' && !filename[1])
      ? stdin
      : fopen(filename, "r")
   ;

   if (!fp) {
      fprintf(stderr, ERR_PRELOAD_FILE, filename);
      return -1;
   }

   while (fgets(oneline, sizeof oneline, fp)) {
      n++;
      t = StripLeadingAndTrailingSpaces(oneline);

      if (strlen(t) < 2)
         continue;

      if (*t == '#' || *t == ';')
         continue;

      name = strtok(t, "=");
      if (!name || !*name) {
         fprintf(stderr, WARN_BAD_LINE, filename, n);
         continue;
      }

      StripLeadingAndTrailingSpaces(name);

      value = strtok(NULL, "\n\r");
      if (!value || !*value) {
         fprintf(stderr, WARN_BAD_LINE, filename, n);
         continue;
      }

      while ((*value == ' ' || *value == '\t') && *value != 0)
         value++;

      // should NameOnly affect this?
      sprintf(buffer, "%s=%s", name, value);
      rc |= WriteSetting(buffer);
   }

   fclose(fp);
   return rc;
}



/*
 *    Main... 
 *
 */
int main(int argc, char *argv[])
{
   bool SwitchesAllowed = true;
   bool WriteMode = false;
   bool DisplayAllOpt = false;
   bool preloadfileOpt = false;
   int ReturnCode = 0;
   int c;
   const char *preloadfile = DEFAULT_PRELOAD;

    static const struct option longopts[] = {
       {"all", no_argument, NULL, 'a'},
       {"binary", no_argument, NULL, 'b'},
       {"ignore", no_argument, NULL, 'e'},
       {"names", no_argument, NULL, 'N'},
       {"values", no_argument, NULL, 'n'},
       {"load", optional_argument, NULL, 'p'},
       {"quiet", no_argument, NULL, 'q'},
       {"write", no_argument, NULL, 'w'},
       {"help", no_argument, NULL, 'h'},
       {"version", no_argument, NULL, 'V'},
       {NULL, 0, NULL, 0}
    };

   PrintName = true;
   PrintNewline = true;
   IgnoreError = false;
   Quiet = false;

   if (argc < 2) {
       Usage(stderr);
   }

   while ((c =
           getopt_long(argc, argv, "bneNwfpqoxaAXVdh", longopts,
                       NULL)) != -1)
      switch (c) {
      case 'b':
         /* This is "binary" format, which means more for BSD. */
         PrintNewline = false;
         /* FALL THROUGH */
      case 'n':
           PrintName = false;
        break;
      case 'e':
      /*
       * For FreeBSD, -e means a "%s=%s\n" format. ("%s: %s\n" default)
       * We (and NetBSD) use "%s = %s\n" always, and -e to ignore errors.
       */
           IgnoreError = true;
        break;
      case 'N':
           NameOnly = true;
        break;
      case 'w':
           SwitchesAllowed = false;
           WriteMode = true;
        break;
      case 'f':	/* the NetBSD way */
      case 'p':
           preloadfileOpt = true;
           if (optarg)
              preloadfile = optarg;
           break;
      case 'q':
           Quiet = true;
        break;
      case 'o':		/* BSD: binary values too, 1st 16 bytes in hex */
      case 'x':		/* BSD: binary values too, whole thing in hex */
           /* does nothing */ ;
        break;
      case 'a':         /* string and integer values (for Linux, all of them) */
      case 'A':         /* same as -a -o */
      case 'X':         /* same as -a -x */
           DisplayAllOpt = true;
           break;
      case 'V':
           fprintf(stdout, "sysctl (%s)\n",procps_version);
           exit(0);
      case 'd':         /* BSD: print description ("vm.kvm_size: Size of KVM") */
      case 'h':         /* BSD: human-readable (did FreeBSD 5 make -e default?) */
      case '?':
           Usage(stdout);
      default:
           Usage(stderr);
      }
    if (DisplayAllOpt)
       return DisplayAll(PROC_PATH);
    if (preloadfileOpt)
       return Preload(preloadfile);
 
    argc -= optind;
    argv += optind;
 
    if (argc < 1) {
       warnx("no variables specified");
       Usage(stderr);
    }
    if (NameOnly && Quiet) {
       warnx("options -N and -q can not coexist");
       Usage(stderr);
    }

    if (WriteMode || index(*argv, '='))
       ReturnCode = WriteSetting(*argv);
    else
       ReturnCode = ReadSetting(*argv);

   return ReturnCode;
}


