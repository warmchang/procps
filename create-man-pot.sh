#!/bin/sh
#
# Run this command in the top level directory to create the translation template:

if [ -d man-po ] ; then
  echo "man-po: directory exists, will be reused"
  else
    mkdir -p man-po
fi

po4a-updatepo -f man -m free.1 \
                     -m kill.1 \
                     -m pgrep.1 \
                     -m pidof.1 \
                     -m pkill.1 \
                     -m pmap.1 \
                     -m pwdx.1 \
                     -m skill.1 \
                     -m slabtop.1 \
                     -m snice.1 \
                     -m sysctl.8 \
                     -m sysctl.conf.5 \
                     -m tload.1 \
                     -m uptime.1 \
                     -m vmstat.8 \
                     -m w.1 \
                     -m watch.1 \
                     -p man-po/template-man.pot
                     
po4a-updatepo -f man -m ps/ps.1 \
                     -p man-po/template-man-ps.pot

po4a-updatepo -f man -m top/top.1 \
                     -p man-po/template-man-top.pot

# Rename the file according to the version of your (pre-release) tarball.                     
# Send the new file together with a link to the tarball to the TP coordinators:
# <coordinator@translationproject.org>