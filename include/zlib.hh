#ifndef IVANP_ZLIB_HH
#define IVANP_ZLIB_HH

#include <string_view>
#include <vector>

namespace zlib {

size_t deflate_f(int f1, int f2, size_t len1=0, bool gz=true);
std::vector<char> deflate_s(std::string_view str, bool gz=true);

}

#endif
