#include "online_client.h"
#include <curl/curl.h>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#ifndef KEY_DASH_API_URL
#define KEY_DASH_API_URL ""
#endif
namespace network {
namespace {
size_t append(char* data, size_t size, size_t count, void* context) {
    auto& output = *static_cast<std::string*>(context);
    const auto bytes = size * count;
    if (bytes > 100000 || output.size() > 100000 - bytes) return 0;
    output.append(data, bytes); return bytes;
}
int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<std::atomic_bool*>(context)->load() ? 1 : 0;
}
}
OnlineClient::OnlineClient() {
    static std::once_flag initialized;
    std::call_once(initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    const char* custom = std::getenv("KEY_DASH_API_URL");
    endpoint_ = custom ? custom : KEY_DASH_API_URL;
    while (!endpoint_.empty() && endpoint_.back() == '/') endpoint_.pop_back();
    // Only local test servers may use HTTP. Never disable certificate verification.
    const bool secure = endpoint_.rfind("https://", 0) == 0;
    const bool local = endpoint_.rfind("http://127.0.0.1:", 0) == 0 || endpoint_.rfind("http://localhost:", 0) == 0;
    if (!secure && !local) endpoint_.clear();
}
OnlineClient::~OnlineClient() { stopping_ = true; if (pending_.valid()) pending_.wait(); }
bool OnlineClient::start(std::string method, std::string path, std::string body, std::string token) {
    if (busy() || !configured()) return false;
    const auto url = endpoint_ + path;
    pending_ = std::async(std::launch::async, [this, url, method, body, token] {
        HttpResult result;
        auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(curl_easy_init(), curl_easy_cleanup);
        if (!curl) { result.error = "Network could not start"; return result; }
        curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
        if (!token.empty()) headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
        auto cleanup = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>(headers, curl_slist_free_all);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "KEY-DASH/0.2.0");
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 4000L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, 8000L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https,http");
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, append);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &result.body);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &stopping_);
        if (method == "POST") {
            curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
        const auto status = curl_easy_perform(curl.get());
        if (status != CURLE_OK) result.error = "Connection failed. Check Wi-Fi.";
        else curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.status);
        return result;
    });
    return true;
}
std::optional<HttpResult> OnlineClient::poll() {
    if (!pending_.valid() || pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
    return pending_.get();
}
}
