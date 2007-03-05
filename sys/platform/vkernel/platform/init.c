/*
 * Copyright (c) 2006 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/platform/vkernel/platform/init.c,v 1.30 2007/03/05 02:41:45 swildner Exp $
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/cons.h>
#include <sys/random.h>
#include <sys/vkernel.h>
#include <sys/tls.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/vmspace.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <vm/vm_page.h>

#include <machine/globaldata.h>
#include <machine/tls.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/bridge/if_bridgevar.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <assert.h>

vm_paddr_t phys_avail[16];
vm_paddr_t Maxmem;
vm_paddr_t Maxmem_bytes;
int MemImageFd = -1;
int RootImageFd = -1;
struct vknetif_info NetifInfo[VKNETIF_MAX];
int NetifNum;
vm_offset_t KvaStart;
vm_offset_t KvaEnd;
vm_offset_t KvaSize;
vm_offset_t virtual_start;
vm_offset_t virtual_end;
vm_offset_t kernel_vm_end;
vm_offset_t crashdumpmap;
vm_offset_t clean_sva;
vm_offset_t clean_eva;
struct msgbuf *msgbufp;
caddr_t ptvmmap;
vpte_t	*KernelPTD;
vpte_t	*KernelPTA;	/* Warning: Offset for direct VA translation */
u_int cpu_feature;	/* XXX */
u_int tsc_present;	/* XXX */

struct privatespace *CPU_prvspace;

static struct trapframe proc0_tf;
static void *proc0paddr;

static void init_sys_memory(char *imageFile);
static void init_kern_memory(void);
static void init_globaldata(void);
static void init_vkernel(void);
static void init_rootdevice(char *imageFile);
static void init_netif(char *netifFile[], int netifFileNum);
static void usage(const char *ctl);

/*
 * Kernel startup for virtual kernels - standard main() 
 */
int
main(int ac, char **av)
{
	char *memImageFile = NULL;
	char *rootImageFile = NULL;
	char *netifFile[VKNETIF_MAX];
	char *suffix;
	int netifFileNum = 0;
	int c;
	int i;
	int n;

	/*
	 * Process options
	 */
	kernel_mem_readonly = 1;

	while ((c = getopt(ac, av, "svm:r:e:I:U")) != -1) {
		switch(c) {
		case 'e':
			/*
			 * name=value:name=value:name=value...
			 */
			n = strlen(optarg);
			kern_envp = malloc(n + 2);
			for (i = 0; i < n; ++i) {
				if (optarg[i] == ':')
					kern_envp[i] = 0;
				else
					kern_envp[i] = optarg[i];
			}
			kern_envp[i++] = 0;
			kern_envp[i++] = 0;
			break;
		case 's':
			boothowto |= RB_SINGLE;
			break;
		case 'v':
			bootverbose = 1;
			break;
		case 'i':
			memImageFile = optarg;
			break;
		case 'I':
			if (netifFileNum < VKNETIF_MAX)
				netifFile[netifFileNum++] = optarg;
			break;
		case 'r':
			rootImageFile = optarg;
			break;
		case 'm':
			Maxmem_bytes = strtoull(optarg, &suffix, 0);
			if (suffix) {
				switch(*suffix) {
				case 'g':
				case 'G':
					Maxmem_bytes <<= 30;
					break;
				case 'm':
				case 'M':
					Maxmem_bytes <<= 20;
					break;
				case 'k':
				case 'K':
					Maxmem_bytes <<= 10;
					break;
				default:
					Maxmem_bytes = 0;
					usage("Bad maxmem option");
					/* NOT REACHED */
					break;
				}
			}
			break;
		case 'U':
			kernel_mem_readonly = 0;
			break;
		}
	}

	cpu_disable_intr();
	init_sys_memory(memImageFile);
	init_kern_memory();
	init_globaldata();
	init_vkernel();
	init_kqueue();
	init_rootdevice(rootImageFile);
	init_netif(netifFile, netifFileNum);
	init_exceptions();
	mi_startup();
	/* NOT REACHED */
	exit(1);
}

