#include "whisperlib/net/address.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace whisper {
namespace net {

TEST(IpAddress, BasicOps) {
  IpAddress ip1(0x7f000001);
  EXPECT_TRUE(ip1.is_ipv4());
  EXPECT_FALSE(ip1.is_ipv6());
  EXPECT_EQ(ip1.ToString(), "127.0.0.1");
  EXPECT_EQ(ip1.ipv4(), 0x7f000001);
  EXPECT_EQ(ip1, IpAddress::kIPv4Loopback);

  static const uint8_t kBuf[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
  IpAddress ip2(kBuf);
  EXPECT_TRUE(ip2.is_ipv6());
  EXPECT_FALSE(ip2.is_ipv4());
  EXPECT_EQ(ip2.ToString(), "::1");
  EXPECT_EQ(ip2.ipv6(), IpAddress::IpArray({
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }));
  EXPECT_EQ(ip2, IpAddress::kIPv6Loopback);
  EXPECT_NE(ip1, ip2);
  EXPECT_LT(ip2, ip1);

  IpAddress ip3(IpAddress::IpArray({
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }));
  EXPECT_EQ(ip2, ip3);
}

}  // namespace net
}  // namespace whisper
