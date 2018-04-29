/*
 * Copyright (c) 2016, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hermit/stddef.h>
#include <hermit/stdio.h>
#include <hermit/tasks.h>
#include <hermit/errno.h>
#include <hermit/syscall.h>
#include <hermit/logging.h>
#include <hermit/syscall_disabler.h>
#include "syscall.h"

void __startcontext(void);

volatile extern void dummy_asm_func(void);

/* Hack to enable compilation of the assembly code generated by the 
   syscall rewriter */
void dummy_func()
{
	dummy_asm_func();
}

struct fast_sc_state {
	uint64_t r9;
	uint64_t r8;
	uint64_t r10;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rax;
};

void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	va_list ap;

	if (BUILTIN_EXPECT(!ucp, 0))
		return;

	LOG_DEBUG("sys_makecontext %p, func %p, stack 0x%zx, task %d\n", ucp, func, ucp->uc_stack.ss_sp, per_core(current_task)->id);

	size_t* stack = (size_t*) ((size_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	stack -= (argc > 6 ? argc - 6 : 0) + 1;
	uint32_t idx = (argc > 6 ? argc - 6 : 0) + 1;

	/* Align stack and reserve space for trampoline address.  */
	stack = (size_t*) ((((size_t) stack) & ~0xFULL) - 0x8);

	/* Setup context */
	ucp->uc_mregs.rip = (size_t) func;
	ucp->uc_mregs.rbx = (size_t) &stack[idx];
	ucp->uc_mregs.rsp = (size_t) stack;

	stack[0] = (size_t) &__startcontext;
	stack[idx] = (size_t) ucp->uc_link; // link to the next context

	va_start(ap, argc);
	for (int i = 0; i < argc; i++)
	{
		switch (i)
		{
		case 0:
			ucp->uc_mregs.rdi = va_arg(ap, size_t);
			break;
		case 1:
			ucp->uc_mregs.rsi = va_arg(ap, size_t);
			break;
		case 2:
			ucp->uc_mregs.rdx = va_arg(ap, size_t);
			break;
		case 3:
			ucp->uc_mregs.rcx = va_arg(ap, size_t);
			break;
		case 4:
			ucp->uc_mregs.r8 = va_arg(ap, size_t);
			break;
		case 5:
			ucp->uc_mregs.r9 = va_arg(ap, size_t);
			break;
		default:
			/* copy value on stack */
			stack[i - 5] = va_arg(ap, size_t);
			break;
		}
	}
	va_end(ap);
}

int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	//TODO: implementation is missing

	LOG_WARNING("sys_swapcontext is currently not implemented: %p <=> %p\n", oucp, ucp);
	return -ENOSYS;
}


void fast_syscall_handler(struct fast_sc_state *s)
{
	//uint64_t rax, rdi, rsi, rdx, r10, r8, r9, ret;
	
	/* LOG_INFO("DC: In fast_syscall_handler\n"); */
	/* LOG_INFO("DC: RAX = %#llx, RDI = %#llx, RSI = %#llx, RDX = %#llx, " */
	/* 	 "R10 = %#llx, R8 = %#llx, R9 = %#llx\n", */
	/* 	 s->rax, s->rdi, s->rsi, s->rdx, s->r10, s->r8, s->r9); */
	
	
	s->rax = redirect_syscall(s->rax, s->rdi, s->rsi, s->rdx, s->r10, s->r8, s->r9);

	//LOG_INFO("DC: Returned from fast syscall. Return = %#llx\n", s->rax);
	/*
	  asm volatile  (	"mov	%%rax,%0\n\t"
	  "mov	%%rdi,%1\n\t"
	  "mov	%%rsi,%2\n\t"
	  "mov 	%%rdx,%3\n\t"
	  "mov 	%%r10,%4\n\t"
	  "mov 	%%r8,%5\n\t"
	  "mov 	%%r9,%6\n\t"
	  : "=r" (rax), "=r" (rdi), "=r" (rsi), "=r" (rdx), "=r" (r10),
	  "=r" (r8), "=r" (r9)
	  :
	  :
	  );

	  LOG_INFO("DC: RAX = %#llx, RDI = %#llx, RSI = %#llx, RDX = %#llx, R10 = %#llx, R8 = %#llx, R9 = %#llx\n",
	  rax, rdi, rsi, rdx, r10, r8, r9);
	
	  ret = redirect_syscall(rax, rdi, rsi, rdx, r10, r8, r9);

	  asm volatile ( "mov  %0,%%rax\n\t"
	  :
	  : "r" (ret)
	  : "%rax"
	  );
	*/
	
}


