#########################################################################
#				ENHANCEMENTS	 			#
#########################################################################

# New method e.g. 'make gui release'.  A series of enhancement
# targets may be specified which set make variables which enhance
# the build in various ways.
#
# img is now the default (produces both the .iso and .img)
#
gui installer img nopkgs:

#########################################################################
#				 SETUP		 			#
#########################################################################

DPORTS_PATH?=	/usr/dports
ISODIR?=	/usr/obj/release
ISOROOT?=	${ISODIR}/root
OBJSYS=		${.OBJDIR}/../sys
GITHOST?=	git.dragonflybsd.org
GITHOST_DPORTS?=mirror-master.dragonflybsd.org

MAKE_JOBS?=	$$(sysctl -n hw.ncpu)

# XXX makeshift fix to build the right kernel for the (target) architecture
# We should configure this in the platform files somehow
.if ${MACHINE_ARCH} == "x86_64"
KERNCONF ?= X86_64_GENERIC
.endif

CHROOT_CMD?=		/usr/sbin/chroot ${ISOROOT} sh -c

.if !make(nopkgs)
# User may specify extra packages in addition to the defaults
#
DPORTS_EXTRA_PACKAGES?=

# dports packages to be built and installed on the release ISO
#
# MISSING: csup
#
DPORTS_PACKAGES?=	devel/git \
			dns/bind-tools \
			net/isc-dhcp43-server \
			sysutils/cdrtools \
			${DPORTS_EXTRA_PACKAGES}

# dports options to use when building packages
#
DPORTS_OPTIONS+=		-DBATCH
.if make(gui)
DPORTS_OPTIONS.devel_git+=	-DWITH_GUI
.else
DPORTS_OPTIONS.devel_git+=	-DWITHOUT_CONTRIB -DWITHOUT_PERL
.endif
.endif

# Specify which root skeletons are required, and let the user include
# their own.  They are copied into ISODIR during the `customizeiso'
# target; each overwrites the last.
#
REQ_ROOTSKELS=	${.CURDIR}/root
ROOTSKELS?=	${REQ_ROOTSKELS}

.if make(gui)
# LIST OF PACKAGES NOT INCLUDED DUE TO BUILD ISSUES:
#	chat/pidgin		textproc/enchant dependency is broken
#	x11/rxvt-unicode	broken configure
#	net/nmap		tries to access openssl/md2.h which does not
#				exist.
#	sysutils/idesk		dying on link __sync_fetch_and_add_4
#
ISOFILE?=		${ISODIR}/dfly-gui.iso
IMGFILE?=		${ISODIR}/dfly-gui.img

# NOTE: order important, do not sort package list
#
# Apps we want in the gui build but which have problems building:
#	chat/xchat	link error on x86-64, something about ___progname
#
#
.if !make(nopkgs)
DPORTS_PACKAGES+=	x11/xorg \
			x11-wm/fluxbox \
			x11-wm/fvwm \
			www/firefox \
			graphics/xpdf \
			shells/zsh \
			editors/emacs \
			editors/vim \
			irc/irssi \
			lang/perl5 \
			editors/nano \
			shells/bash \
			devel/ctags \
			archivers/zip \
			security/sudo \
			www/links \
			ftp/wget \
			x11-fonts/terminus-font \
			net/rsync \
			x11-clocks/asclock \
			sysutils/screen
.endif

ROOTSKELS+=		${.CURDIR}/gui
.endif

ISOFILE ?= ${ISODIR}/dfly.iso
IMGFILE ?= ${ISODIR}/dfly.img

IMGMNT ?= ${ISODIR}/mnt

# USB umass now probes starting at da8, so the usb stick is
# probably sitting on da8.
#
IMGUSBDEV ?= da8

# note: we use the '${NRLOBJDIR}/nrelease' construct, that is we add
# the additional '/nrelease' manually, as a safety measure.
#
NRLOBJDIR?= /usr/obj

#########################################################################
#				BASE ISO TARGETS 			#
#########################################################################

release:	check clean buildworld1 buildkernel1 \
		buildiso srcs customizeiso mkiso mkimg

quickrel:	check clean buildworld2 buildkernel2 \
		buildiso srcs customizeiso mkiso mkimg

realquickrel:	check clean \
		buildiso srcs customizeiso mkiso mkimg

restartpkgs:	check customizeiso mkiso mkimg

quick:		quickrel

realquick:	realquickrel

#########################################################################
#			   CORE SUPPORT TARGETS 			#
#########################################################################

