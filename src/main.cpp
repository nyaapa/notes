#include <iostream>
#include <string>
#include <iomanip>

#include "json.hpp"
#include "cxxopts.hpp"

#include "curl/curl.h"
#include "sqlite3.h"

using json = nlohmann::json;
using namespace std::string_literals;
using namespace std::literals;

struct message {
	const ulong update_id;
	const ulong chat_id;
	const ulong message_id;
	const std::string_view username;
	const std::string_view note;
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
			auto& msg = update["message"];

			auto update_id = update["update_id"].get<ulong>();
			process(message {
				.update_id = update_id,
				.chat_id = msg["chat"]["id"].get<ulong>(),
				.message_id = msg["message_id"].get<ulong>(),
				.username = msg["from"]["username"].get<std::string_view>(),
				.note = msg["text"].get<std::string_view>()
			});

			starting_from = std::max(starting_from, update_id);
		}

		return starting_from;
	}

	void reply(message const& msg, std::string_view text) const {
		if (auto encoded_text = curl_easy_escape(this->curl, text.data(), text.length())) {
			auto request = "sendMessage?chat_id=" + std::to_string(msg.chat_id) + "&reply_to_message_id=" + std::to_string(msg.message_id) + "&text=" + encoded_text;
			curl_free(encoded_text); // guard it?
			this->request(request);
		}
	}
};

class sqlite {
	sqlite3 *dbh;

	void create_user(std::string_view username) {
		std::string_view insert = R"(insert or ignore into users (name) values (?))";

		sqlite3_stmt *stmt{nullptr};
		if (auto res = sqlite3_prepare(this->dbh, insert.data(), insert.length(), &stmt, nullptr)) {
			throw std::runtime_error("SQL prepare error("s + std::to_string(res) + ")");
		}

		if (auto res = sqlite3_bind_text(stmt, 1, username.data(), -1, SQLITE_STATIC)) {
			throw std::runtime_error("SQL bind error("s + std::to_string(res) + ") of "s + username.data());
		}

		auto res = sqlite3_step(stmt);
		while (res == SQLITE_ROW) {
			res = sqlite3_step(stmt);
		}

		sqlite3_finalize(stmt);
		if (res != SQLITE_DONE) {
			throw std::runtime_error("SQL step error("s + std::to_string(res) + ") for "s + username.data());
		}
	}

public:
	explicit sqlite() : dbh{nullptr} {
		if (sqlite3_open("notes.db", &(this->dbh))) {
			throw std::runtime_error("Can't open database: "s + sqlite3_errmsg(this->dbh));
		}

		std::cout << "Opened database successfully\n";
	}

	long get_user_id(std::string_view username) {
		std::string_view select = R"(select user_id from users where name = ?)";

		sqlite3_stmt *stmt{nullptr};
		if (auto res = sqlite3_prepare(this->dbh, select.data(), select.length(), &stmt, nullptr)) {
			throw std::runtime_error("SQL prepare error("s + std::to_string(res) + ")");
		}

		if (auto res = sqlite3_bind_text(stmt, 1, username.data(), -1, SQLITE_STATIC)) {
			throw std::runtime_error("SQL bind error("s + std::to_string(res) + ") of "s + username.data());
		}

		auto res = sqlite3_step(stmt);

		if (res == SQLITE_ROW) {
			auto res = sqlite3_column_int64(stmt, 0);
			sqlite3_finalize(stmt);
			return res;
		} else if (res == SQLITE_DONE) {
			sqlite3_finalize(stmt);
			this->create_user(username);
			return get_user_id(username);
		} else {
			sqlite3_finalize(stmt);
			throw std::runtime_error("SQL step error("s + std::to_string(res) + ") for "s + username.data());
		}
	}

