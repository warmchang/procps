AUTOMAKE_OPTIONS = dejagnu
export DEJAGNU

if LINUX
# Programs that are expected across the board
DEJATOOL = \
		   free \
		   pgrep \
		   pkill \
		   ps \
		   pwdx \
		   uptime \
		   vmstat \
		   w \
		   sysctl slabtop pmap kill

endif
		
	
EXTRA_DIST = \
			 free.test/free.exp \
			 kill.test/kill.exp \
			 w.test/w.exp \
			 pgrep.test/pgrep.exp \
			 pkill.test/pkill.exp \
			 site.exp \
			 config/unix.exp \
			 uptime.test/uptime.exp \
			 pmap.test/pmap.exp \
			 ps.test/ps_output.exp \
			 ps.test/ps_personality.exp \
			 pwdx.test/pwdx.exp \
			 global-conf.exp \
			 sysctl.test/sysctl_read.exp \
			 slabtop.test/slabtop.exp \
			 vmstat.test/vmstat.exp
