#ifndef	_HELPER_H
#define	_HELPER_H

//xxx
#ifdef	__x86_64__
typedef	unsigned long long	hma_t;
#define	SETINT3(a)	((((a) & 0xFFFFFFFFFFFFFF00)) | 0x00000000000000CC)
#else
typedef	unsigned long	hma_t;
#define SETINT3(a)	((((a) & 0xFFFFFF00)) | 0x000000CC)
#endif

struct maps_entry {
	hma_t me_from;
	hma_t me_to;
	char me_flags[5];
	hma_t me_offset;
	unsigned me_major;
	unsigned me_minor;
	hma_t me_inode;
	char me_name[256];
	unsigned char *me_data;
	LIST_ENTRY(maps_entry) me_entries;
};

void read_proc_mmap(pid_t pid);
void write_proc_mmap(pid_t pid);
void read_all_disk(struct user_regs_struct *regs);
void write_all_disk(struct user_regs_struct *regs);
void print_snap_mem();

#ifdef  _WITH_XED
#include <xed/xed-decoded-inst.h>
#include <xed/xed-decoded-inst-api.h>
#include <xed/xed-error-enum.h>
#include <xed/xed-init.h>
#include <xed/xed-types.h>
#include <xed/xed-interface.h>
#include <xed/xed-decode.h>
#include <xed/xed-machine-mode-enum.h>

void do_print_instr(pid_t pid, hma_t ip);

#else
#define	do_print_instr(a, b)	
#define	xed_tables_init()	
#endif // _WITH_XED

extern unsigned LogLevel;

#define DPRNT(...) do {   \
	if (LogLevel >= 2) printf(__VA_ARGS__);  \
} while (0)

#define IPRNT(...) do {  \
	if (LogLevel >= 1) printf(__VA_ARGS__);   \
} while (0)

#define APRNT(...) do {  \
	if (LogLevel >= 0) printf(__VA_ARGS__);   \
} while (0)
#endif 
