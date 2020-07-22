#ifndef	_SNAPUTIL_H
#define	_SNAPUTIL_H

namespace SnapUtil {

std::vector<std::string> *charpp2vecstr(char **arg, unsigned total_args);
char **vecstr2charpp(std::vector<std::string> v, unsigned *total_args);

}
#endif
