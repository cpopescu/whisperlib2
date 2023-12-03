#ifndef WHISPERLIB_DNS_RESOLVE_H_
#define WHISPERLIB_DNS_RESOLVE_H_

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "whisperlib/net/address.h"
#include "whisperlib/sync/producer_consumer_queue.h"

namespace whisper {
namespace net {

// Simple DNS information about a host expressed as a UTF8 string.
class DnsHostInfo {
 public:
  // Builds a DnsHostInfo for a UTF8 host name.
  DnsHostInfo(absl::string_view hostname);

  // Name of the host:
  const std::string& hostname() const { return hostname_; }
  // IPv4 addresses resolved to the host.
  const std::vector<IpAddress>& ipv4() const { return ipv4_; }
  // IPv6 addresses resolved to the host.
  const std::vector<IpAddress>& ipv6() const { return ipv6_; }

  // Converts and returns the hostname for actual DNS resolve (i.e. puncode
  // encoded).
  absl::StatusOr<std::string> GetDnsResolveName() const;

  // If we have any IP addresses resolved to this hostname.
  bool IsValid() const;
  // Sets the corresponding IP addresses for the host.
  void SetIpAddress(std::vector<IpAddress> ipv4, std::vector<IpAddress> ipv6);

  // Returns the first available IP address (IPv4 preference).
  absl::optional<IpAddress> PickFirstAddress() const;
  // Returns the first available IPv4 address.
  absl::optional<IpAddress> PickFirstIpv4Address() const;
  // Returns the first available IPv6 address.
  absl::optional<IpAddress> PickFirstIpv6Address() const;

  // Round robins through the ip addresses (starting w/ the IPV4).
  absl::optional<IpAddress> PickNextAddress() const;
  // Round robins through the ipv4 addresses.
  absl::optional<IpAddress> PickNextIpv4Address() const;
  // Roundf robins through the ipv6 addresses.
  absl::optional<IpAddress> PickNextIpv6Address() const;

  // Returns a string representation for this object for human consumption.
  std::string ToString() const;

 private:
  // Name of the host to resolve, in UTF8 format.
  std::string hostname_;

  // Resolved IPv4 addresses for hostname_.
  std::vector<IpAddress> ipv4_;
  // Resolved IPv6 addresses for hostname_.
  std::vector<IpAddress> ipv6_;

  // Counters for picking next ip / ipv4 / ipv6 in round robin fashion.
  mutable std::atomic<size_t> next_ip_ = ATOMIC_VAR_INIT(0);
  mutable std::atomic<size_t> next_ipv4_ = ATOMIC_VAR_INIT(0);
  mutable std::atomic<size_t> next_ipv6_ = ATOMIC_VAR_INIT(0);
};

// Options for building a DNS resolver.
struct DnsResolverOptions {
  // Number of resolve threads for the resolve, for asynchronous resolve.
  size_t num_threads = 4;
  // Request queue for asynchronous resolve threads.
  size_t queue_size = 100;
  // Duration for waiting on 'put' operation on the resolve queue, else fail.
  absl::Duration put_timeout = absl::Milliseconds(1);

  DnsResolverOptions& set_num_threads(size_t value);
  DnsResolverOptions& set_queue_size(size_t value);
  DnsResolverOptions& set_put_timeout(absl::Duration value);
};

// DNS resolver object. Internally uses getaddrinfo.
// The resolves are not cached.
class DnsResolver {
 public:
  DnsResolver(const DnsResolverOptions& options);
  ~DnsResolver();

  // Returns the default global dns resolver, constructed with default threads.
  static DnsResolver& Default();

  // Resolves a host name, returns the resolve information or error status.
  absl::StatusOr<std::shared_ptr<DnsHostInfo>> Resolve(
      absl::string_view hostname);

  // Resolves a host name asynchronously, and calls the provided callback
  // upon completion.
  using DnsCallback =
      std::function<void(absl::StatusOr<std::shared_ptr<DnsHostInfo>>)>;
  void ResolveAsync(absl::string_view hostname, DnsCallback callback);

 protected:
  void RunResolve(size_t index);

  DnsResolverOptions options_;
  std::vector<std::unique_ptr<std::thread>> threads_;
  using ResolveQueue =
      synch::ProducerConsumerQueue<std::pair<std::string, DnsCallback>>;
  std::vector<std::unique_ptr<ResolveQueue>> resolves_;
  std::atomic<size_t> resolve_index_ = ATOMIC_VAR_INIT(0);
};

}  // namespace net
}  // namespace whisper

#endif  // WHISPERLIB_DNS_RESOLVE_H_
