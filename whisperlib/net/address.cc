#include "whisperlib/net/address.h"

#include <arpa/inet.h>
#include <net/if.h>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "whisperlib/status/status.h"

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

IpAddress::IpAddress(IpArray addr) : addr_(std::move(addr)) {}

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
  static const uint8_t kIpV4Prefix[] = {0, 0, 0, 0, 0,    0,
                                        0, 0, 0, 0, 0xff, 0xff};
  return 0 == memcmp(addr_.data(), kIpV4Prefix, sizeof(kIpV4Prefix));
}
bool IpAddress::is_ipv6() const { return !is_ipv4(); }

bool IpAddress::is_local_link() const {
  static const uint8_t kIpV4Prefix[] = {0, 0, 0, 0,    0,    0,   0,
                                        0, 0, 0, 0xff, 0xff, 169, 254};
  static const uint8_t kIpV6Prefix[] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0};
  return ((0 == memcmp(addr_.data(), kIpV4Prefix, sizeof(kIpV4Prefix))) ||
          (0 == memcmp(addr_.data(), kIpV6Prefix, sizeof(kIpV6Prefix))));
}

uint32_t IpAddress::ipv4() const {
  return ((uint32_t(addr_[12]) << 24) + (uint32_t(addr_[13]) << 16) +
          (uint32_t(addr_[14]) << 8) + (uint32_t(addr_[15])));
}
const IpAddress::IpArray& IpAddress::ipv6() const { return addr_; }
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
    return IpAddress(ntohl(addr.s_addr));
  }
  in6_addr addr6;
  if (inet_pton(AF_INET6, ip_str.c_str(), &addr6) == 1) {
    return IpAddress(addr6.s6_addr);
  }
  return absl::InvalidArgumentError(
      "IP address string could not be parsed "
      "neither as IPv4, nor as IPv6.");
}

absl::StatusOr<IpAddress> IpAddress::ParseFromSockAddr(const sockaddr* saddr,
                                                       size_t saddr_len) {
  if (saddr->sa_family == AF_INET) {
    RET_CHECK(saddr_len >= sizeof(sockaddr_in))
        << "Insufficient buffer size to parse IPv4 from sockaddr.";
    auto saddr_in = reinterpret_cast<const sockaddr_in*>(saddr);
    return IpAddress(saddr_in->sin_addr.s_addr);
  } else if (saddr->sa_family == AF_INET6) {
    RET_CHECK(saddr_len >= sizeof(sockaddr_in6))
        << "Insufficient buffer size to parse IPv4 from sockaddr.";
    auto saddr_in6 = reinterpret_cast<const sockaddr_in6*>(saddr);
    return IpAddress(saddr_in6->sin6_addr.s6_addr);
  } else {
    return absl::InvalidArgumentError(
        "Provided sockaddr structure does not have a AF_INET or AF_INET6 "
        "address family");
  }
}

void IpAddress::ToSockAddr(sockaddr_storage* addr) const {
  if (is_ipv4()) {
    auto saddr = reinterpret_cast<struct sockaddr_in*>(addr);
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = htonl(ipv4());
  } else {
    auto saddr = reinterpret_cast<struct sockaddr_in6*>(addr);
    saddr->sin6_family = AF_INET6;
    memcpy(saddr->sin6_addr.s6_addr, addr_.data(), kIpV6Size);
  }
}

std::string IpAddress::ToString() const {
  if (is_ipv4()) {
    char str[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, static_cast<const void*>(addr_.data() + 12), str,
              sizeof(str));
    return std::string(str);
  } else {
    char str[INET6_ADDRSTRLEN] = {};
    inet_ntop(AF_INET6, static_cast<const void*>(addr_.data()), str,
              sizeof(str));
    return std::string(str);
  }
}

const IpAddress IpAddress::kIPv4Localhost = IpAddress(
    IpAddress::IpArray{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 127, 0, 0, 1});
const IpAddress IpAddress::kIPv6Localhost = IpAddress(
    IpAddress::IpArray{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1});

SockAddrSetter::SockAddrSetter() { addr_.ss_family = AF_INET; }
SockAddrSetter::SockAddrSetter(const IpAddress& addr) { SetIpAddress(addr); }

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
    sockaddr_in6()->sin6_port = htons(port);
  } else if (addr_.ss_family == AF_INET) {
    sockaddr_in()->sin_port = htons(port);
  }
  return *this;
}
SockAddrSetter& SockAddrSetter::SetUseAnyAddress() {
  if (addr_.ss_family == AF_INET6) {
    sockaddr_in6()->sin6_addr = in6addr_any;
  } else if (addr_.ss_family == AF_INET) {
    sockaddr_in()->sin_addr.s_addr = INADDR_ANY;
  }
  return *this;
}
SockAddrSetter& SockAddrSetter::SetIpV6ScopeId(uint32_t scope_id) {
  if (addr_.ss_family == AF_INET6) {
    sockaddr_in6()->sin6_scope_id = htonl(scope_id);
  }
  return *this;
}

HostPort::HostPort(absl::optional<std::string> host,
                   absl::optional<IpAddress> ip, absl::optional<uint16_t> port)
    : host_(std::move(host)), ip_(std::move(ip)), port_(std::move(port)) {}

const absl::optional<std::string>& HostPort::host() const { return host_; }
const absl::optional<IpAddress>& HostPort::ip() const { return ip_; }
const absl::optional<uint16_t>& HostPort::port() const { return port_; }
const absl::optional<uint32_t>& HostPort::scope_id() const { return scope_id_; }