uint64_t redirect_syscall(uint64_t rax, uint64_t rdi, uint64_t rsi, uint64_t rdx,
			  uint64_t r10, uint64_t r8, uint64_t r9)
{
	uint64_t ret;

	switch(rax) {

#ifndef DISABLE_SYS_READ
	case 0:
		/* read */
		ret = sys_read(s->rdi, (char *)s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_READ */

#ifndef DISABLE_SYS_WRITE
	case 1:
		/* write */
		ret = sys_write(s->rdi, (char *)s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_WRITE */

#ifndef DISABLE_SYS_OPEN
	case 2:
		/* open */
		ret = sys_open((const char *)s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_OPEN */

#ifndef DISABLE_SYS_CLOSE
	case 3:
		/* close */
		ret = sys_close(s->rdi);
		break;
#endif /* DISABLE_SYS_CLOSE */

#ifndef DISABLE_SYS_STAT
	case 4:
		/* stat */
		ret = sys_stat((const char *)s->rdi, (struct stat *)s->rsi);
		break;
#endif /* DISABLE_SYS_STAT */

#ifndef DISABLE_SYS_FSTAT
	case 5:
		/* fstat */
		ret = sys_fstat(s->rdi, (struct stat *)s->rsi);
		break;
#endif /* DISABLE_SYS_FSTAT */

#ifndef DISABLE_SYS_LSTAT
	case 6:
		/* lstat */
		ret = sys_lstat((const char *)s->rdi, (struct stat *)s->rsi);
		break;
#endif /* DISABLE_SYS_LSTAT */

#ifndef DISABLE_SYS_LSEEK
	case 8:
		/* lseek */
		ret = sys_lseek(s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_LSEEK */

#ifndef DISABLE_SYS_MMAP /* encompasses mmap and munmap */
	case 9:
		/* mmap */
		ret = sys_mmap(s->rdi, s->rsi, s->rdx, s->r10, s->r8,
			       s->r9);
		break;
#endif /* DISABLE_SYS_MMAP */

#ifndef DISABLE_SYS_MPROTECT
	case 10:
		/* mprotect */
		ret = sys_mprotect(s->rsi, s->rdi, s->rdx);
		break;
#endif /* DISABLE_SYS_MPROTECT */

#ifndef DISABLE_SYS_MUNMAP
	case 11:
		/* munmap */
		ret = sys_munmap(s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_MUNMAP */

#ifndef DISABLE_SYS_BRK
	case 12:
		/* brk */
		ret = sys_brk(s->rdi);
		break;
#endif /* DISABLE_SYS_BRK */

#ifndef DISABLE_SYS_RT_SIGACTION
	case 13:
		/* rt_sigaction */
		ret = sys_rt_sigaction(s->rdi, 
				       (struct sigaction *)s->rsi,
				       (struct sigaction *)s->rdx);
		break;
#endif /* DISABLE_SYS_RT_SIGACTION */

#ifndef DISABLE_SYS_RT_SIGPROCMASK
	case 14:
		/* rt_sigprocmask */
		/* FIXME */
		ret = 0;
		break;
#endif /* DISABLE_SYS_RT_SIGPROCMASK */

#ifndef DISABLE_SYS_IOCTL
	case 16:
		/* ioctl */
		ret = sys_ioctl(s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_IOCTL */

#ifndef DISABLE_SYS_READV
	case 19:
		/* readv */
		ret = sys_readv(s->rdi, (const struct iovec *)s->rsi,
				s->rdx);
		break;
#endif /* DISABLE_SYS_READV */

#ifndef DISABLE_SYS_WRITEV
	case 20:
		/* writev */
		ret = sys_writev(s->rdi, (const struct iovec *)s->rsi,
				 s->rdx);
		break;
#endif /* DISABLE_SYS_WRITEV */

#ifndef DISABLE_SYS_ACCESS
	case 21:
		/* access */
		ret = sys_access((const char *)s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_ACCESS */

#ifndef DISABLE_SYS_SCHED_YIELD
	case 24:
		/* sched_yield */
		ret = sys_sched_yield();
		break;
#endif /* DISABLE_SYS_SCHED_YIELD */

#ifndef DISABLE_SYS_MADVISE
	case 28:
		/* madvise */
		ret = sys_madvise(s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_MADVISE */

#ifndef DISABLE_SYS_NANOSLEEP
	case 35:
		/* nanosleep */
		ret = sys_nanosleep((struct timespec *)s->rdi, 
				    (struct timespec *)s->rsi);
#endif /* DISABLE_SYS_NANOSLEEP */

#ifndef DISABLE_SYS_GETPID
	case 39:
		/* getpid */
		ret = sys_getpid();
		break;
#endif /* DISABLE_SYS_GETPID */

#ifndef DISABLE_SYS_SOCKET
	case 41:
		/* socket */
		ret = sys_socket(s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_SOCKET */

#ifndef NO_NET
#ifndef DISABLE_SYS_CONNECT
	case 42:
		/* connect */
		ret = lwip_connect(s->rdi, (const struct sockaddr*) s->rsi, s->rdx);
		break;
#endif
#endif /* NO_NET */

#ifndef NO_NET
#ifndef DISABLE_SYS_ACCEPT
	case 43:
		/* accept */
		ret = lwip_accept(s->rdi, (struct sockaddr *) s->rsi, (unsigned int *)s->rdx);
		break;
#endif /* DISABLE_SYS_ACCEPT */
#endif /* NO_NET */

#ifndef NO_NET
#ifndef DISABLE_SYS_RECVFROM
	case 45:
		/* recvfrom */
		ret = lwip_recvfrom(s->rdi, (void *)s->rsi, s->rdx, s->r10, (struct sockaddr *)s->r8, (unsigned int *)s->r9);
		break;
#endif /* DISABLE_SYS_RECVFROM */
#endif /* NO_NET */

#ifndef NO_NET
#ifndef DISABLE_SYS_SHUTDOWN
	case 48:
		/* shutdown */
		ret = lwip_shutdown(s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_SHUTDOWN */
#endif /* NO_NET */

#ifndef NO_NET
#ifndef DISABLE_SYS_BIND
	case 49:
		/* bind */
		ret = sys_bind(s->rdi, (struct sockaddr *)s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_BIND */
#endif /* NO_NET */

#ifndef NO_NET
#ifndef DISABLE_SYS_LISTEN
	case 50:
		/* lsiten */
		ret = lwip_listen(s->rdi, s->rsi);
		break;
#endif
#endif /* NO_NET */

#ifndef NO_NET
#ifndef DISABLE_SYS_GETSOCKNAME
	case 51:
		/* getsockname */
		ret = lwip_getsockname(s->rdi, (struct sockaddr *)s->rsi, (unsigned int *)s->rdx);
		break;
#endif
#endif /* NO_NET */

#ifndef DISABLE_SYS_SETSOCKOPT
	case 54:
		/* setsockopt */
		ret = sys_setsockopt(s->rdi, s->rsi, s->rdx, (char *)s->r10, s->r8);
		break;
#endif /* DISABLE_SYS_SETSOCKOPT */

#ifndef DISABLE_SYS_CLONE
	case 56:
		/* clone */
		ret = sys_clone(s->rdi, (void *)s->rsi, (int *)s->rdx, (int *)s->r10, (void *)s->r8, (void *)s->r9);
		break;
#endif /* DISABLE_SYS_CLONE */

#ifndef DISABLE_SYS_EXIT
	case 60:
		/* exit */
		sys_exit(s->rdi);
		LOG_ERROR("Should not reach here after exit ... \n");
		break;
#endif /* DISABLE_SYS_EXIT */

#ifndef DISABLE_SYS_UNAME
	case 63:
		/* uname */
		ret = sys_uname((void *)s->rdi);
		break;
#endif /* DISABLE_SYS_UNAME */

#ifndef DISABLE_SYS_FCNTL
	case 72:
		/* fcntl */
		ret = sys_fcntl(s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_FCNTL */

#ifndef DISABLE_SYS_GETCWD
	case 79:
		/*getcwd */
		ret = sys_getcwd((char *)s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_GETCWD */

#ifndef DISABLE_SYS_MKDIR
	case 83:
		/* mkdir */
		ret = sys_mkdir((const char *)s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_MKDIR */

#ifndef DISABLE_SYS_RMDIR
	case 84:
		/* rmdir */
		ret = sys_rmdir((const char *)s->rdi);
		break;
#endif /* DISABLE_SYS_RMDIR */

#ifndef DISABLE_SYS_UNLINK
	case 87:
		/* unlink */
		ret = sys_unlink((const char *)s->rdi);
		break;
#endif /* DISABLE_SYS_UNLINK */

#ifndef DISABLE_SYS_READLINK
	case 89:
		/* readlink */
		ret = sys_readlink((char *)s->rdi, (char *)s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_READLINK */

#ifndef DISABLE_SYS_GETTIMEOFDAY
	case 96:
		/* gettimeofday */
		ret = sys_gettimeofday((struct timeval *)s->rdi,
				       (struct timezone *)s->rsi);
		break;
#endif /* DISABLE_SYS_GETTIMEOFDAY */

#ifndef DISABLE_SYS_GETRLIMIT
	case 97:
		/* getrlimit */
		ret = sys_getrlimit(s->rdi, (struct rlimit *)s->rsi);
		break;
#endif /* DISABLE_SYS_GETRLIMIT */

#ifndef DISABLE_SYS_SYSINFO
	case 99:
		/* sysinfo */
		ret = sys_sysinfo((void *)s->rdi);
		break;
#endif

#ifndef DISABLE_SYS_GETUID
	case 102:
		/* getuid */
		ret = sys_getuid();
		break;
#endif /* DISABLE_SYS_GETUID */

#ifndef DISABLE_SYS_GETGID
	case 104:
		ret = sys_getgid();
		break;
#endif /* DISABLE_SYS_GETGID */

#ifndef DISABLE_SYS_GETEUID
	case 107:
		/* geteuid */
		ret = sys_geteuid();
		break;
#endif /* DISABLE_SYS_GETEUID */

#ifndef DISABLE_SYS_GETEGID
	case 108:
		ret = sys_getegid();
		break;
#endif /* DISABLE_SYS_GETEGID */

#ifndef DISABLE_SYS_GETPPID
	case 110:
		ret = (long)0;
		ret = sys_getppid();
		break;
#endif /* DISABLE_SYS_GETPPID */

#ifndef DISABLE_SYS_GETPRIORITY
	case 140:
		/* getpriority */
		ret = sys_getpriority(s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_GETPRIORITY */

#ifndef DISABLE_SYS_SETPRIORITY
	case 141:
		/* setpriority */
		ret = sys_setpriority(s->rdi, s->rsi, s->rdx);
		break;
#endif

#ifndef DISABLE_SYS_ARCH_PRCTL
	case 158:
		/* arch_prctl */
		ret = sys_arch_prctl(s->rdi, (unsigned long *)s->rsi, s);
		break;
#endif /* DISABLE_SYS_ARCH_PRCTL */

#ifndef DISABLE_SYS_SETRLIMIT
	case 160:
		/* setrlimit */
		ret = sys_setrlimit(s->rdi, (void *)s->rsi);
		break;
#endif

#ifndef DISABLE_SYS_SETHOSTNAME
	case 170:
		/* sethostname */
		ret = sys_sethostname((char *)s->rdi, s->rsi);
#endif

#ifndef DISABLE_SYS_GETTID
	case 186:
		/* gettid */
		ret = sys_getpid();
		break;
#endif /* DISABLE_SYS_GETTID */

#ifndef DISABLE_SYS_TKILL
	case 200:
		/* tkill */
		ret = sys_tkill(s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_TKILL */

#ifndef DISABLE_SYS_TIME
	case 201:
		/* time */
		ret = sys_time((long int *)s->rdi);
		break;
#endif /* DISABLE_SYS_TIME */

#ifndef DISABLE_SYS_FUTEX
	case 202:
		/* futex */
		ret = sys_futex((int *)s->rdi, s->rsi, s->rdx, (const struct timespec *)s->r10, (int *)s->r9, s->r8);
		break;
#endif /* DISABLE_SYS_FUXTEX */

#ifndef DISABLE_SYS_SCHED_SETAFFINITY
	case 203:
		ret = sys_sched_setaffinity(s->rdi, s->rsi, (long unsigned int *)s->rdx);
		break;
#endif /* DISABLE_SYS_SCHED_SETAFFINITY */

#ifndef DISABLE_SYS_SCHED_GETAFFINITY
	case 204:
		ret = sys_sched_getaffinity(s->rdi, s->rsi, (long unsigned int *)s->rdx);
		break;
#endif /* DISABLE_SYS_SCHED_GETAFFINITY */

#ifndef DISABLE_SYS_SET_TID_ADDRESS
	case 218:
		/* set_tid_address */
		/* TODO */
		ret = s->rdi;
		break;
#endif /* DISABLE_SYS_SET_TID_ADDRESS */

#ifndef DISABLE_SYS_CLOCK_GETTIME
	case 228:
		/* clock_gettime */
		ret = sys_clock_gettime(s->rdi, (struct timespec *)s->rsi);
		break;
#endif /* DISABLE_SYS_CLOCK_GETTIME */

#ifndef DISABLE_SYS_CLOCK_GETRES
	case 229:
		/* clock_getres */
		ret = sys_clock_getres(s->rdi, (struct timespec *)s->rsi);
		break;
#endif

#ifndef DISABLE_SYS_TGKILL
	case 234:
		/* tgkill */
		ret = sys_tgkill(s->rdi, s->rsi, s->rdx);
		break;
#endif /* DISABLE_SYS_TGKILL */

#ifndef DISABLE_SYS_OPENAT
	case 257:
		ret = sys_openat(s->rdi, (const char *)s->rsi, s->rdx, s->r10);
		break;
#endif /* DISABLE_SYS_OPENAT */

#ifndef DISABLE_SYS_EXIT_GROUP
	case 231:
		/* exit_group */
		sys_exit_group(s->rdi);
		LOG_ERROR("Should not reach here after exit_group ... \n");
		break;
#endif /* DISABLE_SYS_EXIT_GROUP */

#ifndef DISABLE_SYS_SET_ROBUST_LIST
	case 273:
		/* set_robust_list */
		ret = sys_set_robust_list((void *)s->rdi, s->rsi);
		break;
#endif /* DISABLE_SYS_SET_ROBUST_LIST */

#ifndef DISABLE_SYS_GET_ROBUST_LIST
	case 274:
		/* get_robust_list */
		ret = sys_get_robust_list(s->rdi, (void *)s->rsi, (size_t *)s->rdx);
		break;
#endif /* DISABLE_SYS_GET_ROBUST_LIST */

#ifndef DISABLE_SYS_PRLIMIT64
	case 302:
		/* prlimit64 */
		ret = sys_prlimit64(s->rdi, s->rsi, (struct rlimit *)s->rdx,
				    (struct rlimit *)s->r10);
		break;
#endif

	default:
		LOG_ERROR("Unsuported Linux syscall: %d\n", rax);
		sys_exit(-EFAULT);
	}

	return ret;
}
