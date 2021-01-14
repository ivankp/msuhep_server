#include "whole_file.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "scope_fd.hh"
#include "error.hh"

std::string whole_file(const char* filename) {
  const scope_fd fd = PCALLR(open)(filename, O_RDONLY);
  struct stat sb;
  PCALL(fstat)(fd, &sb);
  if (!S_ISREG(sb.st_mode)) ERROR("not a regular file");
  std::string m(sb.st_size,'\0');
  PCALL(read)(fd, m.data(), m.size());
  return m;
}
