/*
 * dummy example code for taking a snapshot of a process at a certain point
 * 
 * ./dumper outputdir 0xbreakpoint [no]cont exe argv1
 *
 * use `nocont` to exit after dumping at 0xbreakpoint and `cont` to fix-up and
 * continue
 *
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
int nocont = 1;
unsigned LogLevel;

int
main(int argc, char **argv)
{
	pid_t child_pid = ~0;
	pid_t pid;

	xed_tables_init();
	LogLevel = 1;

	srand(time(NULL));

	outputdir = argv[1];
	{
		struct stat s;
		memset(&s, 0, sizeof(struct stat));
		if ((stat(outputdir, &s) == 0) ||
		  (errno != ENOENT)) {
			printf("output directory '%s' exists...phooey.\n", outputdir);
			return 1;
		}
		if (mkdir(outputdir, 0770) == -1) {
			perror("mkdir: outputdir");
			return 1;
		}
		printf("Output setup: %s\n", outputdir);
	}
	breakaddr = 0;
#ifdef	__x86_64__
	breakaddr = strtoull(argv[2], NULL, 16);
#else
	breakaddr = strtoul(argv[2], NULL, 16);
#endif
	if (breakaddr == 0 && (errno == EINVAL || errno == ERANGE)) {
		printf("Invalid breakpoint address\n");
		return 1;
	}
	if (strncmp(argv[3], "cont", 4) == 0) {
		nocont = 0;
	}
	ex = argv[4];

#ifdef	__x86_64__
	printf("Breakpoint to be set at 0x%llx\n", breakaddr);
#else
	printf("Breakpoint to be set at 0x%lx\n", breakaddr);
#endif

	printf("Using executable: %s\n", ex);

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

		if (execl(ex, ex, argv[5], NULL) == -1) {
			perror("execl");
			return 0;
		}
    } else {
		int k = -1;
		hma_t original_val;

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
				break;
			}
			if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
				perror("ptrace/GETREGS");
				// Should doom.
				kill(pid, SIGKILL);
				break;
			}

			if (!k) {
#ifdef	__x86_64__
				printf("Child start SIGTRAP reached @ IP=0x%llx\n", regs.rip);
#else
				printf("Child start SIGTRAP reached @ IP=0x%lx\n", regs.eip);
#endif
				errno = 0;
				original_val = ptrace(PTRACE_PEEKTEXT, pid, breakaddr, 0);
				if (original_val == -1 && errno != 0) {
					perror("ptrace(PTRACE_PEEKTEXT)");
					// Should doom.
					kill(pid, SIGKILL);
					break;
				}

				do_print_instr(pid, breakaddr);
				hma_t wval = SETINT3(original_val);
				if (ptrace(PTRACE_POKETEXT, pid, breakaddr, wval) == -1) {
					perror("ptrace(PTRACE_POKETEXT)");
					kill(pid, SIGKILL);
					break;
					// Should doom.
				}
				if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
					perror("ptrace(CONT)");
					kill(pid, SIGKILL);
					break;
					// Should doom.
				}
			} else {
#ifdef	__x86_64__
				regs.rip -= 1;
				if (breakaddr != regs.rip) {
#else
				regs.eip -= 1;
				if (breakaddr != regs.eip) {
#endif
					printf("Hit a trap, but it's not ours? Failing!\n");
					kill(pid, SIGKILL);
					return 1;
				}
				if (ptrace(PTRACE_POKETEXT, pid, breakaddr,
				  original_val) == -1) {
					perror("ptrace(PTRACE_POKETEXT)");
				}

				read_proc_mmap(pid);
				write_all_disk(&regs);

				if (nocont) {
					printf("Wrote snapshot to disk and exiting\n");
					kill(pid, SIGKILL);
					exit(0);
				}
				printf("Wrote snapshot to disk and continuing child process\n");

				if (ptrace(PTRACE_SETREGS, pid, 0, &regs) == -1) {
					perror("ptrace/GETREGS");
					kill(pid, SIGKILL);
					break;
				}
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