check:
.if !exists(/usr/local/bin/mkisofs) && !exists(/usr/pkg/bin/mkisofs)
	@echo
	@echo "Your machine does not have cdrtools installed.  You can install it with:"
	@echo "    pkg install cdrtools"
.endif
	@if [ ! -d ${DPORTS_PATH} ]; then \
		echo "${DPORTS_PATH} does not exist."; \
		echo ""; \
		echo "Please set DPORTS_PATH to the dports tree that shall be used for"; \
		echo "package building. The default is /usr/dports. See the Makefile in"; \
		echo "/usr if you are unfamiliar with dports."; \
		/usr/bin/false; \
	fi
.if !exists(/usr/local/bin/mkisofs) && !exists(/usr/pkg/bin/mkisofs)
	@/usr/bin/false
.endif
	@echo "Using ${DPORTS_PATH} as the dports tree."

buildworld1 buildworld2:
	cd ${.CURDIR}/..; \
	${WORLD_CCVER:C/^..*$/WORLD_CCVER=/}${WORLD_CCVER} \
		make -j ${MAKE_JOBS} -DWANT_INSTALLER ${.TARGET:C/build(.*)2/quick\1/:C/1//}

buildkernel1 buildkernel2:
	cd ${.CURDIR}/..; \
	for kernconf in ${KERNCONF}; do \
		${WORLD_CCVER:C/^..*$/WORLD_CCVER=/}${WORLD_CCVER} \
			make -j ${MAKE_JOBS} ${.TARGET:C/build(.*)2/quick\1/:C/1//} \
			KERNCONF=$${kernconf}; \
	done

# note that we do not want to mess with any /usr/obj directories not related
# to buildworld, buildkernel, or nrelease, so we must supply the proper
# MAKEOBJDIRPREFIX for targets that are not run through the buildworld and 
# buildkernel mechanism.
#
# Unconditionally clean out ${ISOROOT} so a previous img build
# does not blow up a future quick iso build
#
# We install with INSTALLSTRIPPEDMODULES to make things fit into the ISO.
# We leave the kernel's debug variables intact.
#
buildiso:
	-rm -rf ${ISOROOT}
	-chflags -R noschg ${ISOROOT}
	rm -rf ${ISOROOT}
	mkdir -p ${ISOROOT}
	if [ ! -d ${NRLOBJDIR}/nrelease ]; then mkdir -p ${NRLOBJDIR}/nrelease; fi
	( cd ${.CURDIR}/..; make -DWANT_INSTALLER DESTDIR=${ISOROOT} installworld )
	( cd ${.CURDIR}/../etc; MAKEOBJDIRPREFIX=${NRLOBJDIR}/nrelease \
		make -m ${.CURDIR}/../share/mk DESTDIR=${ISOROOT} distribution )
	cpdup ${ISOROOT}/etc ${ISOROOT}/etc.hdd
	cd ${.CURDIR}/..; \
	for kernconf in ${KERNCONF}; do \
		make DESTDIR=${ISOROOT} reinstallkernel KERNCONF=$${kernconf}; \
	done
	rm -rf ${ISOROOT}/boot/kernel.old
	ln -sf kernel ${ISOROOT}/boot/kernel/kernel.BOOTP
	# This next mkinitrd has to be the INSTALLED version because else we
	# can't rely on the -c option being present, but it needs to be run
	# OUTSIDE of the chroot because it might want to load vn(4) and can't
	# do so from inside of it.
	#
	${ISOROOT}/sbin/mkinitrd -c ${ISOROOT}/usr/share/initrd -b ${ISOROOT}/boot
	mtree -deU -f ${.CURDIR}/../etc/mtree/BSD.local.dist -p ${ISOROOT}/usr/local/
	mtree -deU -f ${.CURDIR}/../etc/mtree/BSD.var.dist -p ${ISOROOT}/var
	dev_mkdb -f ${ISOROOT}/var/run/dev.db ${ISOROOT}/dev

# The GUI build includes the ports tree (~1G+) while the nominal release
# build does not.
#
# The GUI build includes full sources while the nominal release build
# only includes kernel sources (~27MB).
#
srcs:
.if !defined(WITHOUT_SRCS)
.if make(gui)
	rm -f ${ISOROOT}/usr/src-sys.tgz
	rm -f ${ISOROOT}/usr/src-sys.tar.bz2
.if !empty(DPORTS_PACKAGES)
	cd ${ISOROOT}/usr && make dports-create GITHOST_DPORTS=${GITHOST_DPORTS}
	# NOTE: Adding the git gc --aggressive helps by significantly
	#	reducing the disk space required.
	#
	cd ${ISOROOT}/usr/dports && git gc --aggressive
.endif
	cd ${ISOROOT}/usr && make src-create-repo GITHOST=${GITHOST}
	# XXX: git gc --aggressive
.else
	rm -f ${ISOROOT}/usr/src-sys.tgz
	rm -f ${ISOROOT}/usr/src-sys.tar.bz2
	cd ${.CURDIR}/.. && tar --exclude .git -s '/^\./src/' \
		-cf - ./Makefile ./Makefile.inc1 ./sys | \
		bzip2 -9 > ${ISOROOT}/usr/src-sys.tar.bz2
.endif
.endif

# Customize the ISO by copying rootskels in reverse priority order,
# building packages, and doing other post-install tasks.
#
customizeiso:
	# Copy the rootskels.  Allow sources to be owned by someone other
	# than root (as is common when checked out via git).
	#
.for ROOTSKEL in ${ROOTSKELS}
	cpdup -X cpignore -o ${ROOTSKEL} ${ISOROOT}
	@test -O ${.CURDIR} || echo "chowning copied files to root:wheel"
	@test -O ${.CURDIR} || ((cd ${ROOTSKEL} && find .) | fgrep -v cpignore | (cd ${ISOROOT} && xargs chown root:wheel))
.endfor
	pwd_mkdb -p -d ${ISOROOT}/etc ${ISOROOT}/etc/master.passwd
.for UPGRADE_ITEM in Makefile			\
		     etc.${MACHINE_ARCH} 	\
		     rc.d/Makefile		\
		     periodic/Makefile		\
		     periodic/daily/Makefile	\
		     periodic/security/Makefile	\
		     periodic/weekly/Makefile	\
		     periodic/monthly/Makefile
	cp -R ${.CURDIR}/../etc/${UPGRADE_ITEM} ${ISOROOT}/etc/${UPGRADE_ITEM}
.endfor
	#
	# Setup some things & mount dports tree.  Use defensive umounts and
	# rm -rf's to allow restarts.  Allow /usr/dports to be read-only.
	#
	-@umount ${ISOROOT}/dev
	mount_null /dev ${ISOROOT}/dev
.if !empty(DPORTS_PACKAGES)
	cp /etc/resolv.conf ${ISOROOT}/etc
	${CHROOT_CMD} "ldconfig -elf /lib /usr/lib /usr/lib/gcc*"
	-@umount ${ISOROOT}/usr/distfiles
	-@umount ${ISOROOT}/usr/dports
	rm -rf ${ISOROOT}/usr/obj/dports
	#
	# Mount /usr/dports, make sure /usr/distfiles is writable.
	# Make /usr/dports read-only for safety, else a failed umount and
	# our rm -rf will do bad things.
	#
	mkdir -p ${ISOROOT}/usr/dports
	mkdir -p ${ISOROOT}/usr/distfiles
	mount_null -o ro ${DPORTS_PATH} ${ISOROOT}/usr/dports
	cp /etc/shells ${ISOROOT}/usr/distfiles/.test > /dev/null 2>&1 \
	    || mount_null ${ISODIR}/distfiles ${ISOROOT}/usr/distfiles
	#
	# Build and install packages, skip packages already installed
	#
.for PKG in ${DPORTS_PACKAGES}
	${CHROOT_CMD} "cd /usr/dports/${PKG} && make ${DPORTS_OPTIONS} ${DPORTS_OPTIONS.${PKG:S/\//_/g}} install"
.endfor
.for PKG in ${DPORTS_PACKAGES}
	${CHROOT_CMD} "cd /usr/dports/${PKG} && make ${DPORTS_OPTIONS} clean"
.endfor
	#
	# Remove packages which nothing depends on, add a whatis database
	# for dports manual pages, create an initial locate database and
	# clean up
	#
	${CHROOT_CMD} "yes | pkg autoremove"
	${CHROOT_CMD} "makewhatis /usr/local/man"
	-umount ${ISOROOT}/usr/distfiles
	umount ${ISOROOT}/usr/dports
	rm -rf ${ISOROOT}/usr/dports
	rm -rf ${ISOROOT}/usr/obj/dports
	rm -rf ${ISOROOT}/usr/distfiles
	rm -f ${ISOROOT}/etc/resolv.conf
.endif
	#
	# Finally, update the locate(8) database, allow ISODIR
	# to be on tmpfs (fails to create locate database then)
	#
	-${CHROOT_CMD} /etc/periodic/weekly/310.locate
	umount ${ISOROOT}/dev
	#
	# Recopy files that dports may have updated in /etc into /etc.hdd
	#
	cpdup ${ISOROOT}/etc/shells ${ISOROOT}/etc.hdd/shells
	cpdup ${ISOROOT}/etc/group ${ISOROOT}/etc.hdd/group

mkiso:
	if [ ! -d ${IMGMNT} ]; then mkdir -p ${IMGMNT}; fi
	-if [ -f ${ISODIR}/vn.which ]; then umount ${IMGMNT}; 		\
	    vnconfig -u `cat ${ISODIR}/vn.which`; fi
	newfs_msdos -C 400k -F 12 -L EFI -m 0xf8 ${ISOROOT}/boot/efiboot.img
	vnconfig -l | grep "not in use" | head -n 1 | 			\
	    cut -f 1 -d: > ${ISODIR}/vn.which
	vnconfig -e -s labels `cat ${ISODIR}/vn.which` ${ISOROOT}/boot/efiboot.img
	mount_msdos /dev/`cat ${ISODIR}/vn.which` ${IMGMNT}
	mkdir -p ${IMGMNT}/EFI/BOOT
	cp ${ISOROOT}/boot/loader.efi ${IMGMNT}/EFI/BOOT/BOOTX64.EFI
	umount ${IMGMNT}
	( cd ${ISOROOT}; mkisofs -R -J -o ${ISOFILE} \
		-b boot/cdboot -no-emul-boot \
		-eltorito-alt-boot -eltorito-platform efi -eltorito-boot boot/efiboot.img -no-emul-boot \
		-V "DragonFly `${.CURDIR}/../tools/gitrev.sh | cut -c -22`" . )
	rm -f ${ISOROOT}/boot/efiboot.img

mkimg:
	if [ ! -d ${IMGMNT} ]; then mkdir -p ${IMGMNT}; fi
	-if [ -f ${ISODIR}/vn.which ]; then umount ${IMGMNT}; 		\
	    vnconfig -u `cat ${ISODIR}/vn.which`; fi
.ifdef IMGSIZE
	@echo "STEP: use an image size of ${IMGSIZE} 512-byte sectors"
	rm -f ${IMGFILE}
	sz=`bc -e "((${IMGSIZE}) * 512)" -equit`; truncate -s $${sz} ${IMGFILE}
.else
	@echo "STEP: Determine required image size in 1GB steps"
	@echo "      Leave ~600MB of unused space"
	sz=`du -ck ${ISOROOT} | tail -n 1 | cut -f 1`;			\
	    sz=`bc -e "(($${sz}) * 1.15 + 999999 + 600000) / 1000000" -equit | \
	    cut -f1 -d.`;						\
	    sz=`bc -e "(($${sz}) * 953)" -equit | cut -f1 -d.`;		\
	    rm -f ${IMGFILE};						\
	    truncate -s $${sz}M ${IMGFILE};
.endif
	@echo "STEP: determine free vn device"
	vnconfig -l | grep "not in use" | head -n 1 | 			\
	    cut -f 1 -d: > ${ISODIR}/vn.which
	vnconfig -e -s labels `cat ${ISODIR}/vn.which` ${IMGFILE}
	@echo "STEP: set up legacy MBR"
	fdisk -IB `cat ${ISODIR}/vn.which`
	fdisk -s `cat ${ISODIR}/vn.which` >${ISODIR}/fdisk.dat
	awk '(NR==1){printf("g c%s h%s s%s\n", $$2, $$4, $$6);}' \
	    ${ISODIR}/fdisk.dat >${ISODIR}/fdisk.conf
	echo "p 1 239 63 257985" >>${ISODIR}/fdisk.conf
	awk '($$1=="1:"){printf("p 2 165 258048 %lu\n", $$3 - 258528);}' \
	    ${ISODIR}/fdisk.dat >>${ISODIR}/fdisk.conf
	echo "a 2" >>${ISODIR}/fdisk.conf
	fdisk -iv -f ${ISODIR}/fdisk.conf `cat ${ISODIR}/vn.which`
	rm ${ISODIR}/fdisk.conf ${ISODIR}/fdisk.dat
	newfs_msdos -F 32 -c 2 -L EFI -m 0xf8 `cat ${ISODIR}/vn.which`s1
	mount_msdos /dev/`cat ${ISODIR}/vn.which`s1 ${IMGMNT}
	mkdir -p ${IMGMNT}/EFI/BOOT
	cp ${ISOROOT}/boot/boot1.efi ${IMGMNT}/EFI/BOOT/BOOTX64.EFI
	umount ${IMGMNT}
	@echo "STEP: write standard disklabel"
	disklabel -w -r `cat ${ISODIR}/vn.which`s2 auto
	@echo "STEP: read disklabel back"
	disklabel -r `cat ${ISODIR}/vn.which`s2 > ${IMGFILE}.label
	@echo "STEP: determine number of sectors of whole disk"
	secs=`tail -n 1 ${IMGFILE}.label | cut -f 3 -w`;		\
	    echo "  a:  $${secs} 0 4.2BSD" >> ${IMGFILE}.label;
	@echo "STEP: write modified disklabel back"
	disklabel -R -r `cat ${ISODIR}/vn.which`s2 ${IMGFILE}.label
	rm ${IMGFILE}.label
	disklabel -B `cat ${ISODIR}/vn.which`s2
	newfs /dev/`cat ${ISODIR}/vn.which`s2a
	mount /dev/`cat ${ISODIR}/vn.which`s2a ${IMGMNT}
	cpdup ${ISOROOT} ${IMGMNT}
	@echo "STEP: fixup ${IMGMNT}/etc/fstab"
	echo "/dev/${IMGUSBDEV}s2a / ufs rw,noatime 0 1" > ${IMGMNT}/etc/fstab
	echo "dummy /tmp tmpfs rw 0 0" >> ${IMGMNT}/etc/fstab
	echo "dummy /var/tmp tmpfs rw 0 0" >> ${IMGMNT}/etc/fstab
	echo "dummy /var/run tmpfs rw,-C 0 0" >> ${IMGMNT}/etc/fstab
	echo "dummy /usr/obj tmpfs rw 0 0" >> ${IMGMNT}/etc/fstab
	echo "proc /proc procfs rw 0 0" >> ${IMGMNT}/etc/fstab
	@echo "STEP: fixup ${IMGMNT}/boot/loader.conf"
	-fgrep -v kernel_options ${IMGMNT}/boot/loader.conf > ${IMGMNT}/boot/loader.conf.new
	echo 'vfs.root.mountfrom="ufs:${IMGUSBDEV}s2a"' >> ${IMGMNT}/boot/loader.conf.new
	mv ${IMGMNT}/boot/loader.conf.new ${IMGMNT}/boot/loader.conf
	@echo "STEP: cleanup"
	df ${IMGMNT}
	sync
	sleep 1
	umount ${IMGMNT}
	vnconfig -u `cat ${ISODIR}/vn.which`
	rm -f ${ISODIR}/vn.which
	rmdir ${IMGMNT}
	@echo "STEP: done"

clean:
	sync
	sleep 1
	-umount ${ISOROOT}/usr/distfiles > /dev/null 2>&1
	-umount ${ISOROOT}/usr/dports > /dev/null 2>&1
	-umount ${ISOROOT}/dev > /dev/null 2>&1
	-if [ -f ${ISODIR}/vn.which ]; then umount ${IMGMNT}; 		\
	    vnconfig -u `cat ${ISODIR}/vn.which`; fi
	if [ -d ${ISOROOT} ]; then chflags -R noschg ${ISOROOT}; fi
	rm -rf ${ISOROOT}
	rm -rf ${NRLOBJDIR}/nrelease
	rm -f ${ISODIR}/.didbootstrap ${ISODIR}/vn.which

realclean:	clean
	rm -rf ${OBJSYS}/${KERNCONF}
	rm -rf ${ISODIR}/packages
	rm -rf ${ISODIR}/distfiles

help all:
	@echo "make [gui] release   - complete build from scratch"
	@echo "make [gui] quick     - attempt to do an incremental rebuild"
	@echo "make [gui] realquick - attempt to restart after world & kernel"
	@echo "make [gui] restartpkgs - attempt to restart at the pkg building stage"
	@echo ""
	@echo "DPORTS_EXTRA_PACKAGES may be used to add additional pkgs"
	@echo "GITHOST may be used to override git.dragonflybsd.org"
	@echo "GITHOST_DPORTS may be used to override mirror-master.dragonflybsd.org"
	@echo "IMGSIZE may be used to override the .img (in 512-byte sectors)"
	@echo "MAKE_JOBS may be used to override the default value (sysctl hw.ncpu)"

.PHONY: release quickrel realquickrel
.PHONY: installer
.PHONY: quick realquick
.PHONY: check buildworld1 buildworld2
.PHONY: buildkernel1 buildkernel2 buildiso customizeiso mkiso mkimg
.PHONY: clean realclean help all srcs

.include <bsd.prog.mk>
