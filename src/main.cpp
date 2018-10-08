#include <iostream>
#include <string>
#include <exception>

#include "cxxopts.hpp"

#include "tcurl.hpp"
#include "sqlite.hpp"
#include "lexer.hpp"

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
			std::cout << "[" << msg.update_id << "] " << msg.username << " via " <<  msg.chat_id << ": " << msg.text << "\n";
			lexer{}.parse(msg)->process(th, dbh);
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
