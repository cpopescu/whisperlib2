#include "whisperlib/net/dns_resolve.h"

#include "absl/functional/bind_front.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "whisperlib/status/testing.h"

namespace whisper {
namespace net {

TEST(DnsHostInfo, BasicOps) {
  std::vector<IpAddress> ipv4, ipv6;
  for (uint32_t i = 0; i < 20; ++i) {
    ipv4.emplace_back(IpAddress(0x7f000001 + i));
  }
  for (uint32_t i = 0; i < 10; ++i) {
    ipv6.emplace_back(
        IpAddress::ParseFromString(absl::StrCat("::",  (1 + i))).value());
  }
  {
    DnsHostInfo hi("foo");
    EXPECT_EQ(hi.hostname(), "foo");
    EXPECT_EQ(hi.ipv4().size(), 0);
    EXPECT_EQ(hi.ipv6().size(), 0);
    EXPECT_FALSE(hi.PickFirstAddress().has_value());
    EXPECT_FALSE(hi.PickFirstIpv4Address().has_value());
    EXPECT_FALSE(hi.PickFirstIpv6Address().has_value());
    EXPECT_FALSE(hi.PickNextAddress().has_value());
    EXPECT_FALSE(hi.PickNextIpv4Address().has_value());
    EXPECT_FALSE(hi.PickNextIpv6Address().has_value());
  }
  {
    DnsHostInfo hi("foo");
    hi.SetIpAddress(ipv4, {});
    EXPECT_EQ(hi.hostname(), "foo");
    EXPECT_EQ(hi.ipv4().size(), 20);
    EXPECT_EQ(hi.ipv6().size(), 0);
    EXPECT_EQ(hi.PickFirstAddress().value(), ipv4[0]);
    EXPECT_EQ(hi.PickFirstIpv4Address().value(), ipv4[0]);
    EXPECT_FALSE(hi.PickFirstIpv6Address().has_value());
    for (size_t i = 0; i < 50; ++i) {
      EXPECT_EQ(hi.PickNextAddress().value(), ipv4[i % 20]);
      EXPECT_EQ(hi.PickNextIpv4Address().value(), ipv4[i % 20]);
      EXPECT_FALSE(hi.PickNextIpv6Address().has_value());
    }
  }
  {
    DnsHostInfo hi("foo");
    hi.SetIpAddress({}, ipv6);
    EXPECT_EQ(hi.hostname(), "foo");
    EXPECT_EQ(hi.ipv4().size(), 0);
    EXPECT_EQ(hi.ipv6().size(), 10);
    EXPECT_EQ(hi.PickFirstAddress().value(), ipv6[0]);
    EXPECT_FALSE(hi.PickFirstIpv4Address().has_value());
    EXPECT_EQ(hi.PickFirstIpv6Address().value(), ipv6[0]);
    for (size_t i = 0; i < 50; ++i) {
      EXPECT_FALSE(hi.PickNextIpv4Address().has_value());
      EXPECT_EQ(hi.PickNextIpv6Address().value(), ipv6[i % 10]);
      EXPECT_EQ(hi.PickNextAddress().value(), ipv6[i % 10]);
    }
  }
  {
    DnsHostInfo hi("foo");
    hi.SetIpAddress(ipv4, ipv6);
    EXPECT_EQ(hi.hostname(), "foo");
    EXPECT_EQ(hi.ipv4().size(), 20);
    EXPECT_EQ(hi.ipv6().size(), 10);
    EXPECT_EQ(hi.PickFirstAddress().value(), ipv4[0]);
    EXPECT_EQ(hi.PickFirstIpv4Address().value(), ipv4[0]);
    EXPECT_EQ(hi.PickFirstIpv6Address().value(), ipv6[0]);
    for (size_t i = 0; i < 50; ++i) {
      EXPECT_EQ(hi.PickNextIpv4Address().value(), ipv4[i % 20]);
      EXPECT_EQ(hi.PickNextIpv6Address().value(), ipv6[i % 10]);
      if (i % 30 < 20) {
        EXPECT_EQ(hi.PickNextAddress().value(), ipv4[i % 30]);
      } else {
        EXPECT_EQ(hi.PickNextAddress().value(), ipv6[(i % 30) - 20]);
      }
    }
  }
}

TEST(DnsHostInfo, Punycode) {
  {
    DnsHostInfo hi("foobar");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "foobar");
  }
  {
    DnsHostInfo hi("президент");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "xn--d1abbgf6aiiy");
  }
  {
    DnsHostInfo hi("www.google.com");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "www.google.com");
  }
  {
    DnsHostInfo hi("www.google.中国");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "www.google.xn--fiqs8s");
  }
  {
    DnsHostInfo hi("президент.рф");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "xn--d1abbgf6aiiy.xn--p1ai");
  }
  {
    DnsHostInfo hi("www.中国移动.cn");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "www.xn--fiq02ib9d179b.cn");
  }
  {
    DnsHostInfo hi("中国移动.中国");
    EXPECT_EQ(hi.GetDnsResolveName().value(), "xn--fiq02ib9d179b.xn--fiqs8s");
  }
}

TEST(DnsResolver, Resolve) {
  auto& resolver = DnsResolver::Default();
  for (auto hostname : { "www.google.com" }) {
    // , "中国移动.中国", "президент.рф" }) {
    for (size_t i = 0; i < 30; ++i) {
      absl::Time start = absl::Now();
      ASSERT_OK_AND_ASSIGN(auto info, resolver.Resolve(hostname));
      LOG(INFO) << "Resolved in: " << (absl::Now() - start) << "\n"
                << info->ToString();
    }
  }
}

void ResolveDone(
    std::atomic_size_t* count,
    absl::StatusOr<std::shared_ptr<DnsHostInfo>> data) {
  if (data.ok()) {
    count->fetch_add(1);
    LOG(INFO) << "Resolved async: " << data.value()->ToString();
  } else {
    LOG(ERROR) << "Error in dns resolve: " << data.status();
  }
}

TEST(DnsResolver, ResolveAsync) {
  std::atomic_size_t count(0);
  {
    DnsResolver resolve({});
    for (size_t i = 0; i < 30; ++i) {
      resolve.ResolveAsync("www.google.com", absl::bind_front(
          &ResolveDone, &count));
    }
    LOG(INFO) << "Resolves issued.";
  }
  LOG(INFO) << "Resolver done.";
  EXPECT_EQ(count.load(), 30);
}

}  // namespace net
}  // namespace whisper
