#include <iostream>
#include <string>
#include <iomanip>
#include <charconv>
#include <optional>

#include "cxxopts.hpp"

#include "tcurl.hpp"
#include "sqlite.hpp"

using namespace std::string_literals;
using namespace std::literals;

int main(int argc, char** argv) {
	try {
		cxxopts::Options options("notes", "Notes telegram bot");

		std::string bot_key;
		ulong update_id{0};
		bool init;

		options
			.add_options()
			("b,bot", "bot key", cxxopts::value<std::string>(bot_key))
			("u,update-id", "starting update id", cxxopts::value<ulong>(update_id))
			("i,init", "initialize db", cxxopts::value<bool>(init))
			("help", "Print help")
			;

		auto result = options.parse(argc, argv);

		if (result.count("help")) {
			std::cout << options.help({""}) << std::endl;
			exit(0);
		}

		sqlite dbh{};
		if (init) {
			dbh.init();
		}

		if (!result.count("update-id")) {
			update_id = dbh.get_last_update_id();
		} else {
			--update_id; // process_updates is excluding 
		}

		tcurl th{bot_key};
		th.request("getMe");
		update_id = th.process_updates(update_id, [&dbh, &th](message const& msg) {
			std::cout << "[" << msg.update_id << "] " << msg.username << " via " <<  msg.chat_id << ": " << msg.note << "\n";
			if (msg.note.substr(0, "/add "sv.length()) == "/add ") {
				auto id = dbh.add_note(msg.username, msg.note.substr("/add "sv.length()));
				th.reply(msg, "Added #" + std::to_string(id));
			} else if (msg.note.substr(0, "/get "sv.length()) == "/get ") {
				ulong note_id = std::stoul(msg.note.substr("/get "sv.length()).data()); // std::from_chars -> charconv:410: undefined reference to `__muloti4'
				if (auto note = dbh.get_note(msg.username, note_id)) {
					th.reply(msg, *note);
				} else {
					th.reply(msg, "No such message");
				}
			} else {
				th.reply(msg, "Can't parse the request");
			}
			// TODO: date + type?
			// TODO: delete notes
		});

		dbh.set_last_update_id(update_id);
	} catch (std::exception& e) {
		std::cerr << "Unexpected exception: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Unexpected exception" << std::endl;
	}
}