/*
 * Initialize system memory.  This is the virtual kernel's 'RAM'.
 */
static
void
init_sys_memory(char *imageFile)
{
	struct stat st;
	int i;
	int fd;

	/*
	 * Figure out the system memory image size.  If an image file was
	 * specified and -m was not specified, use the image file's size.
	 */

	if (imageFile && stat(imageFile, &st) == 0 && Maxmem_bytes == 0)
		Maxmem_bytes = (vm_paddr_t)st.st_size;
	if ((imageFile == NULL || stat(imageFile, &st) < 0) && 
	    Maxmem_bytes == 0) {
		err(1, "Cannot create new memory file %s unless "
		       "system memory size is specified with -m",
		       imageFile);
		/* NOT REACHED */
	}

	/*
	 * Maxmem must be known at this time
	 */
	if (Maxmem_bytes < 32 * 1024 * 1024 || (Maxmem_bytes & SEG_MASK)) {
		err(1, "Bad maxmem specification: 32MB minimum, "
		       "multiples of %dMB only",
		       SEG_SIZE / 1024 / 1024);
		/* NOT REACHED */
	}

	/*
	 * Generate an image file name if necessary, then open/create the
	 * file exclusively locked.  Do not allow multiple virtual kernels
	 * to use the same image file.
	 */
	if (imageFile == NULL) {
		for (i = 0; i < 1000000; ++i) {
			asprintf(&imageFile, "/var/vkernel/memimg.%06d", i);
			fd = open(imageFile, 
				  O_RDWR|O_CREAT|O_EXLOCK|O_NONBLOCK, 0644);
			if (fd < 0 && errno == EWOULDBLOCK) {
				free(imageFile);
				continue;
			}
			break;
		}
	} else {
		fd = open(imageFile, O_RDWR|O_CREAT|O_EXLOCK|O_NONBLOCK, 0644);
	}
	printf("Using memory file: %s\n", imageFile);
	if (fd < 0 || fstat(fd, &st) < 0) {
		err(1, "Unable to open/create %s", imageFile);
		/* NOT REACHED */
	}

	/*
	 * Truncate or extend the file as necessary.
	 */
	if (st.st_size > Maxmem_bytes) {
		ftruncate(fd, Maxmem_bytes);
	} else if (st.st_size < Maxmem_bytes) {
		char *zmem;
		off_t off = st.st_size & ~SEG_MASK;

		kprintf("%s: Reserving blocks for memory image\n", imageFile);
		zmem = malloc(SEG_SIZE);
		bzero(zmem, SEG_SIZE);
		lseek(fd, off, SEEK_SET);
		while (off < Maxmem_bytes) {
			if (write(fd, zmem, SEG_SIZE) != SEG_SIZE) {
				err(1, "Unable to reserve blocks for memory image");
				/* NOT REACHED */
			}
			off += SEG_SIZE;
		}
		if (fsync(fd) < 0)
			err(1, "Unable to reserve blocks for memory image");
		free(zmem);
	}
	MemImageFd = fd;
	Maxmem = Maxmem_bytes >> PAGE_SHIFT;
}

/*
 * Initialize kernel memory.  This reserves kernel virtual memory by using
 * MAP_VPAGETABLE
 */
