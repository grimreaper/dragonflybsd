/*
 * Copyright (c) 2003 Craig Rodrigues <rodrigc@attbi.com>.
 * Copyright (c) 2002,2003 Alexey Zelkin <phantom@FreeBSD.org>
 * Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
 * Copyright (c) 1996 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/tls.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <pthread_np.h>
#include "un-namespace.h"

#include "thr_private.h"

int
_pthread_attr_destroy(pthread_attr_t *attr)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL)
		/* Invalid argument: */
		ret = EINVAL;
	else {
		/* Free the memory allocated to the attribute object: */
		free(*attr);

		/*
		 * Leave the attribute pointer NULL now that the memory
		 * has been freed:
		 */
		*attr = NULL;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_destroy, pthread_attr_destroy);

int
_pthread_attr_get_np(pthread_t pid, pthread_attr_t *dst)
{
	struct pthread *curthread;
	struct pthread_attr attr;
	int	ret;

	if (pid == NULL || dst == NULL || *dst == NULL)
		return (EINVAL);

	curthread = tls_get_curthread();
	if ((ret = _thr_ref_add(curthread, pid, /*include dead*/0)) != 0)
		return (ret);
	attr = pid->attr;
	if (pid->tlflags & TLFLAGS_DETACHED)
		attr.flags |= PTHREAD_DETACHED;
	_thr_ref_delete(curthread, pid);
	memcpy(*dst, &attr, sizeof(struct pthread_attr));

	return (0);
}

__strong_reference(_pthread_attr_get_np, pthread_attr_get_np);

int
_pthread_attr_getaffinity_np(const pthread_attr_t *attr, size_t cpusetsize,
    cpu_set_t *mask)
{
	const cpu_set_t *ret;
	cpu_set_t mask1;

	if (attr == NULL || *attr == NULL || mask == NULL)
		return (EINVAL);

	if (((*attr)->flags & THR_CPUMASK) == 0) {
		size_t len;

		len = sizeof(mask1);
		if (sysctlbyname("machdep.smp_active", &mask1, &len,
		    NULL, 0) < 0)
			return (errno);
		ret = &mask1;
	} else {
		ret = &(*attr)->cpumask;
	}

	if (cpusetsize > sizeof(*ret)) {
		memset(mask, 0, cpusetsize);
		memcpy(mask, ret, sizeof(*ret));
	} else {
		memcpy(mask, ret, cpusetsize);
	}
	return (0);
}

__strong_reference(_pthread_attr_getaffinity_np, pthread_attr_getaffinity_np);

int
_pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || detachstate == NULL)
		ret = EINVAL;
	else {
		/* Check if the detached flag is set: */
		if ((*attr)->flags & PTHREAD_DETACHED)
			/* Return detached: */
			*detachstate = PTHREAD_CREATE_DETACHED;
		else
			/* Return joinable: */
			*detachstate = PTHREAD_CREATE_JOINABLE;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_getdetachstate, pthread_attr_getdetachstate);

int
_pthread_attr_getguardsize(const pthread_attr_t * __restrict attr,
    size_t * __restrict guardsize)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || guardsize == NULL)
		ret = EINVAL;
	else {
		/* Return the guard size: */
		*guardsize = (*attr)->guardsize_attr;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_getguardsize, pthread_attr_getguardsize);

int
_pthread_attr_getinheritsched(const pthread_attr_t * __restrict attr,
    int * __restrict sched_inherit)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL))
		ret = EINVAL;
	else
		*sched_inherit = (*attr)->sched_inherit;

	return(ret);
}

__strong_reference(_pthread_attr_getinheritsched, pthread_attr_getinheritsched);

int
_pthread_attr_getschedparam(const pthread_attr_t * __restrict attr,
    struct sched_param * __restrict param)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL) || (param == NULL))
		ret = EINVAL;
	else
		param->sched_priority = (*attr)->prio;

	return(ret);
}

__strong_reference(_pthread_attr_getschedparam, pthread_attr_getschedparam);

