#include "file_dispatcher.hh"

#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <shared_mutex>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "scope_fd.hh"
#include "error.hh"

namespace {

struct file {
  char* data = nullptr;
  unsigned size = 0;
  time_t t = 0;
  ~file() { free(data); }
};

std::map<std::string,file> files;
std::shared_mutex mx;

}

void dispatch_file(const char* filename, ivanp::socket sock) {
  const scope_fd fd = PCALLR(open)(filename, O_RDONLY);
  struct stat sb;
  PCALL(fstat)(fd, &sb);
  if (!S_ISREG(sb.st_mode)) ERROR(filename," is not a regular file");

  file f;
  { std::shared_lock lock(mx);
    f = files[filename];
  }

  if (f.data && sb.st_mtime < f.t) {
    sock.write(f.data,f.size);
  } else {
    char* m = reinterpret_cast<char*>(
      ::mmap(0,sb.st_size,PROT_READ,MAP_SHARED,fd,0) );
    if (m == MAP_FAILED) THROW_ERRNO("mmap()");

    sock.write(m,sb.st_size);

    if (sb.st_size < (1 << 20)) {
      time_t t = ::time(0);
      std::unique_lock lock(mx);
      if (t-f.t < 5/*sec*/) { // needed recently
        if (f.data) free(f.data);
        f.size = sb.st_size;
        f.data = reinterpret_cast<char*>(memcpy(malloc(f.size),m,f.size));
      }
      if (f.t < t) f.t = t;
      files[filename] = f;
    }
  }
}
