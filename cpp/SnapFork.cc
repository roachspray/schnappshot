extern "C" {                                                                    
#include <sys/wait.h>                                                           
#include <sys/ptrace.h>                                                         
#include <sys/user.h>                                                           
#include <sys/personality.h>                                                    
#include <sys/types.h>                                                          
#include <sys/stat.h>                                                           
#include <unistd.h>                                                             
}                                                                               
                                                                                
#include <memory>                                                               
#include <vector>                                                               
#include <list>                                                                 
#include <queue>                                                                
                                                                                
#include <cstdio>                                                               
#include <cstring>                                                              
#include <ctime>                                                                
#include <iostream>                                                             
#include <fstream>                                                              
#include <chrono>                                                               
#include <random>                                                               
#include <cassert>                                                              
#include <sstream>                                                              
                                                                                
#include "SnapStuff.h"                                                          
#include "SnapDisassem.h"                                                       
#include "SnapUtil.h"                                                           
#include "SnapFork.h"                        

void
SnapForkDumper::runParent(SnapContext &sCtx)
{
	pid_t pid = sCtx.getPID();
	hma_t original_val;
	int k = -1;

	while (1) {
		int status;
		struct user_regs_struct regs;

		k++;
		if (waitpid(pid, &status, 0) < 0) {
			std::perror("waitpid");
			kill(pid, SIGKILL);
			break;
		}
		if (!WIFSTOPPED(status)) {
			IPRNT("why'd we stop :((\n");
			break;
		}
		if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
			std::perror("ptrace(GETREGS)");
			kill(pid, SIGKILL);
			break;
		}

		if (!k) {

			errno = 0;
			original_val = ptrace(PTRACE_PEEKTEXT, pid, this->breakaddr, 0);
			if (original_val == -1 && errno != 0) {
				std::perror("original_val = ptrace(PTRACE_PEEKTEXT)");
				kill(pid, SIGKILL);
				break;
			}

			SnapDisassem::printDecoded(pid, this->breakaddr);
			hma_t wval = SETINT3(original_val);
			if (ptrace(PTRACE_POKETEXT, pid, this->breakaddr, wval) == -1) {
				std::perror("ptrace(PTRACE_POKETEXT)");
				kill(pid, SIGKILL);
				break;
			}
			if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
				std::perror("ptrace(CONT)");
				kill(pid, SIGKILL);
				break;
				// Should doom.
			}
		} else {
#ifdef	__x86_64__
			regs.rip -= 1;
			if (this->breakaddr != regs.rip) {
#else
			regs.eip -= 1;
			if (this->breakaddr != regs.eip) {
#endif
				kill(pid, SIGKILL);
				return;/// false;
			}
			if (ptrace(PTRACE_POKETEXT, pid, this->breakaddr,
			  original_val) == -1) {
				std::perror("ptrace(PTRACE_POKETEXT)");
			}

			sCtx.readProcMaps();
			sCtx.setRegs(&regs);
			sCtx.writeSnapToDisk();

			if (1) {
				IPRNT("Wrote snapshot to disk and exiting\n");
				kill(pid, SIGKILL);
				std::exit(0);
			} else {
                IPRNT("Wrote snapshot to disk and continuing child process\n");
			}

			if (ptrace(PTRACE_SETREGS, pid, 0, &regs) == -1) {
				std::perror("ptrace/SETREGS");
				kill(pid, SIGKILL);
				break;
			}
			if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
				std::perror("ptrace(CONT)");
				kill(pid, SIGKILL);
				break;
			}
		}
	}
	return ;
}


void
SnapForkReplayer::runParent(SnapContext &sCtx)
{
	pid_t pid = sCtx.getPID();
	int k = -1;

	while (1) {
		int status;
		struct user_regs_struct regs;

		k++;

		if (waitpid(pid, &status, 0) == -1) {
			std::perror("waitpid");
			kill(pid, SIGKILL);
			break;
		}
		if (!WIFSTOPPED(status)) {
			IPRNT("!WIFSTOPPED\n");
			break;
		}
            
		if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
			std::perror("ptrace(GETREGS)");
			kill(pid, SIGKILL);
			break;
		}

		if (!k) {
			errno = 0;
			hma_t endog = ptrace(PTRACE_PEEKTEXT, pid, this->breakaddr, 0);
			if (endog == -1 && errno != 0) {
				std::perror("ptrace(PTRACE_PEEKTEXT)");
				kill(pid, SIGKILL);
				break;
			}

			SnapDisassem::printDecoded(pid, this->breakaddr);
			hma_t wval2 = SETINT3(endog);
			if (ptrace(PTRACE_POKETEXT, pid, this->breakaddr, wval2) == -1) {
				std::perror("ptrace(PTRACE_POKETEXT)");
				kill(pid, SIGKILL);
				break;
			}

			IPRNT("P> Reading all from disk..\n");
			sCtx.readSnapFromDisk();

			if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
				std::perror("ptrace(CONT)");
				kill(pid, SIGKILL);
				break;
			}
		} else if (k) {
#ifdef	__x86_64__
			regs.rip -= 1;
#else
			regs.eip -= 1;
#endif
			// xxxchangeme
			if (k == 5) {
				IPRNT("P> finished replay loop\n");
				kill(pid, SIGKILL);
				break;
		
			}
			if (this->breakaddr != regs.rip) {
				IPRNT("P> broke but not on our bp\n");
				SnapDisassem::printDecoded(pid, regs.rip);
				kill(pid, SIGKILL);
				break;
			}

			sCtx.writeProcMaps();

			if (ptrace(PTRACE_SETREGS, pid, 0, &sCtx.regs) == -1) {
				std::perror("ptrace/SETREGS");
				kill(pid, SIGKILL);
				break;
			}
			IPRNT("P>.\nP>.\nP>.\n");
			if (ptrace(PTRACE_CONT, pid, 0, 0) == -1) {
				std::perror("ptrace(CONT)");
				kill(pid, SIGKILL);
				break;
			}
		}
	}
	return;
}