int
_pthread_attr_getschedpolicy(const pthread_attr_t * __restrict attr,
    int * __restrict policy)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL) || (policy == NULL))
		ret = EINVAL;
	else
		*policy = (*attr)->sched_policy;

	return(ret);
}

__strong_reference(_pthread_attr_getschedpolicy, pthread_attr_getschedpolicy);

int
_pthread_attr_getscope(const pthread_attr_t * __restrict attr,
    int * __restrict contentionscope)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL) || (contentionscope == NULL))
		/* Return an invalid argument: */
		ret = EINVAL;

	else
		*contentionscope = ((*attr)->flags & PTHREAD_SCOPE_SYSTEM) ?
		    PTHREAD_SCOPE_SYSTEM : PTHREAD_SCOPE_PROCESS;

	return(ret);
}

__strong_reference(_pthread_attr_getscope, pthread_attr_getscope);

int
_pthread_attr_getstack(const pthread_attr_t * __restrict attr,
		       void ** __restrict stackaddr,
		       size_t * __restrict stacksize)
{
	int     ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || stackaddr == NULL
	    || stacksize == NULL )
		ret = EINVAL;
	else {
		/* Return the stack address and size */
		*stackaddr = (*attr)->stackaddr_attr;
		*stacksize = (*attr)->stacksize_attr;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_getstack, pthread_attr_getstack);

int
_pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || stackaddr == NULL)
		ret = EINVAL;
	else {
		/* Return the stack address: */
		*stackaddr = (*attr)->stackaddr_attr;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_getstackaddr, pthread_attr_getstackaddr);

int
_pthread_attr_getstacksize(const pthread_attr_t * __restrict attr,
    size_t * __restrict stacksize)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || stacksize  == NULL)
		ret = EINVAL;
	else {
		/* Return the stack size: */
		*stacksize = (*attr)->stacksize_attr;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_getstacksize, pthread_attr_getstacksize);

int
_pthread_attr_init(pthread_attr_t *attr)
{
	int	ret;
	pthread_attr_t	pattr;

	/* Allocate memory for the attribute object: */
	if ((pattr = (pthread_attr_t) malloc(sizeof(struct pthread_attr))) == NULL)
		/* Insufficient memory: */
		ret = ENOMEM;
	else {
		/* Initialise the attribute object with the defaults: */
		memcpy(pattr, &_pthread_attr_default,
		    sizeof(struct pthread_attr));

		/* Return a pointer to the attribute object: */
		*attr = pattr;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_init, pthread_attr_init);

int
_pthread_attr_setaffinity_np(pthread_attr_t *attr, size_t cpusetsize,
    const cpu_set_t *mask)
{
	cpu_set_t active, mask1;
	size_t len, cplen = cpusetsize;

	if (attr == NULL || *attr == NULL || mask == NULL)
		return (EINVAL);

	if (cplen > sizeof(mask1))
		cplen = sizeof(mask1);
	CPU_ZERO(&mask1);
	memcpy(&mask1, mask, cplen);

	len = sizeof(active);
	if (sysctlbyname("machdep.smp_active", &active, &len, NULL, 0) < 0)
		return (errno);

	CPUMASK_ANDMASK(mask1, active);
	if (CPUMASK_TESTZERO(mask1))
		return (EPERM);

	(*attr)->cpumask = mask1;
	(*attr)->flags |= THR_CPUMASK;
	return (0);
}

__strong_reference(_pthread_attr_setaffinity_np, pthread_attr_setaffinity_np);

int
_pthread_attr_setcreatesuspend_np(pthread_attr_t *attr)
{
	int	ret;

	if (attr == NULL || *attr == NULL) {
		ret = EINVAL;
	} else {
		(*attr)->suspend = THR_CREATE_SUSPENDED;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_setcreatesuspend_np, pthread_attr_setcreatesuspend_np);

int
_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL ||
	    (detachstate != PTHREAD_CREATE_DETACHED &&
	    detachstate != PTHREAD_CREATE_JOINABLE))
		ret = EINVAL;
	else {
		/* Check if detached state: */
		if (detachstate == PTHREAD_CREATE_DETACHED)
			/* Set the detached flag: */
			(*attr)->flags |= PTHREAD_DETACHED;
		else
			/* Reset the detached flag: */
			(*attr)->flags &= ~PTHREAD_DETACHED;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_setdetachstate, pthread_attr_setdetachstate);

int
_pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
	int	ret;

	/* Check for invalid arguments. */
	if (attr == NULL || *attr == NULL)
		ret = EINVAL;
	else {
		/* Save the stack size. */
		(*attr)->guardsize_attr = guardsize;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_setguardsize, pthread_attr_setguardsize);

int
_pthread_attr_setinheritsched(pthread_attr_t *attr, int sched_inherit)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL))
		ret = EINVAL;
	else if (sched_inherit != PTHREAD_INHERIT_SCHED &&
		 sched_inherit != PTHREAD_EXPLICIT_SCHED)
		ret = EINVAL;
	else
		(*attr)->sched_inherit = sched_inherit;

	return(ret);
}

