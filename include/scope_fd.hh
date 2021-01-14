#ifndef IVANP_SCOPE_FILE_DESCRIPTOR_HH
#define IVANP_SCOPE_FILE_DESCRIPTOR_HH

class scope_fd {
  int fd = -1;

public:
  scope_fd() noexcept = default;
  scope_fd(int fd) noexcept: fd(fd) { }
  scope_fd(scope_fd&& o) noexcept: fd(o.fd) { o.fd = -1; }
  scope_fd& operator=(scope_fd&& o) noexcept {
    std::swap(fd,o.fd);
    return *this;
  }
  scope_fd(const scope_fd&) noexcept = delete;
  scope_fd& operator=(const scope_fd&) noexcept = delete;

  ~scope_fd() noexcept { ::close(fd); }

  bool operator==(int o) const noexcept { return fd == o; }
  bool operator!=(int o) const noexcept { return fd != o; }

  operator int() const noexcept { return fd; }
};

#endif
