#include "whisperlib/net/dns_resolve.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "absl/container/flat_hash_set.h"
#include "unicode/errorcode.h"
#include "unicode/idna.h"
#include "whisperlib/io/errno.h"
#include "whisperlib/status/status.h"

namespace whisper {
namespace net {

DnsHostInfo::DnsHostInfo(absl::string_view hostname)
  : hostname_(hostname) {
}

bool DnsHostInfo::IsValid() const {
  return !ipv4_.empty() || !ipv6_.empty();
}

void DnsHostInfo::SetIpAddress(std::vector<IpAddress> ipv4,
                               std::vector<IpAddress> ipv6) {
  ipv4_ = std::move(ipv4);
  ipv6_ = std::move(ipv6);
}

absl::optional<IpAddress> DnsHostInfo::PickFirstAddress() const {
  if (!ipv4_.empty()) { return ipv4_.front(); }
  if (!ipv6_.empty()) { return ipv6_.front(); }
  return {};
}
absl::optional<IpAddress> DnsHostInfo::PickFirstIpv4Address() const {
  if (!ipv4_.empty()) { return ipv4_.front(); }
  return {};
}
absl::optional<IpAddress> DnsHostInfo::PickFirstIpv6Address() const {
  if (!ipv6_.empty()) { return ipv6_.front(); }
  return {};
}

absl::optional<IpAddress> DnsHostInfo::PickNextAddress() const {
  if (ipv4_.empty() && ipv6_.empty()) {
    return {};
  }
  const size_t ndx = next_ip_.fetch_add(1) % (ipv4_.size() + ipv6_.size());
  return ndx < ipv4_.size() ? ipv4_[ndx] : ipv6_[ndx - ipv4_.size()];
}

absl::optional<IpAddress> DnsHostInfo::PickNextIpv4Address() const {
  if (ipv4_.empty()) {
    return {};
  }
  return ipv4_[next_ipv4_.fetch_add(1) % ipv4_.size()];
}
absl::optional<IpAddress> DnsHostInfo::PickNextIpv6Address() const {
  if (ipv6_.empty()) {
    return {};
  }
  return ipv6_[next_ipv6_.fetch_add(1) % ipv6_.size()];
}

namespace {
const icu::IDNA* CreateIdna() {
  icu::ErrorCode err;
  const icu::IDNA* idna = icu::IDNA::createUTS46Instance(
      UIDNA_NONTRANSITIONAL_TO_ASCII, err);
  LOG_IF(ERROR, !err.isSuccess())
    << "Error creating IDNA icu object: " << err.errorName();
  return idna;
}
}  // namespace

absl::StatusOr<std::string> DnsHostInfo::GetDnsResolveName() const {
  static const icu::IDNA* idna = CreateIdna();
  bool is_ascii = true;
  for (auto c : hostname_) {
    if (c < 0 || c >= 0x80) {
      is_ascii = false; break;
    }
  }
  if (is_ascii) { return std::string(hostname_); }
  if (ABSL_PREDICT_FALSE(idna == nullptr)) {
    return absl::InternalError("Error creating IDNA icu object.");
  }
  std::string result;
  icu::StringByteSink<std::string> sink(&result);
  icu::IDNAInfo info;
  icu::ErrorCode icu_error;
  idna->nameToASCII_UTF8(hostname_, sink, info, icu_error);
  if (icu_error.isFailure() || info.hasErrors()) {
    return status::InvalidArgumentErrorBuilder()
      << "Error converting hostname to punycode: " << icu_error.errorName()
      << " / error bits: " << info.getErrors();
  }
  return result;
}
std::string DnsHostInfo::ToString() const {
  std::string s;
  absl::StrAppend(&s, "Hostname: `", hostname_, "`\n");
  auto resolve_name = GetDnsResolveName();
  if (resolve_name.ok()) {
    absl::StrAppend(&s, "DNS resolve name: `", resolve_name.value(), "`\n");
  } else {
    absl::StrAppend(&s, "Error DNS name: `",
                    resolve_name.status().message(), "`\n");
  }
  for (const auto& ip : ipv4_) {
    absl::StrAppend(&s, "  IPv4: ", ip.ToString(), "\n");
  }
  for (const auto& ip : ipv6_) {
    absl::StrAppend(&s, "  IPv6: ", ip.ToString(), "\n");
  }
  return s;
}


DnsResolverOptions& DnsResolverOptions::set_num_threads(size_t value) {
  num_threads = value;
  return *this;
}
DnsResolverOptions& DnsResolverOptions::set_queue_size(size_t value) {
  queue_size = value;
  return *this;
}
DnsResolverOptions& DnsResolverOptions::set_put_timeout(absl::Duration value) {
  put_timeout = value;
  return *this;
}

DnsResolver& DnsResolver::Default() {
  static DnsResolver* kResolver = new DnsResolver(DnsResolverOptions());
  return *kResolver;
}


DnsResolver::DnsResolver(const DnsResolverOptions& options)
  : options_(options) {
  CHECK_GT(options_.num_threads, 0UL);
  CHECK_GT(options_.queue_size, 0UL);
  threads_.reserve(options_.num_threads);
  resolves_.reserve(options_.num_threads);
  for (size_t i = 0; i < options_.num_threads; ++i) {
    threads_.emplace_back(absl::make_unique<std::thread>(
        &DnsResolver::RunResolve, this, i));
    resolves_.emplace_back(absl::make_unique<ResolveQueue>(
        options_.queue_size));
  }
}
DnsResolver::~DnsResolver() {
  for (const auto& queue : resolves_) {
    queue->Put({"", nullptr});
  }
  for (const auto& t : threads_) {
    t->join();
  }
}

void DnsResolver::RunResolve(size_t index) {
  while (true) {
    auto req = resolves_[index]->Get();
    if (req.first.empty() && req.second == nullptr) {
      break;
    }
    req.second(Resolve(req.first));
  }
}

void DnsResolver::ResolveAsync(
    absl::string_view hostname, DnsCallback callback) {
  const size_t index = resolve_index_.fetch_add(1) % resolves_.size();
  if (!resolves_[index]->Put(std::make_pair(
          std::string(hostname), callback), options_.put_timeout)) {
    callback(absl::InternalError("Asynchronous resolve queue is full."));
  }
}

namespace {
status::StatusWriter AddrInfoToStatus(int err) {
  switch (err) {
  case EAI_ADDRFAMILY:
    return status::InvalidArgumentErrorBuilder()
      << "[EAI_ADDRFAMILY] The specified network host does not have any "
      "network addresses in the requested address family.";
  case EAI_AGAIN:
    return status::UnavailableErrorBuilder()
      << "[EAI_AGAIN] The name server returned a temporary failure indication. "
      "Try again later.";
  case EAI_BADFLAGS:
    return status::InvalidArgumentErrorBuilder()
      << "[EAI_BADFLAGS] hints.ai_flags contains invalid flags; or, "
      "hints.ai_flags included AI_CANONNAME and name was NULL.";
  case EAI_FAIL:
    return status::InternalErrorBuilder()
      << "[EAI_FAIL] The name server returned a permanent failure indication.";
  case EAI_FAMILY:
    return status::UnimplementedErrorBuilder()
      << "[EAI_FAMILY] The requested address family is not supported.";
  case EAI_MEMORY:
    return status::ResourceExhaustedErrorBuilder()
      << "[EAI_MEMORY] Out of memory.";
  case EAI_NODATA:
    return status::NotFoundErrorBuilder()
      << "[EAI_NODATA] The specified network host exists, but does not have "
      "any network addresses defined.";
  case EAI_NONAME:
    return status::NotFoundErrorBuilder()
      << "[EAI_NONAME] The node or service is not known.";
  case EAI_SERVICE:
    return status::NotFoundErrorBuilder()
      << "[EAI_SERVICE] The requested service is not available for the "
      "requested socket type.";
  case EAI_SOCKTYPE:
    return status::NotFoundErrorBuilder()
      << "The requested socket type is not supported.";
  case EAI_SYSTEM:
    return error::ErrnoToStatus(error::Errno());
  }
  return status::InternalErrorBuilder() << "Unknown error during getaddrinfo.";
}
}  // namespace

absl::StatusOr<std::shared_ptr<DnsHostInfo>>
DnsResolver::Resolve(absl::string_view hostname) {
  auto hi = std::make_shared<DnsHostInfo>(hostname);
  struct addrinfo* result = nullptr;
  ASSIGN_OR_RETURN(auto resolve_name, hi->GetDnsResolveName(),
                   _ << "Obtaining DNS resolve name for `" << hostname << "`");
  const int err = ::getaddrinfo(
      resolve_name.c_str(), nullptr, nullptr, &result);
  if (err != 0) {
    return AddrInfoToStatus(err) << " DNS Resolving: `" << hostname << "`";
  }
  absl::flat_hash_set<IpAddress> ipv4, ipv6;
  for (struct addrinfo* res = result; res != nullptr; res = res->ai_next) {
    auto ss = reinterpret_cast<const struct sockaddr_storage*>(res->ai_addr);
    if ( ss->ss_family == AF_INET ) {
      auto p = reinterpret_cast<const sockaddr_in*>(ss);
      ipv4.insert(IpAddress(ntohl(p->sin_addr.s_addr)));
    } else if ( ss->ss_family == AF_INET6 ) {
      auto p = reinterpret_cast<const sockaddr_in6*>(ss);
      ipv6.insert(IpAddress(p->sin6_addr.s6_addr));
      // reinterpret_cast<const uint8_t*>()));
    }
  }
  hi->SetIpAddress(std::vector<IpAddress>(ipv4.begin(), ipv4.end()),
                   std::vector<IpAddress>(ipv6.begin(), ipv6.end()));
  return {std::move(hi)};
}

}  // namespace net
}  // namespace whisper
