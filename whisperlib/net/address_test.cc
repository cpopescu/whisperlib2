#include "whisperlib/net/address.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "whisperlib/status/testing.h"

namespace whisper {
namespace net {

TEST(IpAddress, BasicOps) {
  IpAddress ip1(0x7f000001);
  EXPECT_TRUE(ip1.is_ipv4());
  EXPECT_FALSE(ip1.is_ipv6());
  EXPECT_EQ(ip1.ToString(), "127.0.0.1");
  EXPECT_EQ(ip1.ipv4(), 0x7f000001);
  EXPECT_EQ(ip1, IpAddress::kIPv4Localhost);

  static const uint8_t kBuf[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 1};
  IpAddress ip2(kBuf);
  EXPECT_TRUE(ip2.is_ipv6());
  EXPECT_FALSE(ip2.is_ipv4());
  EXPECT_EQ(ip2.ToString(), "::1");
  EXPECT_EQ(ip2.ipv6(), IpAddress::IpArray(
                            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
  EXPECT_EQ(ip2, IpAddress::kIPv6Localhost);
  EXPECT_NE(ip1, ip2);
  EXPECT_LT(ip2, ip1);

  IpAddress ip3(
      IpAddress::IpArray({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
  EXPECT_EQ(ip2, ip3);
}

TEST(IpAddress, Parse) {
  ASSERT_OK_AND_ASSIGN(auto ip1, IpAddress::ParseFromString("127.0.0.1"));
  EXPECT_EQ(ip1.ToString(), "127.0.0.1");
  EXPECT_EQ(ip1, IpAddress::kIPv4Localhost);
  ASSERT_OK_AND_ASSIGN(auto ip2, IpAddress::ParseFromString("::1"));
  EXPECT_EQ(ip2.ToString(), "::1");
  EXPECT_EQ(ip2, IpAddress::kIPv6Localhost);
  ASSERT_OK_AND_ASSIGN(
      auto ip3,
      IpAddress::ParseFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
  EXPECT_EQ(ip3.ToString(), "2001:db8:85a3::8a2e:370:7334");
  ASSERT_OK_AND_ASSIGN(
      auto ip4, IpAddress::ParseFromString("2001:db8:85a3::8a2e:370:7334"));
  EXPECT_EQ(ip4.ToString(), "2001:db8:85a3::8a2e:370:7334");

  EXPECT_RAISES_WITH_MESSAGE_THAT(IpAddress::ParseFromString("").status(),
                                  InvalidArgument,
                                  testing::HasSubstr("Empty IP address"));
  EXPECT_RAISES_WITH_MESSAGE_THAT(
      IpAddress::ParseFromString("foobar").status(), InvalidArgument,
      testing::HasSubstr("IP address string could not be parsed"));
}

TEST(IpAddress, LocalLink) {
  {
    ASSERT_OK_AND_ASSIGN(auto ip, IpAddress::ParseFromString("127.0.0.3"));
    EXPECT_FALSE(ip.is_local_link());
  }
  {
    ASSERT_OK_AND_ASSIGN(auto ip, IpAddress::ParseFromString("169.254.0.15"));
    EXPECT_TRUE(ip.is_local_link());
  }
  {
    ASSERT_OK_AND_ASSIGN(auto ip, IpAddress::ParseFromString("fe80::33:2"));
    EXPECT_TRUE(ip.is_local_link());
  }
  {
    ASSERT_OK_AND_ASSIGN(
        auto ip,
        IpAddress::ParseFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    EXPECT_FALSE(ip.is_local_link());
  }
}

TEST(IpAddress, SockAddr) {
  {
    ASSERT_OK_AND_ASSIGN(auto ip, IpAddress::ParseFromString("127.0.0.3"));
    sockaddr_storage addr;
    ip.ToSockAddr(&addr);
    EXPECT_EQ(addr.ss_family, AF_INET);
    EXPECT_EQ((reinterpret_cast<struct sockaddr_in*>(&addr))->sin_addr.s_addr,
              htonl(0x7f000003));
  }
  {
    ASSERT_OK_AND_ASSIGN(
        auto ip,
        IpAddress::ParseFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    sockaddr_storage addr;
    ip.ToSockAddr(&addr);
    EXPECT_EQ(addr.ss_family, AF_INET6);
    static const uint8_t kExpected[] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3,
                                        0,    0,    0,    0,    0x8a, 0x2e,
                                        0x03, 0x70, 0x73, 0x34};
    EXPECT_EQ(
        memcmp(
            (reinterpret_cast<struct sockaddr_in6*>(&addr))->sin6_addr.s6_addr,
            kExpected, sizeof(kExpected)),
        0);
  }
}

TEST(SockAddrSetter, Basic) {
  {
    SockAddrSetter s(IpAddress::kIPv4Localhost);
    s.SetPort(0x1234);
    EXPECT_EQ(s.addr()->sa_family, AF_INET);
    EXPECT_EQ((reinterpret_cast<const struct sockaddr_in*>(s.addr()))
                  ->sin_addr.s_addr,
              htonl(0x7f000001));
    EXPECT_EQ((reinterpret_cast<const struct sockaddr_in*>(s.addr()))->sin_port,
              htons(0x1234));
  }
  {
    SockAddrSetter s;
    s.SetIpFamily(false).SetUseAnyAddress().SetPort(0x1234);
    EXPECT_EQ(s.addr()->sa_family, AF_INET);
    EXPECT_EQ((reinterpret_cast<const struct sockaddr_in*>(s.addr()))
                  ->sin_addr.s_addr,
              htonl(INADDR_ANY));
    EXPECT_EQ((reinterpret_cast<const struct sockaddr_in*>(s.addr()))->sin_port,
              htons(0x1234));
  }
  {
    ASSERT_OK_AND_ASSIGN(
        auto ip,
        IpAddress::ParseFromString("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    static const uint8_t kExpected[] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3,
                                        0,    0,    0,    0,    0x8a, 0x2e,
                                        0x03, 0x70, 0x73, 0x34};
    SockAddrSetter s;
    s.SetIpAddress(ip).SetPort(0x1234).SetIpV6ScopeId(0x3456);
    EXPECT_EQ(s.addr()->sa_family, AF_INET6);
    EXPECT_EQ(memcmp((reinterpret_cast<const struct sockaddr_in6*>(s.addr()))
                         ->sin6_addr.s6_addr,
                     kExpected, sizeof(kExpected)),
              0);
    EXPECT_EQ(
        (reinterpret_cast<const struct sockaddr_in6*>(s.addr()))->sin6_port,
        htons(0x1234));
    EXPECT_EQ(
        (reinterpret_cast<const struct sockaddr_in6*>(s.addr()))->sin6_scope_id,
        htonl(0x3456));
  }
  {
    SockAddrSetter s;
    s.SetIpFamily(true).SetUseAnyAddress().SetPort(0x1234);
    EXPECT_EQ(s.addr()->sa_family, AF_INET6);
    EXPECT_EQ(
        (reinterpret_cast<const struct sockaddr_in6*>(s.addr()))->sin6_port,
        htons(0x1234));
    static const uint8_t kExpected[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(memcmp((reinterpret_cast<const struct sockaddr_in6*>(s.addr()))
                         ->sin6_addr.s6_addr,
                     kExpected, sizeof(kExpected)),
              0);
  }
}

TEST(HostPort, Base) {
  {
    HostPort hp;
    EXPECT_FALSE(hp.host().has_value());
    EXPECT_FALSE(hp.ip().has_value());
    EXPECT_FALSE(hp.port().has_value());
    EXPECT_FALSE(hp.IsValid());
    EXPECT_FALSE(hp.IsResolved());
    EXPECT_EQ(hp.ToString(), "[]");
    EXPECT_RAISES(hp.ToHostportString().status(), FailedPrecondition);
  }
  {
    HostPort hp("foobar", {}, {});
    EXPECT_TRUE(hp.host().has_value());
    EXPECT_EQ(hp.host().value(), "foobar");
    EXPECT_FALSE(hp.ip().has_value());
    EXPECT_FALSE(hp.port().has_value());
    EXPECT_FALSE(hp.IsValid());
    EXPECT_FALSE(hp.IsResolved());
    EXPECT_EQ(hp.ToString(), "foobar");
    EXPECT_RAISES(hp.ToHostportString().status(), FailedPrecondition);
  }
  {
    HostPort hp("foobar", {}, 22);
    EXPECT_TRUE(hp.host().has_value());
    EXPECT_EQ(hp.host().value(), "foobar");
    EXPECT_FALSE(hp.ip().has_value());
    EXPECT_TRUE(hp.port().has_value());
    EXPECT_EQ(hp.port().value(), 22);
    EXPECT_TRUE(hp.IsValid());
    EXPECT_FALSE(hp.IsResolved());
    EXPECT_EQ(hp.ToString(), "foobar:22");
    ASSERT_OK_AND_ASSIGN(auto s, hp.ToHostportString());
    EXPECT_EQ(s, "foobar:22");
  }
  {
    HostPort hp("foobar", IpAddress::ParseFromString("127.0.0.33").value(), 22);
    EXPECT_TRUE(hp.host().has_value());
    EXPECT_EQ(hp.host().value(), "foobar");
    EXPECT_TRUE(hp.ip().has_value());
    EXPECT_EQ(hp.ip().value().ToString(), "127.0.0.33");
    EXPECT_TRUE(hp.port().has_value());
    EXPECT_EQ(hp.port().value(), 22);
    EXPECT_TRUE(hp.IsValid());
    EXPECT_TRUE(hp.IsResolved());
    EXPECT_EQ(hp.ToString(), "foobar[127.0.0.33]:22");
    ASSERT_OK_AND_ASSIGN(auto s, hp.ToHostportString());
    EXPECT_EQ(s, "127.0.0.33:22");
  }
  {
    HostPort hp(
        "foobar",
        IpAddress::ParseFromString("2001:db8:85a3::8a2e:370:7334").value(), 22);
    EXPECT_TRUE(hp.host().has_value());
    EXPECT_EQ(hp.host().value(), "foobar");
    EXPECT_TRUE(hp.ip().has_value());
    EXPECT_EQ(hp.ip().value().ToString(), "2001:db8:85a3::8a2e:370:7334");
    EXPECT_TRUE(hp.port().has_value());
    EXPECT_EQ(hp.port().value(), 22);
    EXPECT_TRUE(hp.IsValid());
    EXPECT_TRUE(hp.IsResolved());
    EXPECT_EQ(hp.ToString(), "foobar[2001:db8:85a3::8a2e:370:7334]:22");
    ASSERT_OK_AND_ASSIGN(auto s, hp.ToHostportString());
    EXPECT_EQ(s, "[2001:db8:85a3::8a2e:370:7334]:22");
  }
}

TEST(HostPort, Parse) {
  {
    ASSERT_OK_AND_ASSIGN(auto hp, HostPort::ParseFromString(""));
    EXPECT_FALSE(hp.host().has_value());
    EXPECT_FALSE(hp.ip().has_value());
    EXPECT_FALSE(hp.port().has_value());
  }
  {
    ASSERT_OK_AND_ASSIGN(auto hp, HostPort::ParseFromString("foobar"));
    EXPECT_TRUE(hp.host().has_value());
    EXPECT_EQ(hp.host().value(), "foobar");
    EXPECT_FALSE(hp.ip().has_value());
    EXPECT_FALSE(hp.port().has_value());
  }
  {
    ASSERT_OK_AND_ASSIGN(auto hp, HostPort::ParseFromString("foobar:22"));
    EXPECT_TRUE(hp.host().has_value());
    EXPECT_EQ(hp.host().value(), "foobar");
    EXPECT_FALSE(hp.ip().has_value());
    EXPECT_TRUE(hp.port().has_value());
    EXPECT_EQ(hp.port().value(), 22);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto hp, HostPort::ParseFromString("127.0.0.1:22"));
    EXPECT_FALSE(hp.host().has_value());
    EXPECT_TRUE(hp.ip().has_value());
    EXPECT_EQ(hp.ip().value().ToString(), "127.0.0.1");
    EXPECT_TRUE(hp.port().has_value());
    EXPECT_EQ(hp.port().value(), 22);
  }
  {
    ASSERT_OK_AND_ASSIGN(auto hp, HostPort::ParseFromString(
                                      "[2001:db8:85a3::8a2e:370:7334]:22"));
    EXPECT_FALSE(hp.host().has_value());
    EXPECT_TRUE(hp.ip().has_value());
    EXPECT_EQ(hp.ip().value().ToString(), "2001:db8:85a3::8a2e:370:7334");
    EXPECT_TRUE(hp.port().has_value());
    EXPECT_EQ(hp.port().value(), 22);
  }
  {
    ASSERT_OK_AND_ASSIGN(
        auto hp, HostPort::ParseFromString("[2001:db8:85a3::8a2e:370:7334]"));
    EXPECT_FALSE(hp.host().has_value());
    EXPECT_TRUE(hp.ip().has_value());
    EXPECT_EQ(hp.ip().value().ToString(), "2001:db8:85a3::8a2e:370:7334");
    EXPECT_FALSE(hp.port().has_value());
  }
  EXPECT_RAISES(
      HostPort::ParseFromString("2001:db8:85a3::8a2e:370:7334:22").status(),
      InvalidArgument);
  EXPECT_RAISES(HostPort::ParseFromString("127.0.0.1:foo").status(),
                InvalidArgument);
  EXPECT_RAISES(HostPort::ParseFromString("foobar:foo").status(),
                InvalidArgument);
  EXPECT_RAISES(HostPort::ParseFromString("foobar:").status(), InvalidArgument);
  EXPECT_RAISES(HostPort::ParseFromString("foobar:0").status(),
                InvalidArgument);
  EXPECT_RAISES(HostPort::ParseFromString("foobar:100000").status(),
                InvalidArgument);
}

}  // namespace net
}  // namespace whisper
