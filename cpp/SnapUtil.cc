#ifndef	_SNAPUTIL_H
#define	_SNAPUTIL_H

#include <vector>
#include <string>
#include <cassert>

namespace SnapUtil {

// ripped from stackoverflow:
//    questions/25652505/skip-1st-iteration-of-range-based-for-loops
template <typename T>
struct skip
{
	T& t;
	std::size_t n;
	skip(T& v, std::size_t s) : t(v), n(s) {}
	auto begin() -> decltype(std::begin(t)) {
		return std::next(std::begin(t), n);
	}
	auto end() -> decltype(std::end(t)) {
		return std::end(t);
	}
};


// total_args is the total number of args in this **
std::vector<std::string> *
charpp2vecstr(char **arg, unsigned total_args)
{
	std::vector<std::string> *varg = new std::vector<std::string>(arg,
	  arg + (total_args-1));
	return varg;
}

// returns number of args in 2nd argument, but it can be nullptr
char **
vecstr2charpp(std::vector<std::string> v, unsigned *total_args)
{
	std::vector<char *> *s = new std::vector<char *>();;
	unsigned ta = 0;

	for (auto &e : v) {
		s->push_back(&e[0]);
		ta++;
	}
	// XXX (arr) we should check if last is null already?
	s->push_back(nullptr);
	// XXX (arr) do we want to incr this?
	ta++;
	if (total_args != nullptr) {
		*total_args = ta;
	}
	// XXX (arr): Fix all this... we're leaking or doing something wrong.
	return s->data();
}

}
#endif
