#include "platform/net/reactor_stub.hpp"

namespace platform::net {

ReactorStub::ReactorStub(Handler handler) : handler_(std::move(handler)) {}

void ReactorStub::simulate_receive(const ConnectionContext& context, const platform::protocol::Frame& frame) const {
    if (handler_) {
        handler_(context, frame);
    }
}

}  // namespace platform::net

