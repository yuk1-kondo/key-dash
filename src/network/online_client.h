#pragma once
#include <atomic>
#include <future>
#include <optional>
#include <string>
namespace network {
struct HttpResult { long status{0}; std::string body, error; };
class OnlineClient {
public:
    OnlineClient();
    ~OnlineClient();
    bool configured() const { return !endpoint_.empty(); }
    const std::string& endpoint() const { return endpoint_; }
    bool busy() const { return pending_.valid(); }
    bool start(std::string method, std::string path, std::string body = {}, std::string token = {});
    std::optional<HttpResult> poll();
private:
    std::string endpoint_;
    std::future<HttpResult> pending_;
    std::atomic_bool stopping_{false};
};
}
