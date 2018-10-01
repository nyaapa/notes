#include <iostream>
#include <string>

#include "json.hpp"
#include "cxxopts.hpp"

#include "curl/curl.h"

using json = nlohmann::json;
using namespace std::string_literals;

struct message {
	const ulong update_id;
	const ulong chat_id;
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

	explicit tcurl(std::string_view bot_key) noexcept : bot_key{bot_key}, base_address{"https://"s + api_host + "/bot"s + this->bot_key + "/"s}  {
		this->curl = curl_easy_init();
		if (!this->curl) {
			std::cerr << "CURL initialization failed" << std::endl;
			std::terminate();
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
	void process_updates(ulong starting_from, T process) const {
		auto updates = this->request("getUpdates?offset=" + std::to_string(starting_from));

		for (auto& update : updates) {
			auto& msg = update["message"];

			process(message {
				.update_id = update["update_id"].get<ulong>(),
				.chat_id = msg["chat"]["id"].get<ulong>(),
				.username = msg["from"]["username"].get<std::string_view>(),
				.text = msg["text"].get<std::string_view>()
			});
		}
	}
};

int main(int argc, char** argv) {
	try {
	    cxxopts::Options options("notes", "Notes telegram bot");

		std::string bot_key;
		ulong update_id{0};

		options
			.add_options()
			("b,bot", "bot key", cxxopts::value<std::string>(bot_key))
			("u,update-id", "starting update id", cxxopts::value<ulong>(update_id))
			("help", "Print help")
			;

		auto result = options.parse(argc, argv);

		if (result.count("help")) {
			std::cout << options.help({""}) << std::endl;
			exit(0);
		}

		tcurl th{bot_key};
		th.request("getMe");
		th.process_updates(update_id, [](message const& msg) {
			std::cout << "[" << msg.update_id << "] " << msg.username << " via " <<  msg.chat_id << ": " << msg.text << "\n";
		});
	} catch (std::exception& e) {
		std::cerr << "Unexpected exception: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Unexpected exception" << std::endl;
	}
}
