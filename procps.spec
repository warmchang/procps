Summary: Utilities for monitoring your system and processes on your system.
Name: procps
%define major_version 3
%define minor_version 0
%define revision 3
%define version %{major_version}.%{minor_version}.%{revision}
Version: %{version}
Release: 1
Copyright: GPL
Group: Applications/System
Source: ftp://people.redhat.com/johnsonm/procps/procps-%{version}.tar.gz
BuildRoot: /var/tmp/procps-root

%package X11
Group: Applications/System
Summary: X based process monitoring utilities.

%description
The procps package contains a set of system utilities which provide
system information.  Procps includes ps, free, sysctl, skill, snice,
tload, top, uptime, vmstat, w, and watch.  The ps command displays
a snapshot of running processes.  The top command provides a
repetitive update of the statuses of running processes.  The free
command displays the amounts of free and used memory on your system.
Sysctl is a simple program for managing system configuration entries.
The skill command sends a terminate command (or another
specified signal) to a specified set of processes.  The snice
command is used to change the scheduling priority of specified
processes.  The tload command prints a graph of the current system
load average to a specified tty.  The uptime command displays the
current time, how long the system has been running, how many users
are logged on and system load averages for the past one, five and
fifteen minutes.  The w command displays a list of the users who
are currently logged on and what they're running.  The watch program
watches a running program.  The vmstat command displays virtual
memory statistics about processes, memory, paging, block I/O, traps
and CPU activity.

%description X11
The procps-X11 package contains the XConsole shell script, a backwards
compatibility wrapper for the xconsole program.

%prep
%setup

%build
PATH=/usr/X11R6/bin:$PATH

make CC="gcc $RPM_OPT_FLAGS" LDFLAGS=-s

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/bin $RPM_BUILD_ROOT/usr/bin $RPM_BUILD_ROOT/sbin
mkdir -p $RPM_BUILD_ROOT/usr/man/man1 $RPM_BUILD_ROOT/usr/man/man8
mkdir -p $RPM_BUILD_ROOT/lib $RPM_BUILD_ROOT/usr/X11R6/bin

make DESTDIR=$RPM_BUILD_ROOT install=install install

%clean
rm -rf $RPM_BUILD_ROOT

%post
# add libproc to the cache
/sbin/ldconfig
# remove obsolete files
rm -f /etc/psdevtab /etc/psdatabase

%files
%defattr(0644,root,root,755)
%attr(0644,root,root) %config(missingok) /etc/X11/applnk/Utilities/top.desktop
%doc NEWS BUGS TODO
%attr(755,root,root) /lib/libproc.so.3.0.?
%attr(555,root,root) /bin/ps
%attr(555,root,root) /sbin/sysctl
%attr(555,root,root) /usr/bin/*

%attr(0644,root,root) /usr/man/man1/*
%attr(0644,root,root) /usr/man/man8/*

%files X11
%attr(0755,root,root) /usr/X11R6/bin/XConsole