static
void
init_kern_memory(void)
{
	void *base;
	char *zero;
	vpte_t pte;
	int i;

	/*
	 * Memory map our kernel virtual memory space.  Note that the
	 * kernel image itself is not made part of this memory for the
	 * moment.
	 *
	 * The memory map must be segment-aligned so we can properly
	 * offset KernelPTD.
	 */
	base = mmap((void *)0x40000000, KERNEL_KVA_SIZE, PROT_READ|PROT_WRITE,
		    MAP_FILE|MAP_SHARED|MAP_VPAGETABLE, MemImageFd, 0);
	if (base == MAP_FAILED) {
		err(1, "Unable to mmap() kernel virtual memory!");
		/* NOT REACHED */
	}
	madvise(base, KERNEL_KVA_SIZE, MADV_NOSYNC);
	KvaStart = (vm_offset_t)base;
	KvaSize = KERNEL_KVA_SIZE;
	KvaEnd = KvaStart + KvaSize;

	/*
	 * Create a top-level page table self-mapping itself. 
	 *
	 * Initialize the page directory at physical page index 0 to point
	 * to an array of page table pages starting at physical page index 1
	 */
	lseek(MemImageFd, 0L, 0);
	for (i = 0; i < KERNEL_KVA_SIZE / SEG_SIZE; ++i) {
		pte = ((i + 1) * PAGE_SIZE) | VPTE_V | VPTE_R | VPTE_W;
		write(MemImageFd, &pte, sizeof(pte));
	}

	/*
	 * Initialize the PTEs in the page table pages required to map the
	 * page table itself.  This includes mapping the page directory page
	 * at the base so we go one more loop then normal.
	 */
	lseek(MemImageFd, PAGE_SIZE, 0);
	for (i = 0; i <= KERNEL_KVA_SIZE / SEG_SIZE * sizeof(vpte_t); ++i) {
		pte = (i * PAGE_SIZE) | VPTE_V | VPTE_R | VPTE_W;
		write(MemImageFd, &pte, sizeof(pte));
	}

	/*
	 * Initialize remaining PTEs to 0.  We may be reusing a memory image
	 * file.  This is approximately a megabyte.
	 */
	i = (KERNEL_KVA_SIZE / PAGE_SIZE - i) * sizeof(pte);
	zero = malloc(PAGE_SIZE);
	bzero(zero, PAGE_SIZE);
	while (i) {
		write(MemImageFd, zero, (i > PAGE_SIZE) ? PAGE_SIZE : i);
		i = i - ((i > PAGE_SIZE) ? PAGE_SIZE : i);
	}
	free(zero);

	/*
	 * Enable the page table and calculate pointers to our self-map
	 * for easy kernel page table manipulation.
	 *
	 * KernelPTA must be offset so we can do direct VA translations
	 */
	mcontrol(base, KERNEL_KVA_SIZE, MADV_SETMAP,
		 0 | VPTE_R | VPTE_W | VPTE_V);
	KernelPTD = (vpte_t *)base;			  /* pg directory */
	KernelPTA = (vpte_t *)((char *)base + PAGE_SIZE); /* pg table pages */
	KernelPTA -= KvaStart >> PAGE_SHIFT;

	/*
	 * phys_avail[] represents unallocated physical memory.  MI code
	 * will use phys_avail[] to create the vm_page array.
	 */
	phys_avail[0] = PAGE_SIZE +
			KERNEL_KVA_SIZE / PAGE_SIZE * sizeof(vpte_t);
	phys_avail[0] = (phys_avail[0] + PAGE_MASK) & ~(vm_paddr_t)PAGE_MASK;
	phys_avail[1] = Maxmem_bytes;

	/*
	 * (virtual_start, virtual_end) represent unallocated kernel virtual
	 * memory.  MI code will create kernel_map using these parameters.
	 */
	virtual_start = KvaStart + PAGE_SIZE +
			KERNEL_KVA_SIZE / PAGE_SIZE * sizeof(vpte_t);
	virtual_start = (virtual_start + PAGE_MASK) & ~(vm_offset_t)PAGE_MASK;
	virtual_end = KvaStart + KERNEL_KVA_SIZE;

	/*
	 * kernel_vm_end could be set to virtual_end but we want some 
	 * indication of how much of the kernel_map we've used, so
	 * set it low and let pmap_growkernel increase it even though we
	 * don't need to create any new page table pages.
	 */
	kernel_vm_end = virtual_start;

	/*
	 * Allocate space for process 0's UAREA.
	 */
	proc0paddr = (void *)virtual_start;
	for (i = 0; i < UPAGES; ++i) {
		pmap_kenter_quick(virtual_start, phys_avail[0]);
		virtual_start += PAGE_SIZE;
		phys_avail[0] += PAGE_SIZE;
	}

	/*
	 * crashdumpmap
	 */
	crashdumpmap = virtual_start;
	virtual_start += MAXDUMPPGS * PAGE_SIZE;

	/*
	 * msgbufp maps the system message buffer
	 */
	assert((MSGBUF_SIZE & PAGE_MASK) == 0);
	msgbufp = (void *)virtual_start;
	for (i = 0; i < (MSGBUF_SIZE >> PAGE_SHIFT); ++i) {
		pmap_kenter_quick(virtual_start, phys_avail[0]);
		virtual_start += PAGE_SIZE;
		phys_avail[0] += PAGE_SIZE;
	}
	msgbufinit(msgbufp, MSGBUF_SIZE);

	/*
	 * used by kern_memio for /dev/mem access
	 */
	ptvmmap = (caddr_t)virtual_start;
	virtual_start += PAGE_SIZE;

	/*
	 * Bootstrap the kernel_pmap
	 */
	pmap_bootstrap();
}