	long add_note(std::string_view username, std::string_view note) {
		auto id = get_user_id(username);
		std::string insert = "insert into notes (user_id, note) values ("s + std::to_string(id) + ", ?)";

		sqlite3_stmt *stmt{nullptr};
		if (auto res = sqlite3_prepare(this->dbh, insert.data(), insert.length(), &stmt, nullptr)) {
			throw std::runtime_error("SQL prepare error("s + std::to_string(res) + ")");
		}

		if (auto res = sqlite3_bind_text(stmt, 1, note.data(), -1, SQLITE_STATIC)) {
			throw std::runtime_error("SQL bind error("s + std::to_string(res) + ") of "s + note.data());
		}

		auto res = sqlite3_step(stmt);
		while (res == SQLITE_ROW) {
			res = sqlite3_step(stmt);
		}

		sqlite3_finalize(stmt);
		if (res != SQLITE_DONE) {
			throw std::runtime_error("SQL step error("s + std::to_string(res) + ") for "s + note.data());
		}

		return sqlite3_last_insert_rowid(this->dbh);
	}

	void init() {
		auto notes = R"(
			create table if not exists notes (
				note_id integer primary key autoincrement,
				user_id int not null,
				note text not null
			)
		)";

		char *err{nullptr};

		if (sqlite3_exec(this->dbh, notes, nullptr, nullptr, &err)) {
			auto error = "SQL error: "s +  err;
			sqlite3_free(err);
			throw std::runtime_error(error);
		}

		auto notes_idx = R"(create index if not exists notes_user_id_idx ON notes (user_id))";

		if (sqlite3_exec(this->dbh, notes_idx, nullptr, nullptr, &err)) {
			auto error = "SQL error: "s +  err;
			sqlite3_free(err);
			throw std::runtime_error(error);
		}

		std::cout << "notes are in place\n";

		auto users = R"(
			create table if not exists users (
				user_id integer primary key autoincrement,
				name nvarchar(60) unique not null
			)
		)";

		if (sqlite3_exec(this->dbh, users, nullptr, nullptr, &err)) {
			auto error = "SQL error: "s +  err;
			sqlite3_free(err);
			throw std::runtime_error(error);
		}

		std::cout << "users are in place\n";

		auto state = R"(
			create table if not exists state (
				_unique integer primary key not null default 0,
				update_id integer
			)
		)";

		if (sqlite3_exec(this->dbh, state, nullptr, nullptr, &err)) {
			auto error = "SQL error: "s +  err;
			sqlite3_free(err);
			throw std::runtime_error(error);
		}

		auto init_state = R"(insert or ignore into state (update_id) values (0))";

		if (sqlite3_exec(this->dbh, init_state, nullptr, nullptr, &err)) {
			auto error = "SQL error: "s +  err;
			sqlite3_free(err);
			throw std::runtime_error(error);
		}

		std::cout << "state is in place\n";
	}

	ulong get_last_update_id() const {
		std::string_view select = R"(select update_id from state)";

		sqlite3_stmt *stmt{nullptr};
		if (auto res = sqlite3_prepare(this->dbh, select.data(), select.length(), &stmt, nullptr)) {
			throw std::runtime_error("SQL prepare error("s + std::to_string(res) + ")");
		}

		auto res = sqlite3_step(stmt);

		if (res == SQLITE_ROW) {
			auto res = sqlite3_column_int64(stmt, 0);
			sqlite3_finalize(stmt);
			return res;
		} else {
			sqlite3_finalize(stmt);
			throw std::runtime_error("SQL step error("s + std::to_string(res) + ") for state init");
		}
	}

	void set_last_update_id(ulong update_id) const {
		auto update_state = "update state set update_id = " + std::to_string(update_id);

		char *err{nullptr};
		if (sqlite3_exec(this->dbh, update_state.data(), nullptr, nullptr, &err)) {
			auto error = "SQL error: "s +  err;
			sqlite3_free(err);
			throw std::runtime_error(error);
		}
	}

	~sqlite() {
		sqlite3_close(this->dbh);
	}
};

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
				th.reply(msg, std::to_string(id));
			} else {
				th.reply(msg, "Can't parse the request");
			}
			// TODO: not every command
			// TODO: return notes
			// TODO: return note id
			// TODO: date + type?
			// TODO: store update id & skip it?
		});

		dbh.set_last_update_id(update_id);
	} catch (std::exception& e) {
		std::cerr << "Unexpected exception: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "Unexpected exception" << std::endl;
	}
}
