#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/Makefile,v 1.22.2.4 2002/03/12 17:13:32 phantom Exp $

SUBDIR= colldef dict doc examples i18n initrd locale-links \
	man me misc mk ctypedef monetdef msgdef \
	numericdef skel syscons tabset termcap terminfo timedef zoneinfo

SUBDIR_ORDERED=		# maximum parallism

.if defined(WANT_INSTALLER)
SUBDIR+= installer
.endif

.include <bsd.subdir.mk>
