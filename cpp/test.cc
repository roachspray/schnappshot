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
#include <stdexcept>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <cassert>
#include <sstream>
#include <functional> // std::function

#include "SnapStuff.h"
#include "SnapDisassem.h"
#include "SnapUtil.h"
#include "SnapFork.h"

namespace SnapUtil {
namespace Test {
bool
test_charpp2vecstr_00()
{
	// maybe NULL entry at end
	char *args[] = {
		 "flummox",
		 "tomfoolerly",
		 "bomba"
	};
	unsigned total_args = 3;

	std::cout << "test_charpp2vecstr_00 part 1" << std::endl;

	auto res00 = charpp2vecstr(args, total_args);
	unsigned int i = 0;
	for (auto s : *res00) { 
		 if (s.compare(args[i++]) == 0) {
			  continue;
		 }
		 return false;
	}
	std::cout << "test_charpp2vecstr_00 part 2" << std::endl;
	auto res01 = charpp2vecstr(args+1, total_args-1);
	i = 1;
	for (auto s : *res01) { 
		 if (s.compare(args[i++]) == 0) {
			  continue;
		 }
		 return false;
	}
	return true;
}
bool
test_vecstr2charpp_00()
{
	std::vector<std::string> v = { "Chump", "Mump", "Crump" };
	unsigned total_args = 0;
	std::cout << "test_vecstr2charpp_00" << std::endl;
	char **s = vecstr2charpp(v, &total_args); 
	assert(total_args == 3 && "total_args != 3");
	for (unsigned i = 0; i < total_args; i++) {
		 if (s[i] == NULL) {
			  break;
		 }	
		 std::string sq(s[i]);
		 if (sq.compare(v.at(i)) == 0) {
			  continue;
		 }
		 return false;
	}
	return true;
}
std::vector<std::function<bool()>> testcases = {
	test_charpp2vecstr_00,
	test_vecstr2charpp_00
};
}
}

int
main(int argc, char **argv)
{
	std::cout << "Starting SnapUtil test cases" << std::endl;
	for (auto &c : SnapUtil::Test::testcases) {
		if (c() == false) {
			std::cout << "FALSE" << std::endl << std::endl;
			throw std::runtime_error("failed testcase");
		}
		std::cout << "TRUE" << std::endl;
	}
	std::cout << "Passed SnapUtil test cases" << std::endl;
	return EXIT_SUCCESS;
}
