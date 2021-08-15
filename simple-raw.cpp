#include "simple-raw.h"

#if defined(__linux__)
#  include <arpa/inet.h>
#  include <endian.h>
#  define ntohll(x) be64toh(x)
#  define htonll(x) htobe64(x)
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <arpa/inet.h>
#  include <sys/endian.h>
#  define ntohll(x) be64toh(x)
#  define htonll(x) htobe64(x)
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  define ntohll(x) betoh64(x)
#  define htonll(x) htobe64(x)
#elif defined(__APPLE__)
#  include <arpa/inet.h>
#elif defined(_WIN32) || defined(__CYGWIN__)
#define IS_LITTLE_ENDIAN (((struct { union { unsigned int x; unsigned char c; }}){1}).c)
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#ifdef IS_LITTLE_ENDIAN
#define BYTE_ORDER LITTLE_ENDIAN
#else
#define BYTE_ORDER BIG_ENDIAN
#endif
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define ntohs(x) _byteswap_ushort(x)
#    define htons(x) _byteswap_ushort(x)
#    define ntohl(x) _byteswap_ulong(x)
#    define htonl(x) _byteswap_ulong(x)
#    define ntohll(x) _byteswap_uint64(x)
#    define htonll(x) _byteswap_uint64(x)
#  else
#    define ntohs(x) (x)
#    define htons(x) (x)
#    define ntohl(x) (x)
#    define htonl(x) (x)
#    define ntohll(x) (x)
#    define htonll(x) (x)
#  endif
#else
#  warning "no 64-bits ntoh/hton byte operations"
#endif

#include <stdio.h>
#include <unistd.h>

[[nodiscard]] size_t SafeWrite(int fd, const char* ptr, size_t size) {
  size_t retval = 0;
  while (size > 0) {
    //std::cerr << "SafeWrite: size=" << size << " retval=" << retval << std::endl;
#ifdef WIN32
    auto bytes = write(fd, ptr, uint32_t(size));
#else
    auto bytes = write(fd, ptr, size);
#endif
    if (bytes == 0)
      break;
    if (bytes < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    ptr += bytes;
    size -= size_t(bytes);
    retval += size_t(bytes);
  }
  //std::cerr << "SafeWrite: size=" << size << " retval=" << retval << std::endl;
  return retval;
}

[[nodiscard]] std::pair<size_t,int> SafeRead(int fd, char* ptr, size_t size) {
  size_t retval = 0;
  while (size > 0) {
    errno = 0;
    //std::cerr << "SafeRead: size=" << size << " retval=" << retval << std::endl;
#ifdef WIN32
    auto bytes = read(fd, ptr, uint32_t(size));
#else
    auto bytes = read(fd, ptr, size);
#endif
    if (bytes == 0)
      break;
    if (bytes < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    ptr += bytes;
    size -= size_t(bytes);
    retval += size_t(bytes);
  }
  //std::cerr << "SafeRead: size=" << size << " retval=" << retval << std::endl;
  return {retval, errno};
}

bool WriteBinary(int out, uint32_t number) {
  number = htonl(number);
  return sizeof(number) == SafeWrite(out, reinterpret_cast<char*>(&number), sizeof(number));
}
bool WriteBinary(int out, uint64_t number) {
  number = htonll(number);
  return sizeof(number) == SafeWrite(out, reinterpret_cast<char*>(&number), sizeof(number));
}
bool WriteBinary(int out, const char* str, uint32_t length) {
  if (!WriteBinary(out, uint32_t(length)))
    return false;
  return length == SafeWrite(out, str, length);
}

uint32_t ReadBinary(int in, uint32_t defaultValue, bool& good) {
  uint32_t number = 0;
  auto [bytes,err] = SafeRead(in, reinterpret_cast<char*>(&number), sizeof(number));
  //std::cerr << "Read32: " << number << std::endl;
  return (good = (good && err == 0 && bytes == sizeof(number))) ? ntohl(number) : defaultValue;
}
uint64_t ReadBinary(int in, uint64_t defaultValue, bool& good) {
  uint64_t number = 0;
  auto [bytes,err] = SafeRead(in, reinterpret_cast<char*>(&number), sizeof(number));
  //std::cerr << "Read64 " << number << std::endl;
  return (good = (good && err == 0 && bytes == sizeof(number))) ? ntohll(number) : defaultValue;
}
std::string ReadBinary(int in, const std::string& defaultValue, bool& good) {
  uint32_t length = ReadBinary(in, uint32_t(0), good);
  //std::cerr << "ReadString " << length << std::endl;
  if (!good)
    return defaultValue;
  std::string retval (length, '\0');
  auto [bytes,err] = SafeRead(in, retval.data(), length);
  //std::cerr << "readBinary(String " << length << ") -> " << bytes << std::endl;
  return (good = (good && err == 0 && bytes == length)) ? retval : defaultValue;
}
