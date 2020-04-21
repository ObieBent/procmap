/*
 * convenient.h
 * A few convenience macros and routines..
 * Mostly for kernel-space usage, some for user-space as well.
 */
#ifndef __CONVENIENT_H__
#define __CONVENIENT_H__

#include <asm/param.h>		/* HZ */
#include <linux/sched.h>

#ifdef __KERNEL__
#include <linux/ratelimit.h>

/* 
   *** PLEASE READ this first ***

    We can reduce the load, and increase readability, by using the trace_printk
    instead of printk. To see the trace_printk() output do:
       cat /sys/kernel/debug/tracing/trace

    If we insist on using the regular printk, lets at least rate-limit it.
	For the programmers' convenience, this too is programatically controlled 
	(by an integer var USE_RATELIMITING [default: On]).

 	*** Kernel module authors Note: ***
	To use the trace_printk(), pl #define the symbol USE_FTRACE_PRINT in your
	Makefile:
	 EXTRA_CFLAGS += -DUSE_FTRACE_PRINT
	If you do not do this, we will use the usual printk() .

	To view :
	  printk's       : dmesg
      trace_printk's : cat /sys/kernel/debug/tracing/trace

	 Default: printk (with rate-limiting)
 */
/* Keep this defined to use the FTRACE-style trace_printk(), else will use
   regular printk() */
//#define USE_FTRACE_BUFFER
#undef USE_FTRACE_BUFFER

#ifdef USE_FTRACE_BUFFER
#define DBGPRINT(string, args...)                                       \
     trace_printk(string, ##args);
#else
#define DBGPRINT(string, args...) do {                                  \
     int USE_RATELIMITING=0;                                            \
	 if (USE_RATELIMITING) {                                            \
	   pr_info_ratelimited(pr_fmt(string), ##args);                     \
	 }                                                                  \
	 else                                                               \
           pr_info(pr_fmt(string), ##args);                             \
} while (0)
#endif
#endif				/* #ifdef __KERNEL__ */

/*------------------------ MSG, QP ------------------------------------*/
#ifdef DEBUG
#ifdef __KERNEL__
#define MSG(string, args...) do {                                       \
	DBGPRINT("%s:%d : " string, __FUNCTION__, __LINE__, ##args);        \
} while (0)
#else
#define MSG(string, args...) do {                                       \
	fprintf(stderr, "%s:%d : " string, __FUNCTION__, __LINE__, ##args); \
} while (0)
#endif

#ifdef __KERNEL__
#define MSG_SHORT(string, args...) do {                                 \
	DBGPRINT(string, ##args);                                           \
} while (0)
#else
#define MSG_SHORT(string, args...) do {                                 \
	fprintf(stderr, string, ##args);                                    \
} while (0)
#endif

// QP = Quick Print
#define QP MSG("\n")

#ifdef __KERNEL__
#ifndef USE_FTRACE_BUFFER
#define QPDS do {                                                       \
	MSG("\n");                                                          \
	dump_stack();                                                       \
} while(0)
#else
#define QPDS do {                                                       \
	MSG("\n");                                                          \
	trace_dump_stack();                                                 \
} while(0)
#endif
#endif

#ifdef __KERNEL__
#define HexDump(from_addr, len) \
 	    print_hex_dump_bytes (" ", DUMP_PREFIX_ADDRESS, from_addr, len);
#endif
#else				/* #ifdef DEBUG */
#define MSG(string, args...)
#define MSG_SHORT(string, args...)
#define QP
#define QPDS
#endif

/* SHOW_DELTA_*(low, hi) :
 * Show the low val, high val and the delta (hi-low) in either bytes/KB/MB/GB,
 * as required.
 * Inspired from raspberry pi kernel src: arch/arm/mm/init.c:MLM()
 */
#define SHOW_DELTA_b(low, hi) (low), (hi), ((hi) - (low))
#define SHOW_DELTA_K(low, hi) (low), (hi), (((hi) - (low)) >> 10)
#define SHOW_DELTA_M(low, hi) (low), (hi), (((hi) - (low)) >> 20)
#define SHOW_DELTA_G(low, hi) (low), (hi), (((hi) - (low)) >> 30)
#define SHOW_DELTA_MG(low, hi) (low), (hi), (((hi) - (low)) >> 20), (((hi) - (low)) >> 30)

#ifdef __KERNEL__
/*------------------------ PRINT_CTX ---------------------------------*/
/* 
 An interesting way to print the context info:
 If USE_FTRACE_BUFFER is On, it implies we'll use trace_printk(), else the vanilla
 printk() (see above).
 If we are using trace_printk(), we will automatically get output in the ftrace 
 latency format (see below):

 * The Ftrace 'latency-format' :
                       _-----=> irqs-off          [d]
                      / _----=> need-resched      [N]
                     | / _---=> hardirq/softirq   [H|h|s]   H=>both h && s
                     || / _--=> preempt-depth     [#]
                     ||| /                      
 CPU  TASK/PID       ||||  DURATION                  FUNCTION CALLS 
 |     |    |        ||||   |   |                     |   |   |   | 

 However, if we're _not_ using ftrace trace_printk(), then we'll _emulate_ the same
 with the printk() !
 (of course, without the 'Duration' and 'Function Calls' fields).
 */
