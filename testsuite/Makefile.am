AUTOMAKE_OPTIONS = dejagnu
export DEJAGNU

# Programs that are expected across the board.
DEJATOOL =

if LINUX
# These should be in defined in 'across the board' scope, but are
# temporarily disabled on other than linux systems, see commit
# 3d807ae853b8b4264da156065b34f1447658a8ba
DEJATOOL += \
	kill \
	pmap \
	slabtop \
	sysctl

DEJATOOL += \
	free \
	pgrep \
	pkill \
	ps \
	pwdx \
	uptime \
	vmstat \
	w
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
