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
	config/unix.exp \
	global-conf.exp \
	site.exp \
	*.test
