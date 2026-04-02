#include "dist_platform/router_logic.hpp"

#include <limits>

namespace dist_platform {

std::optional<GatewayLoad> SelectLeastLoadedGateway(const std::vector<GatewayLoad>& gateways) {
  if (gateways.empty()) {
    return std::nullopt;
  }
  const GatewayLoad* best = nullptr;
  double best_metric = std::numeric_limits<double>::max();
  for (const auto& gateway : gateways) {
    const double weight = gateway.weight == 0 ? 1.0 : static_cast<double>(gateway.weight);
    const double metric = gateway.load_score > 0.0 ? gateway.load_score
                                                    : static_cast<double>(gateway.active_connections) / weight;
    if (metric < best_metric) {
      best_metric = metric;
      best = &gateway;
    }
  }
  return *best;
}

}  // namespace dist_platform