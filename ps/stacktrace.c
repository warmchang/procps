/*
 * Gnu debugger stack trace code provided by Peter Mattis
 * <petm@CSUA.Berkeley.EDU> on Thu, 2 Nov 1995
 *
 * Modified for easy use by Albert Cahalan.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"

#define INTERACTIVE 0
#define STACK_TRACE 1

char *stored_prog_name = "You forgot to set \"program\"";
static int stack_trace_done;

/***********/
static void debug_stop(char **args){
  execvp (args[0], args);
  perror ("exec failed");
  _exit (0);
}

/***********/
static void stack_trace_sigchld(int signum){
  (void)signum;
  stack_trace_done = 1;
}

/************/
static void stack_trace(char **args){
  pid_t pid;
  int in_fd[2];
  int out_fd[2];
  fd_set fdset;
  fd_set readset;
  struct timeval tv;
  int sel, index, state;
  char buffer[256];
  char c;

  stack_trace_done = 0;
  signal(SIGCHLD, stack_trace_sigchld);

  if((pipe (in_fd) == -1) || (pipe (out_fd) == -1)){
    perror ("could open pipe");
    _exit (0);
  }

  pid = fork ();
  if (pid == 0){
    close (0); dup (in_fd[0]);   /* set the stdin to the in pipe */
    close (1); dup (out_fd[1]);  /* set the stdout to the out pipe */
    close (2); dup (out_fd[1]);  /* set the stderr to the out pipe */
    execvp (args[0], args);      /* exec gdb */
    perror ("exec failed");
    _exit (0);
  } else {
    if(pid == (pid_t) -1){
      perror ("could not fork");
      _exit (0);
    }
  }

  FD_ZERO (&fdset);
  FD_SET (out_fd[0], &fdset);

  write (in_fd[1], "backtrace\n", 10);
  write (in_fd[1], "p x = 0\n", 8);
  write (in_fd[1], "quit\n", 5);

  index = 0;
  state = 0;

  for(;;){
    readset = fdset;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    sel = select (FD_SETSIZE, &readset, NULL, NULL, &tv);
    if (sel == -1) break;

    if((sel > 0) && (FD_ISSET (out_fd[0], &readset))){
      if(read (out_fd[0], &c, 1)){
        switch(state){
        case 0:
          if(c == '#'){
            state = 1;
            index = 0;
            buffer[index++] = c;
          }
          break;
        case 1:
          buffer[index++] = c;
          if((c == '\n') || (c == '\r')){
            buffer[index] = 0;
            fprintf (stderr, "%s", buffer);
            state = 0;
            index = 0;
          }
          break;
        default:
          break;
        }
      }
    }
    else if(stack_trace_done) break;
  }

  close (in_fd[0]);
  close (in_fd[1]);
  close (out_fd[0]);
  close (out_fd[1]);
  _exit (0);
}

/************/
void debug(int method, char *prog_name){
  pid_t pid;
  char buf[16];
  char *args[4] = { "gdb", NULL, NULL, NULL };
  int x;

  snprintf (buf, sizeof(buf), "%d", getpid ());

  args[1] = prog_name;
  args[2] = buf;

  pid = fork ();
  if(pid == 0){
    switch (method){
    case INTERACTIVE:
      fprintf (stderr, "debug_stop\n");
      debug_stop(args);
      break;
    case STACK_TRACE:
      fprintf (stderr, "stack_trace\n");
      stack_trace(args);
      break;
    }
    _exit(0);
  } else if(pid == (pid_t) -1){
    perror ("could not fork");
    return;
  }

  x = 1;
  while(x);  /* wait for debugger? */
}

/************/
static void stack_trace_sigsegv(int signum){
  (void)signum;
  debug(STACK_TRACE, stored_prog_name);
}

/************/
void init_stack_trace(char *prog_name){
  stored_prog_name = prog_name;
  signal(SIGSEGV, stack_trace_sigsegv);
}