HostPort& HostPort::set_host(absl::string_view value) {
  host_ = std::string(value);
  return *this;
}
HostPort& HostPort::set_ip(IpAddress ip) {
  ip_ = std::move(ip);
  return *this;
}
HostPort& HostPort::set_port(uint16_t port) {
  port_ = port;
  return *this;
}
HostPort& HostPort::set_scope_id(uint32_t scope_id) {
  scope_id_ = scope_id;
  return *this;
}

bool HostPort::IsValid() const {
  return (port_.has_value() && (port_.value() != 0) &&
          (ip_.has_value() || host_.has_value()));
}
bool HostPort::IsResolved() const {
  return (port_.has_value() && (port_.value() != 0) && ip_.has_value());
}
void HostPort::Update(const HostPort& hp) {
  if (hp.host().has_value()) {
    set_host(hp.host().value());
  }
  if (hp.ip().has_value()) {
    set_ip(hp.ip().value());
  }
  if (hp.port().has_value()) {
    set_port(hp.port().value());
  }
  if (hp.scope_id().has_value()) {
    set_scope_id(hp.scope_id().value());
  }
}

std::string HostPort::ToString() const {
  std::string result;
  if (host_.has_value()) {
    absl::StrAppend(&result, host_.value());
  }
  if (ip_.has_value()) {
    const bool brackets = !result.empty() || ip_.value().is_ipv6();
    if (brackets) {
      absl::StrAppend(&result, "[");
    }
    absl::StrAppend(&result, ip_.value().ToString());
    if (brackets) {
      absl::StrAppend(&result, "]");
    }
  }
  if (port_.has_value()) {
    absl::StrAppend(&result, ":", port_.value());
  }
  if (result.empty()) {
    return "[]";
  }
  return result;
}

absl::StatusOr<std::string> HostPort::ToHostportString() const {
  std::string result;
  if (ip_.has_value()) {
    const bool brackets = ip_.value().is_ipv6();
    if (brackets) {
      absl::StrAppend(&result, "[");
    }
    absl::StrAppend(&result, ip_.value().ToString());
    if (brackets) {
      absl::StrAppend(&result, "]");
    }
  } else if (host_.has_value()) {
    absl::StrAppend(&result, host_.value());
  } else {
    return absl::FailedPreconditionError(
        "Host port has no host or ip specified.");
  }
  if (port_.has_value()) {
    absl::StrAppend(&result, ":", port_.value());
  } else {
    return absl::FailedPreconditionError("Host port has no port specified.");
  }
  return result;
}

absl::Status HostPort::ToSockAddr(sockaddr_storage* addr) const {
  if (!IsResolved()) {
    return status::FailedPreconditionErrorBuilder(
        "Host port is nor resolved yet for sockaddr conversion.");
  }
  ip_.value().ToSockAddr(addr);
  if (addr->ss_family == AF_INET6) {
    auto saddr = reinterpret_cast<struct sockaddr_in6*>(addr);
    saddr->sin6_port = htons(port_.value());
    if (scope_id_.has_value()) {
      saddr->sin6_scope_id = htonl(scope_id_.value());
    }
  } else if (addr->ss_family == AF_INET) {
    auto saddr = reinterpret_cast<struct sockaddr_in*>(addr);
    saddr->sin_port = htons(port_.value());
  }
  return absl::OkStatus();
}

absl::StatusOr<HostPort> HostPort::ParseFromString(
    absl::string_view host_port) {
  if (host_port.empty()) {
    return HostPort();
  }
  HostPort result;
  size_t pos = absl::string_view::npos;
  if (!absl::EndsWith(host_port, "]")) {
    pos = host_port.rfind(':');
  }
  absl::string_view host = host_port.substr(0, pos);
  absl::string_view ip_host = host;
  if (absl::StartsWith(host, "[") && absl::EndsWith(host, "]")) {
    ip_host = host.substr(1, host.size() - 2);
  }
  auto ip_result = IpAddress::ParseFromString(ip_host);
  if (ip_result.ok()) {
    result.set_ip(std::move(ip_result).value());
    if (result.ip().value().is_ipv6() && ip_host == host) {
      return status::InvalidArgumentErrorBuilder()
             << "An IPv6 host port needs to be in form [ip]:port for `"
             << host_port << "`";
    }
  } else {
    result.set_host(host);
  }
  if (pos != absl::string_view::npos) {
    uint32_t port;
    if (!absl::SimpleAtoi(host_port.substr(pos + 1), &port)) {
      return status::InvalidArgumentErrorBuilder()
             << "Error parsing hostport port from string: `" << host_port
             << "`";
    }
    if (port == 0 || port > 0xffff) {
      return status::InvalidArgumentErrorBuilder()
             << "Error parsing hostport port out of range: " << port;
    }
    result.set_port(uint16_t(port));
  }
  return result;
}

absl::StatusOr<HostPort> HostPort::ParseFromSockAddr(const sockaddr* saddr,
                                                     size_t saddr_len) {
  HostPort hp;
  ASSIGN_OR_RETURN(auto ip, IpAddress::ParseFromSockAddr(saddr, saddr_len));
  hp.set_ip(std::move(ip));
  // The Ip parsing verifies the size, no longer need to do it here.
  if (saddr->sa_family == AF_INET) {
    auto saddr_in = reinterpret_cast<const sockaddr_in*>(saddr);
    hp.set_port(ntohs(saddr_in->sin_port));
  } else if (saddr->sa_family == AF_INET6) {
    auto saddr_in6 = reinterpret_cast<const sockaddr_in6*>(saddr);
    hp.set_port(ntohs(saddr_in6->sin6_port));
    if (saddr_in6->sin6_scope_id != 0) {
      hp.set_scope_id(ntohl(saddr_in6->sin6_scope_id));
    }
  }
  return hp;
}

}  // namespace net
}  // namespace whisper
