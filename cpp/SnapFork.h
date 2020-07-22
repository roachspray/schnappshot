#ifndef	_SNAPFORK_H
#define	_SNAPFORK_H

#include "SnapUtil.h"

struct SnapFork {
	std::string ex;
	std::vector<std::string> *args;	//
	unsigned nargs;
	hma_t breakaddr;
	
	SnapFork(char *_ex, char **_args, int na, hma_t ba) : ex(_ex),
	  nargs(na), breakaddr(ba) {
		args = SnapUtil::charpp2vecstr(_args, nargs);
	}
	~SnapFork() {
		delete args;	// XXX (arr): we're stil leaking
	}
	virtual void runChild() {
		if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
			std::perror("ptrace(TRACEME)");
			std::exit(EXIT_FAILURE);
		}
		if (personality(ADDR_NO_RANDOMIZE) == -1) {
			std::perror("personality(ADDR_NO_RANDOMIZE)");
			std::exit(EXIT_FAILURE);
		}
#ifndef	_SEE_CHILD_OUTPUT
		int fd = open("/dev/null", O_WRONLY);
		dup2(fd, 1);
		dup2(fd, 2);
		(void)close(fd);
#endif
		char **targs = SnapUtil::vecstr2charpp(*this->args, nullptr);
		if (execv(this->ex.c_str(), targs) == -1) {
			std::perror("child: execl (target exe)");
			std::exit(EXIT_FAILURE);
		}
	}
	virtual void runParent(SnapContext &sCtx) {
		std::exit(0);
	}
	bool launch(SnapContext & sc) {
		pid_t pid = fork();
		if (pid == 0) {
			this->runChild();
		} else {
			sc.setPID(pid);
			this->runParent(sc);
		}
		return true;
	}
};

class SnapForkDumper : public SnapFork {
public:
	using SnapFork::SnapFork;
	virtual void runParent(SnapContext &);
};
class SnapForkReplayer : public SnapFork {
public:
	using SnapFork::SnapFork;
	virtual void runParent(SnapContext &);
};
#endif