/*
 * Map the per-cpu globaldata for cpu #0.  Allocate the space using
 * virtual_start and phys_avail[0]
 */
static
void
init_globaldata(void)
{
	int i;
	vm_paddr_t pa;
	vm_offset_t va;

	/*
	 * Reserve enough KVA to cover possible cpus.  This is a considerable
	 * amount of KVA since the privatespace structure includes two 
	 * whole page table mappings.
	 */
	virtual_start = (virtual_start + SEG_MASK) & ~(vm_offset_t)SEG_MASK;
	CPU_prvspace = (void *)virtual_start;
	virtual_start += sizeof(struct privatespace) * SMP_MAXCPU;

	/*
	 * Allocate enough physical memory to cover the mdglobaldata
	 * portion of the space and the idle stack and map the pages
	 * into KVA.  For cpu #0 only.
	 */
	for (i = 0; i < sizeof(struct mdglobaldata); i += PAGE_SIZE) {
		pa = phys_avail[0];
		va = (vm_offset_t)&CPU_prvspace[0].mdglobaldata + i;
		pmap_kenter_quick(va, pa);
		phys_avail[0] += PAGE_SIZE;
	}
	for (i = 0; i < sizeof(CPU_prvspace[0].idlestack); i += PAGE_SIZE) {
		pa = phys_avail[0];
		va = (vm_offset_t)&CPU_prvspace[0].idlestack + i;
		pmap_kenter_quick(va, pa);
		phys_avail[0] += PAGE_SIZE;
	}

	/*
	 * Setup the %gs for cpu #0.  The mycpu macro works after this
	 * point.
	 */
	tls_set_fs(&CPU_prvspace[0], sizeof(struct privatespace));
}

/*
 * Initialize very low level systems including thread0, proc0, etc.
 */
static
void
init_vkernel(void)
{
	struct mdglobaldata *gd;

	gd = &CPU_prvspace[0].mdglobaldata;
	bzero(gd, sizeof(*gd));

	gd->mi.gd_curthread = &thread0;
	thread0.td_gd = &gd->mi;
	ncpus = 1;
	ncpus2 = 1;
	init_param1();
	gd->mi.gd_prvspace = &CPU_prvspace[0];
	mi_gdinit(&gd->mi, 0);
	cpu_gdinit(gd, 0);
	mi_proc0init(&gd->mi, proc0paddr);
	lwp0.lwp_md.md_regs = &proc0_tf;

	/*init_locks();*/
	cninit();
	rand_initialize();
#if 0	/* #ifdef DDB */
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger("Boot flags requested debugger");
#endif
#if 0
	initializecpu();	/* Initialize CPU registers */
#endif
	init_param2((phys_avail[1] - phys_avail[0]) / PAGE_SIZE);

#if 0
	/*
	 * Map the message buffer
	 */
	for (off = 0; off < round_page(MSGBUF_SIZE); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, avail_end + off);
	msgbufinit(msgbufp, MSGBUF_SIZE);
#endif
#if 0
	thread0.td_pcb_cr3 ... MMU
	lwp0.lwp_md.md_regs = &proc0_tf;
#endif
}

/*
 * The root filesystem path for the virtual kernel is optional.  If specified
 * it points to a filesystem image.
 *
 * The virtual kernel caches data from our 'disk' just like a normal kernel,
 * so we do not really want the real kernel to cache the data too.  Use
 * O_DIRECT to remove the duplication.
 */
