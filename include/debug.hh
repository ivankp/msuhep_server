#ifndef IVANP_DEBUG_HH
#define IVANP_DEBUG_HH

#ifdef TEST
#error "TEST macro already defined"
#endif
#ifdef INFO
#error "INFO macro already defined"
#endif

#ifndef NDEBUG

#include <iostream>

#define IVANP_STR1(x) #x
#define IVANP_STR(x) IVANP_STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" IVANP_STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

#define INFO(color,...) ivanp::stream( \
  std::cout, "\033[" color "m", __VA_ARGS__, "\033[0m") << std::endl;

#else

#define TEST(var)
#define INFO(...)

#endif
#endif
