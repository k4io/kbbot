#ifndef BINANCE_API_CLIENT_H
#define BINANCE_API_CLIENT_H

#include <chrono>

#include <cpr/cpr.h>
#include "json.hpp"

class BAPI {
public:
    BAPI(const std::string& api_key, const std::string& secret_key)
        : api_key_(api_key), secret_key_(secret_key) {}

    nlohmann::json getAccountInfo() {
        std::string endpoint = "/api/v3/account";
        std::string timestamp = std::to_string(getTimestamp());
        std::string signature = generateSignature("GET", endpoint, timestamp);

        auto response = cpr::Get(
            cpr::Url{ base_url_ + endpoint },
            cpr::Parameters{ {"timestamp", timestamp} },
            cpr::Header{ {"X-MBX-APIKEY", api_key_}, {"signature", signature} }
        );

        return nlohmann::json::parse(response.text);
    }

private:
    const std::string base_url_ = "https://api.binance.com";
    const std::string api_key_;
    const std::string secret_key_;

    long long getTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    std::string generateSignature(const std::string& method, const std::string& endpoint, const std::string& timestamp) {
        std::string query_string = "timestamp=" + timestamp;

        std::string signature_payload = method + "\n" + endpoint + "\n" + query_string;

        auto hash = hmac_sha256(secret_key_, signature_payload);
        return hex_encode(hash.begin(), hash.end());
    }

    // HMAC-SHA256 function (You may need to implement this part)
    std::vector<unsigned char> hmac_sha256(const std::string& key, const std::string& data) {
        // Implement HMAC-SHA256 using your preferred library or method
        // This is a placeholder, and you might need to replace it with a proper implementation
        // Example: openssl, Crypto++ library, etc.
        return {};
    }

    // Hex encoding function (You may need to implement this part)
    template<typename InputIt>
    std::string hex_encode(InputIt begin, InputIt end) {
        // Implement hex encoding using your preferred method
        // Example: Boost.Hex library, custom implementation, etc.
        return {};
    }
};

#endif