static
void
init_rootdevice(char *imageFile)
{
	struct stat st;

	if (imageFile) {
		RootImageFd = open(imageFile, O_RDWR|O_DIRECT, 0644);
		if (RootImageFd < 0 || fstat(RootImageFd, &st) < 0) {
			err(1, "Unable to open/create %s", imageFile);
			/* NOT REACHED */
		}
		rootdevnames[0] = "ufs:vkd0a";
	}
}

static
int
netif_set_tapflags(int tap_unit, int f, int s)
{
	struct ifreq ifr;
	int flags;

	bzero(&ifr, sizeof(ifr));

	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "tap%d", tap_unit);
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
		warn("tap%d: ioctl(SIOCGIFFLAGS) failed", tap_unit);
		return -1;
	}

	/*
	 * Adjust if_flags
	 *
	 * If the flags are already set/cleared, then we return
	 * immediately to avoid extra syscalls
	 */
	flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);
	if (f < 0) {
		/* Turn off flags */
		f = -f;
		if ((flags & f) == 0)
			return 0;
		flags &= ~f;
	} else {
		/* Turn on flags */
		if (flags & f)
			return 0;
		flags |= f;
	}

	/*
	 * Fix up ifreq.ifr_name, since it may be trashed
	 * in previous ioctl(SIOCGIFFLAGS)
	 */
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "tap%d", tap_unit);

	ifr.ifr_flags = flags & 0xffff;
	ifr.ifr_flagshigh = flags >> 16;
	if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
		warn("tap%d: ioctl(SIOCSIFFLAGS) failed", tap_unit);
		return -1;
	}
	return 0;
}

static
int
netif_set_tapaddr(int tap_unit, in_addr_t addr, in_addr_t mask, int s)
{
	struct ifaliasreq ifra;
	struct sockaddr_in *in;

	bzero(&ifra, sizeof(ifra));
	snprintf(ifra.ifra_name, sizeof(ifra.ifra_name), "tap%d", tap_unit);

	/* Setup address */
	in = (struct sockaddr_in *)&ifra.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(*in);
	in->sin_addr.s_addr = addr;

	if (mask != 0) {
		/* Setup netmask */
		in = (struct sockaddr_in *)&ifra.ifra_mask;
		in->sin_len = sizeof(*in);
		in->sin_addr.s_addr = mask;
	}

	if (ioctl(s, SIOCAIFADDR, &ifra) < 0) {
		warn("tap%d: ioctl(SIOCAIFADDR) failed", tap_unit);
		return -1;
	}
	return 0;
}

