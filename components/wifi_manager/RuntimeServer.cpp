#include "RuntimeServer.hpp"

namespace wifi_manager {

RuntimeServer::RuntimeServer(WiFiContext& ctx)
    : ctx(ctx)
{
}

void RuntimeServer::start() {
    // start runtime HTTP server here
}

void RuntimeServer::stop() {
    // stop runtime server here
}

void RuntimeServer::handleStatusRequest() {
    // respond with current status using ctx.state, etc.
}

} // namespace wifi_manager