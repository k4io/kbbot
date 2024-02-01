#ifndef BINANCE_API_CLIENT_H
#define BINANCE_API_CLIENT_H

#include <chrono>

#include <openssl/ssl.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <boost/algorithm/hex.hpp>

#include <cpr/cpr.h>
#include "json.hpp"

class BAPI {
public:
    BAPI(const std::string& api_key, const std::string& secret_key)
        : api_key_(api_key), secret_key_(secret_key) {}

    nlohmann::json getAccountInfo() {
        std::string endpoint = "/api/v3/account";
        std::unordered_map<std::string, std::string> parameters;

        //parameters.insert({
        //    {"symbol", "BTCUSDT"},
        //    {"interval", "1d"},
        //    {"limit", "50"} });

        const auto& response = sendSignedRequest("GET", endpoint, parameters);

        return nlohmann::json::parse(response.text);
    }

    nlohmann::json getTradingPairInfo(const std::string& symbol, const std::string& timeInterval = "1h", int limit = 50) {
        std::string endpoint = "/api/v3/klines";
        std::unordered_map<std::string, std::string> parameters;

        parameters.insert({
			{"symbol", symbol},
			{"interval", timeInterval},
			{"limit", std::to_string(limit)} });

        auto response = sendPublicRequest("GET", endpoint, parameters);

		return nlohmann::json::parse(response.text);
    }

private:
    static inline std::string base_url_ = "https://api.binance.com";
    const std::string api_key_;
    const std::string secret_key_;

    long long getTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    std::string generateSignature(const std::string& query) {
        return hmac_sha256(this->secret_key_.c_str(), (char*)query.c_str());
    }

    // Openssl HMAC-SHA256
    std::string hmac_sha256(const char* key, char* data) {
        unsigned char* result;
        static char res_hexstring[64];
        int result_len = 32;
        std::string signature;

        result = HMAC(EVP_sha256(), key, strlen((char*)key), const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data)), strlen((char*)data), NULL, NULL);
        for (int i = 0; i < result_len; i++) {
            sprintf(&(res_hexstring[i * 2]), "%02x", result[i]);
        }

        for (int i = 0; i < 64; i++) {
            signature += res_hexstring[i];
        }

        return signature;
    }

    std::string joinQueryParams(const std::unordered_map<std::string, std::string>& params) {
		std::string result;
        for (auto& param : params) {
			result += param.first + "=" + param.second + "&";
		}
		return result.substr(0, result.length() - 1);
	}

    cpr::Response sendSignedRequest(const std::string& httpMethod, const std::string& urlPath, std::unordered_map<std::string, std::string>& parameters) {
        std::string url = this->base_url_ + urlPath + '?';
        std::string queryString;
        std::string timeStamp = "timestamp=" + std::to_string(getTimestamp());

        if (!parameters.empty()) {
            queryString = joinQueryParams(parameters) + "&" + timeStamp;
            url += queryString;
        }
        else {
            queryString = timeStamp;
            url += queryString;
        }

        std::string signature = generateSignature(queryString);  // Replace with your actual API secret key
        url += "&signature=" + signature;
        queryString += "&signature=" + signature;

        if (httpMethod == "POST") {
            auto response = cpr::Post(
                cpr::Url{ url },
                cpr::Header{ {"X-MBX-APIKEY", this->api_key_} }
            );
            return response;
        }
        else if (httpMethod == "GET") {
            auto response = cpr::Get(
                cpr::Url{ url },
                cpr::Header{ {"X-MBX-APIKEY", this->api_key_} }
            );
            return response;
        }
        else throw new std::exception("Http method not implemented!");
	}

    cpr::Response sendPublicRequest(const std::string& httpMethod, const std::string& urlPath, std::unordered_map<std::string, std::string>& parameters) {
		std::string url = this->base_url_ + urlPath + '?';
		std::string queryString;

        if (!parameters.empty()) {
			queryString = joinQueryParams(parameters);
			url += queryString;
		}

        if (httpMethod == "POST") {
            auto response = cpr::Post(
				cpr::Url{ url }
			);
			return response;
		}
        else if (httpMethod == "GET") {
            auto response = cpr::Get(
				cpr::Url{ url }
			);
			return response;
		}
		else throw new std::exception("Http method not implemented!");
	}
};

#endif