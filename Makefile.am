AM_CFLAGS=-Wall
bin_PROGRAMS = icsdrone
icsdrone_SOURCES = argparser.c computer.c eval.c ics_wrap.c main.c net.c sched.c board.c fics.c marker.c polyglot.c utils.c proxy.c icsdrone.h
dist_doc_DATA=README README_old THANKS_old ChangeLog_old 

man6_MANS = icsdrone.man

icsdrone.man: @PACKAGE@.pod
	pod2man -c "" -r "" -s 6 @PACKAGE@.pod > icsdrone.man

README: icsdrone.man
	groff -t -e -mandoc -Tascii icsdrone.man| col -bx > README

deb:
	dpkg-buildpackage

rpm:
	make dist
	rpmbuild -ta @PACKAGE@-@VERSION@.tar.gz

EXTRA_DIST=icsdrone.man icsdroneng.pod icsdroneng.spec.in icsdroneng.spec debian/changelog debian/control debian/docs debian/README debian/compat debian/copyright debian/files debian/icsdroneng.substvars debian/rules