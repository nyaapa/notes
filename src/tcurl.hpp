// Basically telegram logic + curl calls

#pragma once

#include <iostream>
#include <string>

#include "curl/curl.h"

#include "json.hpp"

// TODO: move inside?
using json = nlohmann::json;
using namespace std::string_literals;

struct message {
	const ulong update_id;
	const ulong chat_id;
	const ulong message_id;
	const std::string_view username;
	const std::string_view text;
};

class tcurl {
	static size_t curl_write(void const *contents, size_t size, size_t nmemb, void *userp) {
		((std::string*)userp)->append((char*)contents, size * nmemb);
		return size * nmemb;
	}

	std::string const bot_key;
	std::string const base_address;
	CURL* curl;
public:
	static constexpr auto api_host = "api.telegram.org";

	explicit tcurl(std::string_view bot_key) : bot_key{bot_key}, base_address{"https://"s + api_host + "/bot"s + this->bot_key + "/"s}  {
		this->curl = curl_easy_init();
		if (!this->curl) {
			throw std::runtime_error("CURL initialization failed");
		}
	}

	~tcurl() {
		curl_easy_cleanup(this->curl);
	}

	tcurl(const tcurl&) = delete;
	tcurl(tcurl&&) = delete;

	tcurl& operator=(const tcurl&) = delete;
	tcurl& operator=(tcurl&&) = delete;

	json request(std::string_view const method) const {
		std::string buffer;

		std::string uri = base_address + method.data();
		curl_easy_setopt(this->curl, CURLOPT_URL, uri.c_str());
		curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &buffer);
		curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, &curl_write);

		if (CURLcode res = curl_easy_perform(this->curl); res) {
			throw std::runtime_error("Curl request failed: " + std::to_string(res));
		}

		std::cout << "For " << method << "\n";
		std::cout << buffer << "\n";
		auto response = json::parse(buffer);
		if (!response["ok"].get<bool>()) {
			throw std::runtime_error("Request finished unsuccessfully");
		}

		json result;
		response["result"].swap(result);
		return result;
	}

	template<typename T>
	long process_updates(ulong starting_from, T process) const {
		auto updates = this->request("getUpdates?offset=" + std::to_string(starting_from + 1));

		for (auto& update : updates) {
			auto update_id = update["update_id"].get<ulong>();
			if (auto& msg = update["message"]; !msg.is_null()) {
				process(message {
					.update_id = update_id,
					.chat_id = msg["chat"]["id"].get<ulong>(),
					.message_id = msg["message_id"].get<ulong>(),
					.username = msg["from"]["username"].get<std::string_view>(),
					.text = msg["text"].get<std::string_view>()
				});
			}

			starting_from = std::max(starting_from, update_id);
		}

		return starting_from;
	}

	void reply(message const& msg, std::string_view text) const {
		if (auto encoded_text = curl_easy_escape(this->curl, text.data(), text.length())) {
			auto request = "sendMessage?chat_id=" + std::to_string(msg.chat_id) + "&reply_to_message_id=" + std::to_string(msg.message_id) + "&text=" + encoded_text;
			curl_free(encoded_text); // TODO: guard it?
			this->request(request);
		}
	}
};