#include "file_cache.hh"

#include <cstdlib>
#include <map>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <sys/mman.h>
#include <sched.h>

#include "scope_fd.hh"
#include "error.hh"

namespace ivanp {
namespace {

std::map<std::string,cached_file> files;
std::shared_mutex mx_file_cache;

}

cached_file::~cached_file() { free(data); }

locked_cache_view::~locked_cache_view() {
  if (!view.empty()) mx_file_cache.unlock_shared();
}

std::optional<locked_cache_view>
file_cache(const char* name, size_t max_size) {
  const scope_fd fd = PCALLR(open)(name, O_RDONLY);
  struct stat sb;
  PCALL(fstat)(fd, &sb);
  if (!S_ISREG(sb.st_mode)) ERROR(name," is not a regular file");

  if (sb.st_size == 0)
    return std::optional<locked_cache_view>(std::in_place);

retry_cached:
  mx_file_cache.lock_shared();
  auto& f = files[name];
  if (f.data && sb.st_mtime < f.time)
    goto return_cached;
  mx_file_cache.unlock_shared();

  if (max_size == 0) max_size = file_cache_default_max_size;
  if ((size_t)sb.st_size > max_size) return std::nullopt;

  if (!mx_file_cache.try_lock()) {
    ::sched_yield();
    goto retry_cached;
  }

  f.data = reinterpret_cast<char*>( malloc(f.size = sb.st_size) );
  PCALL(read)(fd, f.data, f.size);
  f.time = ::time(0);
  // TODO: correctly downgrade unique to shared lock
  mx_file_cache.unlock();
  mx_file_cache.lock_shared();

return_cached:
  return std::optional<locked_cache_view>( std::in_place, f.data, f.size );
}

}