#include <linux/sched.h>
#include <linux/interrupt.h>

#ifndef USE_FTRACE_BUFFER	// 'normal' printk(), lets emulate ftrace latency format
#define PRINT_CTX() do {                                                                 \
	char sep='|', intr='.';                                                              \
	                                                                                     \
   if (in_interrupt()) {                                                                 \
      if (in_irq() && in_softirq())                                                      \
	    intr='H';                                                                        \
	  else if (in_irq())                                                                 \
	    intr='h';                                                                        \
	  else if (in_softirq())                                                             \
	    intr='s';                                                                        \
	}                                                                                    \
   else                                                                                  \
	intr='.';                                                                            \
	                                                                                     \
	DBGPRINT(                                                                            \
	"%s(): [%03d]%c%s%c:%d   %c "                                                        \
	"%c%c%c%u "                                                                          \
	"\n"                                                                                 \
	, __func__, smp_processor_id(),                                                      \
    (!current->mm?'[':' '), current->comm, (!current->mm?']':' '), current->pid, sep,    \
	(irqs_disabled()?'d':'.'),                                                           \
	(need_resched()?'N':'.'),                                                            \
	intr,                                                                                \
	(preempt_count() && 0xff)                                                            \
	);                                                                                   \
} while (0)
#else				// using ftrace trace_prink() internally
#define PRINT_CTX() do {                                                                 \
	DBGPRINT("PRINT_CTX:: [cpu %02d]%s:%d\n", smp_processor_id(), __func__,              \
			current->pid);                                                               \
	if (!in_interrupt()) {                                                               \
  		DBGPRINT(" in process context:%c%s%c:%d\n",                                      \
		    (!current->mm?'[':' '), current->comm, (!current->mm?']':' '),               \
				current->pid);                                                           \
	} else {                                                                             \
        DBGPRINT(" in interrupt context: in_interrupt:%3s. in_irq:%3s. in_softirq:%3s. " \
		"in_serving_softirq:%3s. preempt_count=0x%x\n",                                  \
          (in_interrupt()?"yes":"no"), (in_irq()?"yes":"no"), (in_softirq()?"yes":"no"), \
          (in_serving_softirq()?"yes":"no"), (preempt_count() && 0xff));                 \
	}                                                                                    \
} while (0)
#endif
#endif

/*------------------------ assert ---------------------------------------
 * Hey, careful!
 * Using assertions is great *but* be aware of traps & pitfalls:
 * http://blog.regehr.org/archives/1096
 *
 * The closest equivalent perhaps, to assert() in the kernel are the BUG() 
 * or BUG_ON() and WARN() or WARN_ON() macros. Using BUG*() is _only_ for those
 * cases where recovery is impossible. WARN*() is usally considered a better
 * option. Pl see <asm-generic/bug.h> for details.
 *
 * Here, we just trivially emit a noisy [trace_]printk() to "warn" the dev/user.
 */
#ifdef __KERNEL__
#define assert(expr) do {                                               \
 if (!(expr)) {                                                         \
  pr_warn("********** Assertion [%s] failed! : %s:%s:%d **********\n",  \
   #expr, __FILE__, __func__, __LINE__);                                \
 }                                                                      \
} while(0)
#endif

/*------------------------ DELAY_LOOP --------------------------------*/
static inline void beep(int what)
{
#ifdef __KERNEL__
	pr_info("%c", (char)what);
#else
#include <unistd.h>
	char buf=(char)what;
	(void)write(STDOUT_FILENO, &buf, 1);
#endif
}

/* 
 * DELAY_LOOP macro
 * (Mostly) mindlessly loop, then print a char (via our beep() routine,
 * to emulate 'work' :-)
 * @val        : ASCII value to print
 * @loop_count : times to loop around
 */
#define DELAY_LOOP(val,loop_count)                                         \
{                                                                          \
	int c = 0, m;                                                          \
	unsigned int for_index, inner_index, x;                                \
	                                                                       \
	for (for_index=0;for_index<loop_count;for_index++) {                   \
		beep((val));                                                       \
		c++;                                                               \
		for (inner_index=0;inner_index<HZ;inner_index++) {                 \
			for(m=0;m<50;m++);                                             \
			x = inner_index /2;                                            \
		}                                                                  \
	}                                                                      \
	/*printf("c=%d\n",c);*/                                                \
}
/*------------------------------------------------------------------------*/

#ifdef __KERNEL__
/*------------ delay_sec --------------------------------------------------
 * Delays execution for @val seconds.
 * If @val is -1, we sleep forever!
 * MUST be called from process context.
 * (We deliberately do not inline this function; this way, we can see it's
 * entry within a kernel stack call trace).
 */
void delay_sec(long val)
{
	asm ("");    // force the compiler to not inline it!
	if (!in_interrupt()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (-1 == val)
			schedule_timeout(MAX_SCHEDULE_TIMEOUT);
		else
			schedule_timeout(val * HZ);
	}
}
#endif   /* #ifdef __KERNEL__ */

#endif   /* #ifndef __LLKD_CONVENIENT_H__ */
