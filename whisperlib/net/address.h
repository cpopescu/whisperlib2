#ifndef WHISPERLIB_NET_ADDRESS_H_
#define WHISPERLIB_NET_ADDRESS_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <array>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace whisper {
namespace net {

class IpAddress {
 public:
  // Length of an IPv6 address buffer.
  static constexpr size_t kIpV6Size = 16;
  using IpArray = std::array<uint8_t, kIpV6Size>;

  IpAddress() = default;

  // Creates an IPv4 address - with addr in host byte order.
  explicit IpAddress(uint32_t addr);
  // Creates and address from a provided byte buffer.
  explicit IpAddress(IpArray addr);
  explicit IpAddress(const uint8_t (&addr)[kIpV6Size]);

  // The usual IPv4 loopback address  (127.0.0.1)
  static const IpAddress kIPv4Localhost;
  // The usual IPv6 loopback address  (::1)
  static const IpAddress kIPv6Localhost;

  // If the address is an IPv4 address.
  bool is_ipv4() const;
  // If the address is a pure IPv6 address (i.e. !is_ipv4()).
  bool is_ipv6() const;

  // If the provided address is local link - i.e. 169.254.0.0/16 for v4
  // or fe:80::/64 for ipv6
  bool is_local_link() const;

  // The IPv4 part - in host byte order.
  uint32_t ipv4() const;
  // The array with the entire IPv6 address.
  const IpArray& ipv6() const;
  // The IPv6 address as a net address structure.
  in6_addr ipv6_addr() const;
  // Fills in the provided sockaddr structure family and address, according
  // to the IP address stored in this object.
  void ToSockAddr(sockaddr_storage* addr) const;

  // Creates an IpAddress from a string representation.
  static absl::StatusOr<IpAddress> ParseFromString(absl::string_view ip);
  // Creates an IpAddress from the information contained in provided
  // by the socket address. We expect AF_INET or AF_INET6 family for this.
  static absl::StatusOr<IpAddress> ParseFromSockAddr(const sockaddr* saddr,
                                                     size_t saddr_len);

  // Returns the IP address as a string.
  std::string ToString() const;

  // Operators for equality and order comparison, so this can be used in
  // hashes, maps, and sorted.
  bool operator==(const IpAddress& other) const;
  bool operator!=(const IpAddress& other) const;
  bool operator<(const IpAddress& other) const;
  // Hashing functions for absl hash structures.
  template <typename H>
  friend H AbslHashValue(H h, const IpAddress& ip) {
    return H::combine(std::move(h), ip.addr_);
  }

 private:
  // Where in the addr_ buffer does the ipv4 address start.
  static const size_t kIpV4Index = 12;
  IpArray addr_ = {};
};

//
// Helper to prepare a network structure with address, port and so on.
// Usage example:
//  bind(sock, SockAddrSetter().SetIpAddress(ip).SetPort(333).addr());
//
class SockAddrSetter {
 public:
  // Create a sock addr structure setter - with IPv4 as default family.
  SockAddrSetter();
  // Create a sock addr structure setter - seeded w/ provided ip address.
  SockAddrSetter(const IpAddress& addr);

  // Sets the address and family of the inner structure according to the
  // provided IP address object.
  SockAddrSetter& SetIpAddress(const IpAddress& addr);
  // Set the family of the address structure to IPv6 (if true)
  // or IPv4 (if false).
  SockAddrSetter& SetIpFamily(bool is_ipv6);

  // Sets the port of the address according to the address family.
  // Port provided in host byte order.
  SockAddrSetter& SetPort(uint16_t port);
  // Sets the any-address in the internal socket address, according to family.
  SockAddrSetter& SetUseAnyAddress();
  // For a IPv6 address, sets the scope id.
  // scope_id provided in host byte order.
  SockAddrSetter& SetIpV6ScopeId(uint32_t scope_id);

  // Returns the internal sockaddr buffer.
  const struct sockaddr* addr() const;  // const version
  struct sockaddr* addr();              // non const version

 private:
  // Internal utilities for pointer conversion.
  struct sockaddr_in* sockaddr_in();
  struct sockaddr_in6* sockaddr_in6();

  // Underlying data buffer.
  sockaddr_storage addr_ = {};
};

class HostPort {
 public:
  HostPort() = default;
  HostPort(absl::optional<std::string> host, absl::optional<IpAddress> ip,
           absl::optional<uint16_t> port);

  const absl::optional<std::string>& host() const;
  const absl::optional<IpAddress>& ip() const;
  const absl::optional<uint16_t>& port() const;
  const absl::optional<uint32_t>& scope_id() const;

  HostPort& set_host(absl::string_view value);
  HostPort& set_ip(IpAddress ip);
  HostPort& set_port(uint16_t port);
  HostPort& set_scope_id(uint32_t scope_id);

  // If the host-port is valid - i.e. it has port and (host or ip) set.
  bool IsValid() const;
  // If the host-port has the ip and the port set.
  bool IsResolved() const;
  // Updates this host port with information set in the provided HostPort.
  void Update(const HostPort& hp);

  // Parses the host:port from the provided string.
  //  This means: <host/ip>:[port].
  // The returned HostPort may be Invalid or unresolved. Errors are returned
  // for invalid port numbers or host names.
  static absl::StatusOr<HostPort> ParseFromString(absl::string_view host_port);
  // Parses a HostPort from the provided socket addres structure.
  // This sets just the ip / port / scope_id.
  static absl::StatusOr<HostPort> ParseFromSockAddr(const sockaddr* saddr,
                                                    size_t saddr_len);

  // Canonical string representation of this hostport.
  std::string ToString() const;
  // Returns the best representation for a network use: ip:port if ip present,
  // or host:port if host present. Returns error if !IsValid();
  absl::StatusOr<std::string> ToHostportString() const;
  // Sets the ip / port for this hostport to the provided sock address.
  // Error is returned if not IsResolved.
  absl::Status ToSockAddr(sockaddr_storage* addr) const;

 private:
  absl::optional<std::string> host_;
  absl::optional<IpAddress> ip_;
  absl::optional<uint16_t> port_;
  absl::optional<uint32_t> scope_id_;
};

inline std::ostream& operator<<(std::ostream& os, const IpAddress& ip) {
  return os << ip.ToString();
}
inline std::ostream& operator<<(std::ostream& os, const HostPort& hp) {
  return os << hp.ToString();
}

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_NET_ADDRESS_H_
