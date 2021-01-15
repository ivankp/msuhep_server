#include "zlib.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define ZLIB_CONST
#include <zlib.h>

#include <algorithm>
#include <bit>

#include "scope_guard.hh"
#include "error.hh"
// #include "debug.hh"

namespace zlib {

const char* zerrmsg(int code) noexcept {
  switch (code) {
    case Z_STREAM_END   : return "Z_STREAM_END";
    case Z_NEED_DICT    : return "Z_NEED_DICT";
    case Z_ERRNO        : return "Z_ERRNO";
    case Z_STREAM_ERROR : return "Z_STREAM_ERROR";
    case Z_DATA_ERROR   : return "Z_DATA_ERROR";
    case Z_MEM_ERROR    : return "Z_MEM_ERROR";
    case Z_BUF_ERROR    : return "Z_BUF_ERROR";
    case Z_VERSION_ERROR: return "Z_VERSION_ERROR";
    default             : return "Z_OK";
  }
}

std::vector<char> deflate_s(std::string_view str, bool gz) {
  std::vector<char> out;
  const size_t chunk = std::min(std::bit_ceil(str.size()/2),(size_t)1<<20);

  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;

  int ret;
  if ((ret = ::deflateInit2(
    &zs,
    Z_BEST_COMPRESSION,
    Z_DEFLATED,
    gz ? 15|16 : 15,
    8,
    Z_DEFAULT_STRATEGY
  )) != Z_OK) ERROR("deflateInit2(): ",zerrmsg(ret));

  scope_guard deflate_end([&]{ (void)::deflateEnd(&zs); });

  zs.next_in = reinterpret_cast<const unsigned char*>(str.data());
  zs.avail_in = str.size();
  do {
    out.resize(out.size()+chunk);
    zs.next_out = reinterpret_cast<unsigned char*>(
      out.data() + (out.size() - chunk) );
    zs.avail_out = chunk;
    while ((ret = ::deflate(&zs, Z_FINISH)) == Z_OK || ret == Z_BUF_ERROR) { }
    if (ret != Z_STREAM_END) ERROR("deflate(): ",zerrmsg(ret));
  } while (zs.avail_out == 0);
  if (zs.avail_in != 0) ERROR("uncompressed data remains");

  out.resize(out.size()-zs.avail_out);
  return out;
}

size_t deflate_f(int f1, int f2, size_t len1, bool gz) {
  if (!len1) {
    struct stat s1;
    PCALL(fstat)(f1, &s1);
    if (!S_ISREG(s1.st_mode)) ERROR("not a regular file");
    len1 = s1.st_size;
  }
  unsigned char* in = reinterpret_cast<unsigned char*>(
    ::mmap(0,len1,PROT_READ,MAP_SHARED,f1,0));
  if (in == MAP_FAILED) THROW_ERRNO("mmap()");
  scope_guard in_munmap([=]{ ::munmap(in,len1); });

  const size_t len2
    = std::max(std::min(std::bit_ceil(len1/2),(size_t)1<<20),(size_t)1<<10);
  unsigned char* out = reinterpret_cast<unsigned char*>(
    ::malloc(len2));
  scope_guard out_free([=]{ ::free(out); });

  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;

  if (const int ret = ::deflateInit2(
    &zs,
    Z_BEST_COMPRESSION,
    Z_DEFLATED,
    gz ? 15|16 : 15,
    8,
    Z_DEFAULT_STRATEGY
  ); ret != Z_OK) ERROR("deflateInit2(): ",zerrmsg(ret));

  scope_guard deflate_end([&]{ (void)::deflateEnd(&zs); });

  zs.next_in = in;
  zs.avail_in = len1;
  size_t out_len = 0;
  do {
    zs.next_out = out;
    zs.avail_out = len2;
    // while (::deflate(&zs, Z_FINISH) != Z_STREAM_END) { }
    // if (ret == Z_STREAM_ERROR) ERROR("deflate(): ",zerrmsg(ret));
    // TEST(zerrmsg(::deflate(&zs, Z_FINISH)))
    // TODO: make sure deflate() doesn't need to be called in a loop
    ::deflate(&zs, Z_FINISH);
    const ssize_t have = len2 - zs.avail_out;
    const ssize_t nbytes = ::write(f2, out, have);
    if (nbytes != have) {
      if (nbytes == -1) { THROW_ERRNO("write()"); }
      else { ERROR("wrote ",nbytes," bytes out of ",have); }
    }
    out_len += have;
  } while (zs.avail_out == 0);
  if (zs.avail_in != 0) ERROR("uncompressed data remains");

  return out_len;
}

} // end namespace zlib
