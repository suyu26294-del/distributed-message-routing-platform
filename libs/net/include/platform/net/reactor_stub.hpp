#pragma once

#include "platform/protocol/frame_codec.hpp"

#include <functional>
#include <string>

namespace platform::net {

struct ConnectionContext {
    std::string connection_id;
    std::string user_id;
    std::string remote_endpoint;
};

class ReactorStub {
public:
    using Handler = std::function<void(const ConnectionContext&, const platform::protocol::Frame&)>;

    explicit ReactorStub(Handler handler);
    void simulate_receive(const ConnectionContext& context, const platform::protocol::Frame& frame) const;

private:
    Handler handler_;
};

}  // namespace platform::net

