SUBDIRS = matchbox data

# Extra clean files so that maintainer-clean removes *everything*

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-snapshot-`date +"%Y%m%d"`

MAINTAINERCLEANFILES = aclocal.m4 compile config.status config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing
