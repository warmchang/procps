
translate-templates: $(DIST_MAN_POTS)
EXTRA_DIST = $(DIST_MAN_POTS) $(DIST_MAN_POS) $(DIST_translated_MANS)

get-trans:
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man/  .
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man-ps/  ps
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man-top/  top

translate-mans: dist-man-paths $(translated_MANS)

base_langs = $(patsubst %.po,%,$(wildcard *.po))
top_langs = $(patsubst %.po,%,$(notdir $(wildcard top/*.po)))
ps_langs = $(patsubst %.po,%,$(notdir $(wildcard ps/*.po)))
all_langs = $(base_langs) $(top_langs) $(ps_langs)
DIST_MAN_POS = $(wildcard *.po) $(wildcard top/*.po) $(wildcard ps/*.po)
DIST_translated_MANS = 	$(shell find . -name '*.[1-9]')
MAN_SECTIONS = 1 5 8
MAN_PAGES = free.1 kill.1 pgrep.1 pidof.1 pkill.1 pmap.1 pwdx.1 skill.1 slabtop.1 \
	    tload.1 uptime.1 w.1 watch.1 \
	    sysctl.conf.5 \
	    sysctl.8 vmstat.8
man1_pages := $(foreach man,$(filter %.1, $(MAN_PAGES)), man1/$(man))
man5_pages := $(foreach man,$(filter %.5, $(MAN_PAGES)), man5/$(man))
man8_pages := $(foreach man,$(filter %.8, $(MAN_PAGES)), man8/$(man))

man_pages = $(man1_pages) $(man8_pages)

man_paths = $(foreach lang, $(base_langs), $(foreach section,$(MAN_SECTIONS),$(lang)/man$(section))) \
	    $(foreach lang, $(top_langs), $(lang)/man1) \
	    $(foreach lang, $(ps_langs), $(lang)/man1)
translated_MANS :=$(foreach lang,$(base_langs), $(foreach manpage,$(man_pages),$(lang)/$(manpage)))
translated_MANS +=$(foreach lang,$(top_langs), $(lang)/man1/top.1)
translated_MANS +=$(foreach lang,$(ps_langs), $(lang)/man1/ps.1)

PO4A_UPDATEPO = po4a-updatepo
PO4A_TRANSLATE = po4a-translate
translate_manpage = $(PO4A_TRANSLATE) -f man -m $< -p $*.po -l $@

CLEANFILES = $(DIST_MAN_POTS) $(translated_MANS)
maintainer-clean-local:
	-rm -rf $(all_langs)

MAN_PS_POT = template-man-ps.pot
MAN_PS_POT_FILES = $(top_srcdir)/ps/ps.1
MAN_TOP_POT = template-man-top.pot
MAN_TOP_POT_FILES = $(top_srcdir)/top/top.1
MAN_POT = template-man.pot
MAN_POT_FILES = $(top_srcdir)/free.1 $(top_srcdir)/kill.1 $(top_srcdir)/pgrep.1 \
		  $(top_srcdir)/pidof.1 $(top_srcdir)/pkill.1 $(top_srcdir)/pmap.1 \
		  $(top_srcdir)/pwdx.1 $(top_srcdir)/skill.1 $(top_srcdir)/slabtop.1 \
		  $(top_srcdir)/snice.1 $(top_srcdir)/sysctl.8 $(top_srcdir)/uptime.1 \
		  $(top_srcdir)/sysctl.conf.5 $(top_srcdir)/tload.1 \
		  $(top_srcdir)/vmstat.8 $(top_srcdir)/w.1 $(top_srcdir)/watch.1
DIST_MAN_POTS = $(MAN_PS_POT) $(MAN_TOP_POT) $(MAN_POT)

$(MAN_PS_POT): $(MAN_PS_POT_FILES)
	$(PO4A_UPDATEPO) -f man -m $< -p $@

$(MAN_TOP_POT): $(MAN_TOP_POT_FILES)
	$(PO4A_UPDATEPO) -f man -m $< -p $@

$(MAN_POT): $(MAN_POT_FILES)
	$(PO4A_UPDATEPO) -f man -m $< -p $@

dist_man_MANS = $(foreach lang, $(all_langs), $(foreach section, $(MAN_SECTIONS), $(wildcard $(lang)/man$(section)/*.$(section))))
dist-hook: translate-mans
	echo $(translated_MANS)


dist-man-paths:
	echo $(top_langs)
	@for p in $(man_paths) ; do \
	    echo " $(MKDIR_P) '$$p'"; \
	done

# My Makefile-fu fails me here, I cannot see how to have double-wildcards
# or have rules and recipies split
#
%/man1/top.1: ../top/top.1 top/%.po
	$(PO4A_TRANSLATE) -f man -m $< -p top/$*.po -l $@

%/man1/ps.1: ../ps/ps.1 ps/%.po
	$(PO4A_TRANSLATE) -f man -m $< -p ps/$*.po -l $@

%/man1/free.1: ../free.1 %.po
	$(translate_manpage)

%/man1/kill.1: ../kill.1 %.po
	$(translate_manpage)

%/man1/pgrep.1: ../pgrep.1 %.po
	$(translate_manpage)

%/man1/pidof.1: ../pidof.1 %.po
	$(translate_manpage)

%/man1/pkill.1: ../pkill.1 %.po
	$(translate_manpage)

%/man1/pmap.1: ../pmap.1 %.po
	$(translate_manpage)

%/man1/pwdx.1: ../pwdx.1 %.po
	$(translate_manpage)

%/man1/skill.1: ../skill.1 %.po
	$(translate_manpage)

%/man1/slabtop.1: ../slabtop.1 %.po
	$(translate_manpage)

%/man1/tload.1: ../tload.1 %.po
	$(translate_manpage)

%/man1/uptime.1: ../uptime.1 %.po
	$(translate_manpage)

%/man1/w.1: ../w.1 %.po
	$(translate_manpage)

%/man1/watch.1: ../watch.1 %.po
	$(translate_manpage)

%/man5/sysctl.conf.5: ../sysctl.conf.5 %.po
	$(translate_manpage)

%/man8/sysctl.8: ../sysctl.8 %.po
	$(translate_manpage)

%/man8/vmstat.8: ../vmstat.8 %.po
	$(translate_manpage)

.PHONY = translate-mans dist-man-paths
