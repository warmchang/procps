#ifndef SYSINFO_H
#define SYSINFO_H

extern unsigned long Hertz;   /* clock tick frequency */
extern long smp_num_cpus;     /* number of CPUs */

#define JT double
extern void four_cpu_numbers(JT *uret, JT *nret, JT *sret, JT *iret);
#undef JT

extern int        uptime (double *uptime_secs, double *idle_secs);
extern void       loadavg(double *av1, double *av5, double *av15);


/* obsolete */
extern unsigned kb_main_shared;
/* old but still kicking -- the important stuff */
extern unsigned kb_main_buffers;
extern unsigned kb_main_cached;
extern unsigned kb_main_free;
extern unsigned kb_main_total;
extern unsigned kb_swap_free;
extern unsigned kb_swap_total;
/* recently introduced */
extern unsigned kb_high_free;
extern unsigned kb_high_total;
extern unsigned kb_low_free;
extern unsigned kb_low_total;
/* 2.4.xx era */
extern unsigned kb_active;
extern unsigned kb_inact_dirty;
extern unsigned kb_inact_clean;
extern unsigned kb_inact_target;
extern unsigned kb_swap_cached;  /* late 2.4 only */
/* derived values */
extern unsigned kb_swap_used;
extern unsigned kb_main_used;

extern void meminfo(void);

#endif /* SYSINFO_H */
