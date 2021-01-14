#ifndef IVANP_STRING_HH
#define IVANP_STRING_HH

#include <cstring>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace ivanp {

template <typename S, typename... T>
[[ gnu::always_inline ]]
inline S&& stream(S&& s, T&&... x) {
  (s << ... << std::forward<T>(x));
  return s;
}

inline std::string cat() noexcept { return { }; }
inline std::string cat(std::string x) noexcept { return x; }
inline std::string cat(const char* x) noexcept { return x; }

template <typename T>
[[ gnu::always_inline ]]
inline size_t str_size(const T& x) noexcept
requires requires { x.size(); } {
  return x.size();
}
template <typename T>
[[ gnu::always_inline ]]
inline size_t str_size(T x) noexcept
requires std::is_same_v<std::remove_cv_t<T>,char> {
  return 1;
}
template <typename T>
[[ gnu::always_inline ]]
inline size_t str_size(const T* x) noexcept
requires std::is_same_v<std::remove_cv_t<T>,char> {
  return strlen(x);
}

template <typename... T>
[[ gnu::always_inline ]]
inline std::string cat(T&&... x) {
  if constexpr ((... &&
    requires (std::string& s) {
      s += x;
      str_size(x);
    }
  )) {
    std::string s;
    s.reserve((... + str_size(x)));
    (s += ... += x);
    return s;
  } else {
    std::stringstream s;
    (s << ... << std::forward<T>(x));
    return std::move(s).str();
  }
}

// ------------------------------------------------------------------

inline const char* cstr(const char* x) noexcept { return x; }
inline char* cstr(char* x) noexcept { return x; }
inline const char* cstr(const std::string& x) noexcept { return x.data(); }
inline char* cstr(std::string& x) noexcept { return x.data(); }

// ------------------------------------------------------------------

struct chars_less {
  using is_transparent = void;
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
  template <typename T>
  bool operator()(const T& a, const char* b) const noexcept {
    return a < b;
  }
  template <typename T>
  bool operator()(const char* a, const T& b) const noexcept {
    return a < b;
  }
};

inline bool starts_with(const char* str, const char* prefix) noexcept {
  for (;; ++str, ++prefix) {
    const char c = *prefix;
    if (!c) break;
    if (*str != c) return false;
  }
  return true;
}

inline bool ends_with(std::string_view str, std::string_view suffix) noexcept {
  if (str.size() < suffix.size()) return false;
  return starts_with(str.data()+(str.size()-suffix.size()),suffix.data());
}
inline bool ends_with_any(std::string_view str, const auto&... suffix)
noexcept {
  return ( ends_with(str,suffix) || ... );
}

template <typename... T>
requires (... && std::is_same_v<T,char>) && (sizeof...(T) > 0)
[[gnu::always_inline]]
inline void ctrim(char*& s, const char* end, T... x) noexcept {
  for (; s<end; ++s) {
    const char c = *s;
    if (( ... && (c!=x) )) break;
  }
}

inline void replace_first(
  std::string& str, std::string_view s1, std::string_view s2
) {
  const auto pos = str.find(s1);
  if (pos != std::string::npos) str.replace(pos,s1.size(),s2);
}
inline void replace_all(
  std::string& str, std::string_view s1, std::string_view s2
) {
  for (size_t p; (p = str.find(s1,p)) != std::string::npos; ) {
    str.replace(p, s1.size(), s2);
    p += s2.size();
  }
}

} // end namespace ivanp

#endif
