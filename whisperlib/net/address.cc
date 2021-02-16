#include "whisperlib/net/address.h"

#include <arpa/inet.h>
#include <net/if.h>

namespace whisper {
namespace net {

IpAddress::IpAddress(uint32_t addr) {
  addr_[10] = 0xff;
  addr_[11] = 0xff;
  addr_[12] = (addr & 0xff000000) >> 24;
  addr_[13] = (addr & 0xff0000) >> 16;
  addr_[14] = (addr & 0xff00) >> 8;
  addr_[15] = addr & 0xff;
}

IpAddress::IpAddress(const uint8_t (&addr)[kIpV6Size]) {
  std::copy(std::begin(addr), std::end(addr), addr_.begin());
}

IpAddress::IpAddress(IpArray addr)
  : addr_(std::move(addr)) {
}

bool IpAddress::operator==(const IpAddress& other) const {
  return addr_ == other.addr_;
}
bool IpAddress::operator!=(const IpAddress& other) const {
  return addr_ != other.addr_;
}
bool IpAddress::operator<(const IpAddress& other) const {
  for (size_t i = 0; i < kIpV6Size; ++i) {
    if (addr_[i] < other.addr_[i]) {
      return true;
    } else if (addr_[i] > other.addr_[i]) {
      return false;
    }
  }
  return false;
}

bool IpAddress::is_ipv4() const {
  static const uint8_t kIpV4Prefix[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
  return 0 == memcmp(addr_.data(), kIpV4Prefix, sizeof(kIpV4Prefix));
}
bool IpAddress::is_ipv6() const {
  return !is_ipv4();
}

uint32_t IpAddress::ipv4() const {
  return ((uint32_t(addr_[12]) << 24)
          + (uint32_t(addr_[13]) << 16)
          + (uint32_t(addr_[14]) << 8)
          + (uint32_t(addr_[15])));
}
const IpAddress::IpArray& IpAddress::ipv6() const {
  return addr_;
}
in6_addr IpAddress::ipv6_addr() const {
  return *reinterpret_cast<const in6_addr*>(addr_.data());
}

absl::StatusOr<IpAddress> IpAddress::ParseFromString(absl::string_view ip) {
  if (ip.empty()) {
    return absl::InvalidArgumentError("Empty IP address string.");
  }
  std::string ip_str(ip);
  in_addr addr;
  if (inet_pton(AF_INET, ip_str.c_str(), &addr) == 1) {
    return IpAddress(::ntohl(addr.s_addr));
  }
  in6_addr addr6;
  if (inet_pton(AF_INET6, ip_str.c_str(), &addr6) == 1) {
    return IpAddress(addr6.s6_addr);
  }
  return absl::InvalidArgumentError("IP address string could not be parsed "
                                    "neither as IPv4, nor as IPv6.");
}

void IpAddress::ToSockAddr(sockaddr_storage* addr) const {
  if (is_ipv4()) {
    auto saddr = reinterpret_cast<struct sockaddr_in*>(addr);
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = ::htonl(ipv4());
  } else {
    auto saddr = reinterpret_cast<struct sockaddr_in6*>(addr);
    saddr->sin6_family = AF_INET6;
    memcpy(saddr->sin6_addr.s6_addr, addr_.data(), kIpV6Size);
  }
}

std::string IpAddress::ToString() const {
  if (is_ipv4()) {
    char str[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, static_cast<const void*>(addr_.data() + 12),
              str, sizeof(str));
    return std::string(str);
  } else {
    char str[INET6_ADDRSTRLEN] = {};
    inet_ntop(AF_INET6, static_cast<const void*>(addr_.data()),
              str, sizeof(str));
    return std::string(str);
  }
}

const IpAddress IpAddress::kIPv4Loopback = IpAddress(
    IpAddress::IpArray{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff,
      127, 0, 0, 1 });
const IpAddress IpAddress::kIPv6Loopback = IpAddress(
    IpAddress::IpArray{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 });

SockAddrSetter::SockAddrSetter() {
  addr_.ss_family = AF_INET;
}
const struct sockaddr* SockAddrSetter::addr() const {
  return reinterpret_cast<const struct sockaddr*>(&addr_);
}
struct sockaddr* SockAddrSetter::addr() {
  return reinterpret_cast<struct sockaddr*>(&addr_);
}
struct sockaddr_in* SockAddrSetter::sockaddr_in() {
  return reinterpret_cast<struct sockaddr_in*>(&addr_);
}
struct sockaddr_in6* SockAddrSetter::sockaddr_in6() {
  return reinterpret_cast<struct sockaddr_in6*>(&addr_);
}
SockAddrSetter& SockAddrSetter::SetIpAddress(const IpAddress& addr) {
  addr.ToSockAddr(&addr_);
  return *this;
}
SockAddrSetter& SockAddrSetter::SetIpFamily(bool is_ipv6) {
  if (is_ipv6) {
    addr_.ss_family = AF_INET6;
  } else {
    addr_.ss_family = AF_INET;
  }
  return *this;
}
SockAddrSetter& SockAddrSetter::SetPort(uint16_t port) {
  if (addr_.ss_family == AF_INET6) {
    sockaddr_in6()->sin6_port = ::htons(port);
  } else if (addr_.ss_family == AF_INET) {
    sockaddr_in()->sin_port = ::htons(port);
  }
  return *this;
}
SockAddrSetter& SockAddrSetter::SetUseAnyAddress(uint16_t port) {
  if (addr_.ss_family == AF_INET6) {
    sockaddr_in6()->sin6_addr = in6addr_any;
  } else if (addr_.ss_family == AF_INET) {
    sockaddr_in()->sin_addr.s_addr = INADDR_ANY;
  }
  return *this;
}
SockAddrSetter& SockAddrSetter::SetIpV6ScopeId(uint32_t scope_id) {
  if (addr_.ss_family == AF_INET6) {
    sockaddr_in6()->sin6_scope_id = ::htonl(scope_id);
  }
  return *this;
}

}  // namespace net
}  // namespace whisper
