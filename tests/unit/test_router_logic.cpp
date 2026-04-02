#include "dist_platform/router_logic.hpp"

#include <gtest/gtest.h>

TEST(RouterLogicTest, SelectsGatewayWithLowestComputedLoad) {
  const std::vector<dist_platform::GatewayLoad> gateways{{"gw-a", 1, 12, 0.0}, {"gw-b", 2, 8, 0.0}, {"gw-c", 1, 3, 0.0}};
  const auto selected = dist_platform::SelectLeastLoadedGateway(gateways);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->instance_id, "gw-c");
}

TEST(RouterLogicTest, PrefersExplicitLoadScoreWhenProvided) {
  const std::vector<dist_platform::GatewayLoad> gateways{{"gw-a", 1, 100, 0.9}, {"gw-b", 1, 100, 0.2}};
  const auto selected = dist_platform::SelectLeastLoadedGateway(gateways);
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->instance_id, "gw-b");
}