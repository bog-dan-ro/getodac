// x86_64-signal.h - Catch runtime signals and turn them into exceptions
// on an x86_64 based GNU/Linux system.

/* Copyright (C) 2003, 2006, 2007, 2012  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */


#ifdef __x86_64__

#ifndef JAVA_SIGNAL_H
#define JAVA_SIGNAL_H 1

#include <signal.h>
#include <sys/syscall.h>

#define HANDLE_SEGV 1
#define HANDLE_FPE 1
#define HANDLE_PIPE 1

#define SIGNAL_HANDLER(_name)					\
static void _Jv_##_name (int, siginfo_t *,			\
			 void *_p __attribute__ ((__unused__)))

extern "C" 
{
  struct kernel_sigaction 
  {
    void (*k_sa_sigaction)(int,siginfo_t *,void *);
    unsigned long k_sa_flags;
    void (*k_sa_restorer) (void);
    sigset_t k_sa_mask;
  };
}

#define MAKE_THROW_FRAME(_exception)

#define RESTORE(name, syscall) RESTORE2 (name, syscall)
#define RESTORE2(name, syscall)			\
asm						\
  (						\
   ".text\n"					\
   ".byte 0  # Yes, this really is necessary\n" \
   ".align 16\n"				\
   "__" #name ":\n"				\
   "	movq $" #syscall ", %rax\n"		\
   "	syscall\n"				\
   );

/* The return code for realtime-signals.  */
RESTORE (restore_rt, __NR_rt_sigreturn)
void restore_rt (void) asm ("__restore_rt")
  __attribute__ ((visibility ("hidden")));

#define INIT_SEGV						\
do								\
  {								\
    struct kernel_sigaction act;				\
    act.k_sa_sigaction = _Jv_catch_segv;			\
    sigemptyset (&act.k_sa_mask);				\
    act.k_sa_flags = SA_SIGINFO|0x4000000;			\
    act.k_sa_restorer = restore_rt;				\
    syscall (SYS_rt_sigaction, SIGSEGV, &act, NULL, _NSIG / 8);	\
  }								\
while (0)  

#define INIT_FPE						\
do								\
  {								\
    struct kernel_sigaction act;				\
    act.k_sa_sigaction = _Jv_catch_fpe;				\
    sigemptyset (&act.k_sa_mask);				\
    act.k_sa_flags = SA_SIGINFO|0x4000000;			\
    act.k_sa_restorer = restore_rt;				\
    syscall (SYS_rt_sigaction, SIGFPE, &act, NULL, _NSIG / 8);	\
  }								\
while (0)  

/* You might wonder why we use syscall(SYS_sigaction) in INIT_FPE
 * instead of the standard sigaction().  This is necessary because of
 * the shenanigans above where we increment the PC saved in the
 * context and then return.  This trick will only work when we are
 * called _directly_ by the kernel, because linuxthreads wraps signal
 * handlers and its wrappers do not copy the sigcontext struct back
 * when returning from a signal handler.  If we return from our divide
 * handler to a linuxthreads wrapper, we will lose the PC adjustment
 * we made and return to the faulting instruction again.  Using
 * syscall(SYS_sigaction) causes our handler to be called directly
 * by the kernel, bypassing any wrappers.  */

#endif /* JAVA_SIGNAL_H */

#else /* __x86_64__ */
/* This is for the 32-bit subsystem on x86-64.  */

#define sigcontext_struct sigcontext
#include <java-signal-aux.h>

#endif /* __x86_64__ */
