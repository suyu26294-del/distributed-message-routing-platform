#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dist_platform {

struct GatewayLoad {
  std::string instance_id;
  uint32_t weight = 1;
  uint32_t active_connections = 0;
  double load_score = 0.0;
};

std::optional<GatewayLoad> SelectLeastLoadedGateway(const std::vector<GatewayLoad>& gateways);

}  // namespace dist_platform