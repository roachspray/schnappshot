/*
 * ./dumload outdir 0xbreakpoint 0xendbreakpoint exe 
 *
 * dumps at a specific break point and then reloads from another to that saved
 * point.. endbreakpoint meant to be executed after breakpoint
 *
 */
#define	_POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/personality.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/uio.h>
#include <signal.h>
#include <sys/queue.h>

#include "helper.h"

char *ex;
char *outputdir;
hma_t breakaddr;
hma_t endbreakaddr;
unsigned LogLevel;


int
main(int argc, char **argv)
{
    pid_t child_pid = ~0;
	pid_t pid;

	xed_tables_init();
	srand(time(NULL));
	LogLevel = 1;
	outputdir = argv[1];
	{
		struct stat s;
		memset(&s, 0, sizeof(struct stat));
		if ((stat(outputdir, &s) == 0) ||
		  (errno != ENOENT)) {
			IPRNT("output directory '%s' exists...phooey.\n", outputdir);
			return 1;
		}
		if (mkdir(outputdir, 0770) == -1) {
			perror("mkdir: outputdir");
			return 1;
		}
		IPRNT("created directory for output: %s\n", outputdir);
	}
	breakaddr = 0;
#ifdef	__x86_64__
	breakaddr = strtoull(argv[2], NULL, 16);
#else
	breakaddr = strtoul(argv[2], NULL, 16);
#endif
	if (breakaddr == 0 && (errno == EINVAL || errno == ERANGE)) {
		IPRNT("invalid breakpoint address\n");
		return 0;
	}
	endbreakaddr = 0;
#ifdef	__x86_64__
	endbreakaddr = strtoull(argv[3], NULL, 16);
#else
	endbreakaddr = strtoul(argv[3], NULL, 16);
#endif
	if (endbreakaddr == 0 && (errno == EINVAL || errno == ERANGE)) {
		IPRNT("invalid breakpoint address\n");
		return 0;
	}
	ex = argv[4];
#ifdef	__x86_64__
	IPRNT("Breakpoint to be set at 0x%llx\n", breakaddr);
#else
	IPRNT("Breakpoint to be set at 0x%lx\n", breakaddr);
#endif

	child_pid = fork();
	pid = child_pid;
    if (child_pid == 0) {
		if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
			perror("ptrace (traceme)");
			return 1;
		}
		if (personality(ADDR_NO_RANDOMIZE) == -1) {
			perror("personality (aslr)");
			return 1;
		}

#ifndef	_SEE_CHILD_OUTPUT
		int fd = open("/dev/null", O_WRONLY); // XXX return value
		(void)dup2(fd, 1);
		(void)dup2(fd, 2);
		(void)close(fd);
#endif

		if (execl(ex, ex, NULL) == -1) {
			perror("execl");
			return 0;
		}
    } else {
		int k = -1;
		hma_t original_val;
		struct user_regs_struct mine;
		memset(&mine, 0, sizeof (struct user_regs_struct));
		while (1) {
			int status;
			struct user_regs_struct regs;

			k++;
			if (waitpid(pid, &status, 0) < 0) {
				perror("waitpid");
				// Doom
			}
			if (!WIFSTOPPED(status)) {
				IPRNT("!WIFSTOPPED\n");
				break;
			}
			if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
				perror("ptrace(GETREGS)");
				// Doom
			}

			if (!k) {
				DPRNT("Process start SIGTRAP breakpoint reached...\n");
				errno = 0;
				original_val = ptrace(PTRACE_PEEKTEXT, pid, breakaddr, 0);
				if (original_val == -1 && errno) {
					perror("ptrace(PTRACE_PEEKTEXT)");
				}
				hma_t wval = SETINT3(original_val);
				if (ptrace(PTRACE_POKETEXT, pid, breakaddr, wval) == -1) {
					perror("ptrace(PTRACE_POKETEXT)");
				}
				///
				errno = 0;
				hma_t endog = ptrace(PTRACE_PEEKTEXT, pid, endbreakaddr, 0);
				if (endog == -1 && errno) {
					perror("ptrace(PTRACE_PEEKTEXT)");
				}
				
				hma_t wval2 = SETINT3(endog);
				if (ptrace(PTRACE_POKETEXT, pid, endbreakaddr, wval2) == -1) {
					perror("ptrace(PTRACE_POKETEXT)");
				}
				if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
					perror("ptrace(CONT)");
				}
			} else if (k == 1) {
#ifdef	__x86_64__
				regs.rip -= 1;
				mine.rip -= 1;
				if (breakaddr != regs.rip) {
#else
				regs.eip -= 1;
				mine.eip -= 1;
				if (breakaddr != regs.eip) {
#endif
					IPRNT("Hit a trap, but it's not ours? Failing!\n");
					return 1;
				}
				DPRNT("SIGTRAP at desired break point!..removing\n");
				if (ptrace(PTRACE_POKETEXT, pid, breakaddr,
				  original_val) == -1) {
					perror("ptrace(PTRACE_POKETEXT)");
				}

				if (ptrace(PTRACE_GETREGS, pid, 0, &mine) < 0) {
					perror("ptrace(GETREGS)");
				}

				read_proc_mmap(pid);

				if (ptrace(PTRACE_SETREGS, pid, 0, &regs) == -1) {
					perror("ptrace(SETREGS)");
				}
				if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
					perror("ptrace(CONT)");
				}
			} else if (k >= 2) {
				if (k == 5) 	{
					IPRNT("Done...\n");
					return 1;
				}
#ifdef	__x86_64__
				if (endbreakaddr != regs.rip - 1) {
#else
				if (endbreakaddr != regs.eip - 1) {
#endif
					IPRNT("Hit a trap, but it's not ours? Failing!\n");
					return 1;
				}

				write_proc_mmap(pid);                                           

				if (ptrace(PTRACE_SETREGS, pid, 0, &mine) == -1) {
					perror("ptrace(SETREGS)");
				}
				IPRNT("P>.\nP>.\nP>.\n");
				if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
					perror("ptrace(CONT)");
				}
			}
		}
	}
	
	return 0;	
}
