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
#include <list>
#include <queue>
#include <vector>

#include <iostream>
#include <fstream>

#include <chrono>
#include <random>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cassert>

#include "SnapStuff.h"
#include "SnapDisassem.h"
#include "SnapUtil.h"
#include "SnapFork.h"



int
main(int argc, char **argv)
{
	char *outputdir = nullptr, *ex = nullptr, **args = nullptr;
	hma_t breakaddr = 0;
	bool dodump = true;

	LogLevel = 1;

	if (argc < 5) {
		std::cout << "usage: flambe -d <snapdir> <breakpt> <exe> [args..]\n"
		          << "       flambe -r <snapdir> <breakpt> <exec [args..]\n";
		std::exit(EXIT_FAILURE);
	}

	if (std::string(argv[1]) == "-d") {
		dodump = true;
	} else if (std::string(argv[1]) == "-r") {
		dodump = false;
	} else {
		std::runtime_error("Specify either -d (dump) or -r (replay)");
	}
	outputdir = argv[2];

	if (dodump == true) {
		// Fix this to use std::filesystem::exists(), etc
		struct stat s;
		std::memset(&s, 0, sizeof(struct stat));
		if ((stat(outputdir, &s) == 0) ||
		  (errno != ENOENT)) {
			std::perror("stat(outputdir)");
			std::cout << "Output directory already exists: " << outputdir <<
			  std::endl;
			std::exit(EXIT_FAILURE);
		}
		if (mkdir(outputdir, 0770) == -1) {
			std::perror("mkdir(outputdir)");
			std::cout << "Failed to mkdir(): " << outputdir << std::endl;
			std::exit(EXIT_FAILURE);
		}
	}
	IPRNT("Output directory prepared: %s\n", outputdir);

	SnapContext *snapCtx = new SnapContext(outputdir);
#ifdef	__x86_64__
	breakaddr = std::strtoull(argv[3], NULL, 16);
#else
	breakaddr = std::strtoul(argv[3], NULL, 16);
#endif
	if (breakaddr == 0 && (errno == EINVAL || errno == ERANGE)) {
		std::cout << "Invalid breakpoint address: " << argv[3] << std::endl;
		std::exit(EXIT_FAILURE);
	}
	ex = argv[4];
	args = argv + 4;
#ifdef	__x86_64__
	IPRNT("Breakpoint to be set at 0x%llx\n", breakaddr);
#else
	IPRNT("Breakpoint to be set at 0x%lx\n", breakaddr);
#endif
	DPRNT("Using executable: %s\n", ex);

//	SnapFork snapFork(ex, args, argc-3, breakaddr);
	if (dodump) {
		std::cout << "Dumper launching\n";
		SnapForkDumper snapFork(ex, args, argc-3, breakaddr);
		snapFork.launch(*snapCtx);
	} else {
		std::cout << "Replayer launching\n";
		SnapForkReplayer snapFork(ex, args, argc-3, breakaddr);
		snapFork.launch(*snapCtx);
	}
	return 0;
}
