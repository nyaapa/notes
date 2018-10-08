#pragma once

#include <iostream>
#include <string>
#include <optional>
#include <string_view>

#include "sqlite3.h"

// TODO: move inside?
using namespace std::string_literals;
using namespace std::literals;

class sqlite {
	sqlite3 *dbh;

	void create_user(std::string_view username) const {
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

	~sqlite() {
		sqlite3_close(this->dbh);
	}

	sqlite(sqlite const&) = delete;
	sqlite(sqlite&&) = delete;

	sqlite& operator=(sqlite const&) = delete;
	sqlite& operator=(sqlite&&) = delete;

	long get_user_id(std::string_view username) const {
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
			return this->get_user_id(username);
		} else {
			sqlite3_finalize(stmt);
			throw std::runtime_error("SQL step error("s + std::to_string(res) + ") for "s + username.data());
		}
	}

	long add_note(std::string_view username, std::string_view note) const {
		auto id = this->get_user_id(username);
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

	std::optional<std::string> get_note(std::string_view username, ulong note_id) const {
		auto user_id = this->get_user_id(username);
		auto select = R"(
			select
				note
			from
				notes
			where
				note_id = )" + std::to_string(note_id) + R"(
			and
				user_id = )" + std::to_string(user_id);

		sqlite3_stmt *stmt{nullptr};
		if (auto res = sqlite3_prepare(this->dbh, select.data(), select.length(), &stmt, nullptr)) {
			throw std::runtime_error("SQL prepare error("s + std::to_string(res) + ")");
		}

		auto res = sqlite3_step(stmt);

		if (res == SQLITE_ROW) {
			std::string res{reinterpret_cast<char const*>(sqlite3_column_text(stmt, 0))}; // remove unsigned
			sqlite3_finalize(stmt);
			return res;
		} else if (res == SQLITE_DONE) {
			sqlite3_finalize(stmt);
			return {};
		} else {
			sqlite3_finalize(stmt);
			throw std::runtime_error("SQL step error("s + std::to_string(res) + ") for state init");
		}
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
};