__strong_reference(_pthread_attr_setinheritsched, pthread_attr_setinheritsched);

int
_pthread_attr_setschedparam(pthread_attr_t * __restrict attr,
    const struct sched_param * __restrict param)
{
	int policy;

	if ((attr == NULL) || (*attr == NULL))
		return (EINVAL);

	if (param == NULL)
		return (ENOTSUP);

	policy = (*attr)->sched_policy;

	{
		int minv = sched_get_priority_min(policy);
		int maxv = sched_get_priority_max(policy);
		if (minv == -1 || maxv == -1 ||
		    param->sched_priority < minv ||
		    param->sched_priority > maxv) {
			return (ENOTSUP);
		}
	}

	(*attr)->prio = param->sched_priority;

	return (0);
}

__strong_reference(_pthread_attr_setschedparam, pthread_attr_setschedparam);

int
_pthread_attr_setschedpolicy(pthread_attr_t *attr, int pol)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL))
		ret = EINVAL;
	else if (pol != SCHED_FIFO && pol != SCHED_OTHER && pol != SCHED_RR)
		ret = EINVAL;
	else
		(*attr)->sched_policy = pol;

	return(ret);
}

__strong_reference(_pthread_attr_setschedpolicy, pthread_attr_setschedpolicy);

int
_pthread_attr_setscope(pthread_attr_t *attr, int contentionscope)
{
	int ret = 0;

	if ((attr == NULL) || (*attr == NULL)) {
		/* Return an invalid argument: */
		ret = EINVAL;
	} else if ((contentionscope != PTHREAD_SCOPE_PROCESS) &&
	    (contentionscope != PTHREAD_SCOPE_SYSTEM)) {
		ret = EINVAL;
	} else if (contentionscope == PTHREAD_SCOPE_SYSTEM) {
		(*attr)->flags |= contentionscope;
	} else {
		(*attr)->flags &= ~PTHREAD_SCOPE_SYSTEM;
	}
	return (ret);
}

__strong_reference(_pthread_attr_setscope, pthread_attr_setscope);

int
_pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr,
		       size_t stacksize)
{
	int     ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || stackaddr == NULL
	    || stacksize < PTHREAD_STACK_MIN)
		ret = EINVAL;
	else {
		/* Save the stack address and stack size */
		(*attr)->stackaddr_attr = stackaddr;
		(*attr)->stacksize_attr = stacksize;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_setstack, pthread_attr_setstack);

int
_pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || stackaddr == NULL)
		ret = EINVAL;
	else {
		/* Save the stack address: */
		(*attr)->stackaddr_attr = stackaddr;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_setstackaddr, pthread_attr_setstackaddr);

int
_pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	int	ret;

	/* Check for invalid arguments: */
	if (attr == NULL || *attr == NULL || stacksize < PTHREAD_STACK_MIN)
		ret = EINVAL;
	else {
		/* Save the stack size: */
		(*attr)->stacksize_attr = stacksize;
		ret = 0;
	}
	return(ret);
}

__strong_reference(_pthread_attr_setstacksize, pthread_attr_setstacksize);
