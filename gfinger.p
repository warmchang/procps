Return-Path: <qpliu@ulch.Princeton.EDU>
Date: Wed, 24 Feb 93 23:37:45 -0500
From: qpliu@ulch.Princeton.EDU (Quowong P Liu)
To: johnsonm@stolaf.edu
Subject: Suggestions for procps
Reply-To: qpliu@princeton.edu

[...]

Since you are collecting programs that use the proc-fs, I have included
some patches to GNU finger that allow it to read the proc information
that it uses.

[...]

Here the diffs for GNU finger begin.
-------------BEGIN---------------
diff -ru finger-1.37.orig/lib/os.c finger-1.37/lib/os.c
--- finger-1.37.orig/lib/os.c	Thu Oct 22 17:01:10 1992
+++ finger-1.37/lib/os.c	Mon Feb  8 00:56:32 1993
@@ -23,7 +23,9 @@
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/file.h>
+#ifndef linux
 #include <sys/acct.h>
+#endif
 #include <time.h>
 #include <packet.h>
 
@@ -43,9 +45,11 @@
 
 #ifdef HAVE_PROC_FS
 #include <sys/signal.h>
+#ifndef linux
 #include <sys/fault.h>
 #include <sys/syscall.h>
 #include <sys/procfs.h>
+#endif /*linux*/
 #endif
 
 #include <general.h>
@@ -193,6 +197,7 @@
   long idle, current_time, get_last_access ();
   char *hostname, *ttyloc, *tty_location ();
 #ifdef HAVE_PROC_FS
+#ifndef linux
   prpsinfo_t procinfo;
   struct procent
     {
@@ -202,6 +207,16 @@
       char fname[sizeof (procinfo.pr_fname)];
     }
   *proctable, *tableptr;
+#else
+  struct procent
+    {
+      time_t start;
+      int uid;
+      dev_t tty;
+      char fname[15];
+    }
+  *proctable, *tableptr;
+#endif /* linux */
   int nprocs = 0;
 #endif /* HAVE_PROC_FS */
       
@@ -235,6 +250,7 @@
      searching through user list. */
 
 #ifdef HAVE_PROC_FS
+#ifndef linux
   {
     extern char *savedir ();
     char *dirstring = savedir ("/proc", 2*1024, &nprocs);
@@ -280,6 +296,64 @@
 	free (dirstring);
       }
   }
+#else /* linux */
+  {
+
+    extern char *savedir ();
+    char *dirstring = savedir ("/proc", 2*1024, &nprocs);
+    char *proc;
+    int procfd;
+    char procfile[17], procstatbuffer[1024];
+    struct stat proc_fs_stat;
+    char procstate;
+    int tmp_tty; /* dev_t is unsigned, -1 means no controlling tty */
+    
+    /* Allocate process table */
+    if (nprocs)
+      proctable = (struct procent *) xmalloc (sizeof *proctable * nprocs);
+
+    /* Walk processes */
+    if (dirstring && nprocs)
+      {
+	for (tableptr = proctable, proc = dirstring;
+	     *proc; proc += strlen (proc) + 1)
+	  {
+	    sprintf (procfile, "/proc/%.5s/stat", proc);
+
+	    if ((procfd = open (procfile, O_RDONLY)) >= 0
+		&& read (procfd, procstatbuffer, 1024))
+	      {
+		/* see /usr/src/linux/fs/proc/array.c
+		 * this is for 0.99.4
+		 */
+		sscanf (procstatbuffer, "%*d (%14s %c %*d %*d %*d %d "
+			"%*d %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u "
+			"%*u %hd" /* " %*u %*u %*u %*u %*u %*u %*u %*u %*d %*d "
+			"%*d %*d %*u" */,
+			tableptr->fname, &procstate, &tmp_tty,
+			&tableptr->start);
+		tableptr->fname[strlen(tableptr->fname)-1] = 0; /*trailing )*/
+		tableptr->tty = tmp_tty;
+		if (fstat (procfd, &proc_fs_stat) == 0)
+			tableptr->uid = proc_fs_stat.st_uid;
+
+		/* Only collect non-zombies with controlling tty */
+		if (tmp_tty > 0 && procstate != 'Z')
+		  {
+		    tableptr++;
+		  }
+		else
+		  nprocs--;
+		
+		close (procfd);
+	      }
+	    else
+	      nprocs--;
+	  }
+	free (dirstring);
+      }
+  }
+#endif /* linux */
 #endif /* HAVE_PROC_FS */
 
 
@@ -332,10 +406,14 @@
 
 	    if (stat (device_name, &finfo) != -1)
 	      {
+#ifdef linux
+		dev_t tty = finfo.st_rdev&0xff;
+#else
 #ifdef HAVE_ST_RDEV
 		dev_t tty = finfo.st_rdev;
 #else
 		dev_t tty = finfo.st_dev;
+#endif
 #endif
 		int file;
 
diff -ru finger-1.37.orig/lib/packet.c finger-1.37/lib/packet.c
--- finger-1.37.orig/lib/packet.c	Wed Oct 21 18:04:38 1992
+++ finger-1.37/lib/packet.c	Mon Feb  8 01:10:00 1993
@@ -348,7 +348,9 @@
 	  perror (filename);
     }
 
+#ifndef linux /* doesn't have fsync (as of .99.4) */
   fsync (file);
+#endif
   close (file);
 }
 
diff -ru finger-1.37.orig/lib/savedir.c finger-1.37/lib/savedir.c
--- finger-1.37.orig/lib/savedir.c	Fri Oct 16 21:52:40 1992
+++ finger-1.37/lib/savedir.c	Mon Feb  8 01:06:47 1993
@@ -132,7 +132,7 @@
 char *
 stpcpy (dest, source)
      char *dest;
-     char *source;
+     const char *source; /* gcc insists on prototype consistency */
 {
   while ((*dest++ = *source++) != '\0')
     /* Do nothing. */ ;
diff -ru finger-1.37.orig/src/finger.c finger-1.37/src/finger.c
--- finger-1.37.orig/src/finger.c	Wed Oct 21 20:41:13 1992
+++ finger-1.37/src/finger.c	Mon Feb  8 00:56:32 1993
@@ -192,6 +192,9 @@
   long addr;
   char *finger_server = NULL;
   int suppress_hostname = 0;
+#if defined(linux) /* gethostbyaddr evidently returns pointer to static area */
+  int do_hostfree = 0;
+#endif /* linux */
 
 
   username = savestring (arg);
@@ -237,6 +240,9 @@
 	{
 	  host = (struct hostent *) xmalloc (sizeof (struct hostent));
 	  host->h_name = hostname;
+#if defined (linux)
+	  do_hostfree = 1;
+#endif /* linux */
 	}
     }
   else
@@ -296,6 +302,11 @@
   if (finger_server)
     free (finger_server);
 
+#if !defined (linux)
   if (host)
     free (host);
+#else
+  if (host && do_hostfree)
+    free (host);
+#endif
 }
-------------END-------------
