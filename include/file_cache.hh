#ifndef IVANP_FILE_CACHE_HH
#define IVANP_FILE_CACHE_HH

#include <string_view>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <ctime>

namespace ivanp {

struct cached_file {
  char* data = nullptr;
  unsigned size = 0;
  time_t time = 0;
  ~cached_file();
};

class locked_cache_view {
  std::string_view view;
public:
  locked_cache_view(const char* data, size_t size) noexcept
  : view(data,size) { };
  ~locked_cache_view();
  locked_cache_view() noexcept = default;
  locked_cache_view(const locked_cache_view&) = delete;
  locked_cache_view& operator=(const locked_cache_view&) = delete;
  locked_cache_view(locked_cache_view&& o) noexcept: view(o.view) {
    o.view = { };
  }
  locked_cache_view& operator=(locked_cache_view&& o) noexcept {
    std::swap(view,o.view);
    return *this;
  }
  const char* data() const noexcept { return view.data(); }
  size_t size() const noexcept { return view.size(); }
  operator std::string_view() const noexcept { return view; }
};

inline size_t file_cache_default_max_size = 1 << 20;

std::optional<locked_cache_view>
file_cache(const char* name, size_t max_size = 0);

}

#endif
