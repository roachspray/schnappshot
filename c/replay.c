/*
 * ./replay snapdir endbreak exe argv1
 *
 * run exe and break at endbreak and re-load process with info from datadir
 * ..mostly to test the written to disk data and that it works
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
#define	_GNU_SOURCE
#include <sys/uio.h>
#include <signal.h>
#include <sys/queue.h>

#include "helper.h"

char *ex;
char *outputdir;
hma_t endbreakaddr;
unsigned LogLevel;


int
main(int argc, char **argv)
{
	pid_t child_pid = ~0;
	pid_t pid;

	LogLevel = 1;
	xed_tables_init();
	outputdir = argv[1];
	{
		struct stat s;
		memset(&s, 0, sizeof(struct stat));
		if ((stat(outputdir, &s) == -1) ||
			(S_ISDIR(s.st_mode) == 0)) {
			IPRNT("output directory '%s' does not exist or is not a dir\n",
			  outputdir);
			return 1;
		}
	}
	IPRNT("Using snapshot from: %s\n", outputdir);

	endbreakaddr = 0;
#ifdef	__x86_64__
	endbreakaddr = strtoull(argv[2], NULL, 16);
#else
	endbreakaddr = strtoul(argv[2], NULL, 16);
#endif
	if (endbreakaddr == 0 && (errno == EINVAL || errno == ERANGE)) {
		IPRNT("invalide breakpoint address\n");
		return 0;
	}
	IPRNT("End breakpoint to be set at 0x%llx\n", endbreakaddr);

	ex = argv[3];

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
		int fd = open("/dev/null", O_WRONLY);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
#endif

		if (execv(ex, argv+4) == -1) {
			perror("execl");
			return 0;
		}
	} else {

		int k = -1;
		struct user_regs_struct snap_regs;

		memset(&snap_regs, 0, sizeof(struct user_regs_struct));

		while (1) {
			int status;
			struct user_regs_struct regs;
			
			k++;
			if (waitpid(pid, &status, 0) == -1) {
				perror("waitpid");
				kill(pid, SIGKILL);
				break;
			}
			if (!WIFSTOPPED(status)) {
				IPRNT("!WIFSTOPPED\n");
				break;
			}

			if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
				perror("ptrace(GETREGS)");
				kill(pid, SIGKILL);
				break;
			}

			if (!k) {
				errno = 0;
				hma_t endog = ptrace(PTRACE_PEEKTEXT, pid, endbreakaddr, 0);
				if (endog == -1 && errno != 0) {
					perror("ptrace(PTRACE_PEEKTEXT)");
					kill(pid, SIGKILL);
					break;
				}

				do_print_instr(pid, endbreakaddr);
				hma_t wval2 = SETINT3(endog);
				if (ptrace(PTRACE_POKETEXT, pid, endbreakaddr, wval2) == -1) {
					perror("ptrace(PTRACE_POKETEXT)");
					kill(pid, SIGKILL);
					break;
				}
				if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
					perror("ptrace(CONT)");
					kill(pid, SIGKILL);
					break;
				}

				// read off the data so we're ready to write
				IPRNT("P> Reading snap shot from disk\n");
				read_all_disk(&snap_regs);	

			} else if (k) {
				// only replay a few times since it's just example code
				if (k == 5) 	{
					IPRNT("P> ...we've replayed enough...\n");
					return 1;
				}
#ifdef	__x86_64__
				regs.rip -= 1;
#else
				regs.eip -= 1;
#endif
				if (endbreakaddr != regs.rip) {
					IPRNT("P> Broke, but not ours... doom.\n");
					do_print_instr(pid, regs.rip);
					kill(pid, SIGKILL);
					break;
				}
				DPRNT("P> SIGTRAP at desired break point!\n");
				write_proc_mmap(pid);
#ifdef	__x86_64__
				DPRNT("P> Setting IP to replay addr=0x%llx\n", regs.rip);
#else
				DPRNT("P> Setting IP to replay addr=0x%lx\n", regs.eip);
#endif
				if (ptrace(PTRACE_SETREGS, pid, 0, &snap_regs) == -1) {
					perror("ptrace(SETREGS)");
					kill(pid, SIGKILL);
					break;
				}
				IPRNT("P>.\nP>.\nP>.\n");
				if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
					perror("ptrace(CONT)");
					kill(pid, SIGKILL);
					break;
				}
			} 
		}
	}
	
	return 0;	
}
