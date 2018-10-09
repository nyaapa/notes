#pragma once

#include <string>
#include <memory>

#include "tcurl.hpp"
#include "sqlite.hpp"

class command {
protected:
	message msg;
public:
	command(message const& msg) : msg{msg} {}
	virtual ~command() {}

	command(command const&) = delete;
	command(command&&) = delete;
	command& operator=(command const&) = delete;
	command& operator=(command&&) = delete;

	virtual void process(tcurl const&, sqlite const&) = 0;
	virtual std::string to_string() = 0;
};

class get : public command {
	ulong note_id;
public:
	get(message const& msg, ulong note_id) : command{msg}, note_id{note_id} {}

	get(get const&) = delete;
	get(get&&) = delete;
	get& operator=(get const&) = delete;
	get& operator=(get&&) = delete;

	virtual void process(tcurl const& th, sqlite const& dbh) override {
		if (auto note = dbh.get_note(msg.username, note_id)) {
			th.reply(msg, *note);
		} else {
			th.reply(msg, "No such message");
		}
	}

	virtual std::string to_string() override {
		return "get=" + std::to_string(note_id);
	}
};

struct add : public command {
protected:
	std::string text;
public:
	add(message const& msg, std::string text) : command{msg}, text{text} {}

	add(add const&) = delete;
	add(add&&) = delete;
	add& operator=(add const&) = delete;
	add& operator=(add&&) = delete;

	virtual void process(tcurl const& th, sqlite const& dbh) override {
		auto id = dbh.add_note(msg.username, text);
		th.reply(msg, "Added #" + std::to_string(id));
	}

	virtual std::string to_string() override {
		return "add=" + text;
	}
};

struct like : public command {
protected:
	std::string pattern;
public:
	like(message const& msg, std::string pattern) : command{msg}, pattern{pattern} {}

	like(like const&) = delete;
	like(like&&) = delete;
	like& operator=(like const&) = delete;
	like& operator=(like&&) = delete;

	virtual void process(tcurl const& th, sqlite const& dbh) override {
		// Like get for now
		ulong note_id = std::stoul(pattern);
		if (auto note = dbh.get_note(msg.username, note_id)) {
			th.reply(msg, *note);
		} else {
			th.reply(msg, "No such message");
		}
	}

	virtual std::string to_string() override {
		return "like=" + pattern;
	}
};

struct error : public command {
public:
	error(message const& msg) : command{msg} {}

	error(error const&) = delete;
	error(error&&) = delete;
	error& operator=(error const&) = delete;
	error& operator=(error&&) = delete;

	virtual void process(tcurl const& th, sqlite const&) override {
		th.reply(msg, "Can't parse the request");
	}

	virtual std::string to_string() override {
		return "error";
	}
};

class lexer {
	// buffer state
	const char* p, * pe, * eof;
	// current token
	const char* ts, * te;
	// machine state
	int act, cs;
public:
	std::unique_ptr<command> parse(message const&);
};