static
int
netif_add_tap2brg(int tap_unit, const char *ifbridge, int s)
{
	struct ifbreq ifbr;
	struct ifdrv ifd;

	bzero(&ifbr, sizeof(ifbr));
	snprintf(ifbr.ifbr_ifsname, sizeof(ifbr.ifbr_ifsname),
		 "tap%d", tap_unit);

	bzero(&ifd, sizeof(ifd));
	strlcpy(ifd.ifd_name, ifbridge, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = BRDGADD;
	ifd.ifd_len = sizeof(ifbr);
	ifd.ifd_data = &ifbr;

	if (ioctl(s, SIOCSDRVSPEC, &ifd) < 0) {
		/*
		 * 'errno == EEXIST' means that the tap(4) is already
		 * a member of the bridge(4)
		 */
		if (errno != EEXIST) {
			warn("ioctl(%s, SIOCSDRVSPEC) failed", ifbridge);
			return -1;
		}
	}
	return 0;
}

#define TAPDEV_OFLAGS	(O_RDWR | O_NONBLOCK)

/* XXX major()/minor() can't be used in vkernel */
#define TAPDEV_MAJOR(x)	((int)(((u_int)(x) >> 8) & 0xff))
#define TAPDEV_MINOR(x)	((int)((x) & 0xffff00ff))

#ifndef TAP_CDEV_MAJOR
#define TAP_CDEV_MAJOR	149
#endif

/*
 * Locate the first unused tap(4) device file if auto mode is requested,
 * or open the user supplied device file, and bring up the corresponding
 * tap(4) interface.
 *
 * NOTE: Only tap(4) device file is supported currently
 */
static
int
netif_open_tap(const char *netif, int *tap_unit, int s)
{
	char tap_dev[MAXPATHLEN];
	int tap_fd, failed;
	struct stat st;

	*tap_unit = -1;

	if (strcmp(netif, "auto") == 0) {
		int i;

		/*
		 * Find first unused tap(4) device file
		 */
		for (i = 0; ; ++i) {
			snprintf(tap_dev, sizeof(tap_dev), "/dev/tap%d", i);
			tap_fd = open(tap_dev, TAPDEV_OFLAGS);
			if (tap_fd >= 0 || errno == ENOENT)
				break;
		}
		if (tap_fd < 0) {
			warnx("Unable to find a free tap(4)");
			return -1;
		}
	} else {
		/*
		 * User supplied tap(4) device file
		 */
		if (netif[0] == '/')	/* Absolute path */
			strlcpy(tap_dev, netif, sizeof(tap_dev));
		else
			snprintf(tap_dev, sizeof(tap_dev), "/dev/%s", netif);

		tap_fd = open(tap_dev, TAPDEV_OFLAGS);
		if (tap_fd < 0) {
			warn("Unable to open %s", tap_dev);
			return -1;
		}
	}

	/*
	 * Check whether the device file is a tap(4)
	 */
	failed = 1;
	if (fstat(tap_fd, &st) == 0 && S_ISCHR(st.st_mode) &&
	    TAPDEV_MAJOR(st.st_rdev) == TAP_CDEV_MAJOR) {
		*tap_unit = TAPDEV_MINOR(st.st_rdev);

		/*
		 * Bring up the corresponding tap(4) interface
		 */
		if (netif_set_tapflags(*tap_unit, IFF_UP, s) == 0)
			failed = 0;
	} else {
		warnx("%s is not a tap(4) device", tap_dev);
	}

	if (failed) {
		close(tap_fd);
		tap_fd = -1;
		*tap_unit = -1;
	}
	return tap_fd;
}

#undef TAPDEV_MAJOR
#undef TAPDEV_MINOR
#undef TAPDEV_OFLAGS

/*
 * Following syntax is supported,
 * 1) x.x.x.x             tap(4)'s address is x.x.x.x
 *
 * 2) x.x.x.x/z           tap(4)'s address is x.x.x.x
 *                        tap(4)'s netmask len is z
 *
 * 3) x.x.x.x:y.y.y.y     tap(4)'s address is x.x.x.x
 *                        pseudo netif's address is y.y.y.y
 *
 * 4) x.x.x.x:y.y.y.y/z   tap(4)'s address is x.x.x.x
 *                        pseudo netif's address is y.y.y.y
 *                        tap(4) and pseudo netif's netmask len are z
 *
 * 5) bridgeX             tap(4) will be added to bridgeX
 *
 * 6) bridgeX:y.y.y.y     tap(4) will be added to bridgeX
 *                        pseudo netif's address is y.y.y.y
 *
 * 7) bridgeX:y.y.y.y/z   tap(4) will be added to bridgeX
 *                        pseudo netif's address is y.y.y.y
 *                        pseudo netif's netmask len is z
 */
static
int
netif_init_tap(int tap_unit, in_addr_t *addr, in_addr_t *mask, int s)
{
	in_addr_t tap_addr, netmask, netif_addr;
	int next_netif_addr;
	char *tok, *masklen_str, *ifbridge;

	*addr = 0;
	*mask = 0;

	tok = strtok(NULL, ":/");
	if (tok == NULL) {
		/*
		 * Nothing special, simply use tap(4) as backend
		 */
		return 0;
	}

	if (inet_pton(AF_INET, tok, &tap_addr) > 0) {
		/*
		 * tap(4)'s address is supplied
		 */
		ifbridge = NULL;

		/*
		 * If there is next token, then it may be pseudo
		 * netif's address or netmask len for tap(4)
		 */
		next_netif_addr = 0;
	} else {
		/*
		 * Not tap(4)'s address, assume it as a bridge(4)
		 * iface name
		 */
		tap_addr = 0;
		ifbridge = tok;

		/*
		 * If there is next token, then it must be pseudo
		 * netif's address
		 */
		next_netif_addr = 1;
	}

	netmask = netif_addr = 0;

	tok = strtok(NULL, ":/");
	if (tok == NULL)
		goto back;

	if (inet_pton(AF_INET, tok, &netif_addr) <= 0) {
		if (next_netif_addr) {
			warnx("Invalid pseudo netif address: %s", tok);
			return -1;
		}
		netif_addr = 0;

		/*
		 * Current token is not address, then it must be netmask len
		 */
		masklen_str = tok;
	} else {
		/*
		 * Current token is pseudo netif address, if there is next token
		 * it must be netmask len
		 */
		masklen_str = strtok(NULL, "/");
	}

	/* Calculate netmask */
	if (masklen_str != NULL) {
		u_long masklen;

		masklen = strtoul(masklen_str, NULL, 10);
		if (masklen < 32 && masklen > 0) {
			netmask = htonl(~((1LL << (32 - masklen)) - 1)
					& 0xffffffff);
		} else {
			warnx("Invalid netmask len: %lu", masklen);
			return -1;
		}
	}

	/* Make sure there is no more token left */
	if (strtok(NULL, ":/") != NULL) {
		warnx("Invalid argument to '-I'");
		return -1;
	}

back:
	if (ifbridge == NULL) {
		/* Set tap(4) address/netmask */
		if (netif_set_tapaddr(tap_unit, tap_addr, netmask, s) < 0)
			return -1;
	} else {
		/* Tie tap(4) to bridge(4) */
		if (netif_add_tap2brg(tap_unit, ifbridge, s) < 0)
			return -1;
	}

	*addr = netif_addr;
	*mask = netmask;
	return 0;
}

/*
 * NetifInfo[] will be filled for pseudo netif initialization.
 * NetifNum will be bumped to reflect the number of valid entries
 * in NetifInfo[].
 */
static
void
init_netif(char *netifExp[], int netifExpNum)
{
	int i, s;

	if (netifExpNum == 0)
		return;

	s = socket(AF_INET, SOCK_DGRAM, 0);	/* for ioctl(SIOC) */
	if (s < 0)
		return;

	for (i = 0; i < netifExpNum; ++i) {
		struct vknetif_info *info;
		in_addr_t netif_addr, netif_mask;
		int tap_fd, tap_unit;
		char *netif;

		netif = strtok(netifExp[i], ":");
		if (netif == NULL) {
			warnx("Invalide argument to '-I'");
			continue;
		}

		/*
		 * Open tap(4) device file and bring up the
		 * corresponding interface
		 */
		tap_fd = netif_open_tap(netif, &tap_unit, s);
		if (tap_fd < 0)
			continue;

		/*
		 * Initialize tap(4) and get address/netmask
		 * for pseudo netif
		 *
		 * NB: Rest part of netifExp[i] is passed
		 *     to netif_init_tap() implicitly.
		 */
		if (netif_init_tap(tap_unit, &netif_addr, &netif_mask, s) < 0) {
			/*
			 * NB: Closing tap(4) device file will bring
			 *     down the corresponding interface
			 */
			close(tap_fd);
			continue;
		}

		info = &NetifInfo[NetifNum];
		info->tap_fd = tap_fd;
		info->tap_unit = tap_unit;
		info->netif_addr = netif_addr;
		info->netif_mask = netif_mask;

		NetifNum++;
		if (NetifNum >= VKNETIF_MAX)	/* XXX will this happen? */
			break;
	}
	close(s);
}

static
void
usage(const char *ctl)
{
	
}

void
cpu_reset(void)
{
	kprintf("cpu reset\n");
	exit(0);
}

void
cpu_halt(void)
{
	kprintf("cpu halt\n");
	for (;;)
		__asm__ __volatile("hlt");
}
