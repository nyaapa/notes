#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../src/lexer.hpp"

using ::testing::Return;

TEST(get_basic, once) {
	auto text = "/get 1";
	auto cmd = lexer{}.parse(message {
		.update_id = 0,
		.chat_id = 0,
		.message_id = 0,
		.username = nullptr,
		.text = text
	});

	EXPECT_EQ(cmd->to_string(), "get=1") << "Got wrong value";
}

TEST(get_error, once) {
	auto text = "/get 1s";
	auto cmd = lexer{}.parse(message {
		.update_id = 0,
		.chat_id = 0,
		.message_id = 0,
		.username = nullptr,
		.text = text
	});

	EXPECT_EQ(cmd->to_string(), "error") << "Got wrong value";
}

TEST(add_basic, once) {
	auto text = "/add weqwewqewe";
	auto cmd = lexer{}.parse(message {
		.update_id = 0,
		.chat_id = 0,
		.message_id = 0,
		.username = nullptr,
		.text = text
	});

	EXPECT_EQ(cmd->to_string(), "add=weqwewqewe") << "Got wrong value";
}

TEST(unknown, once) {
	auto text = "add weqwewqewe";
	auto cmd = lexer{}.parse(message {
		.update_id = 0,
		.chat_id = 0,
		.message_id = 0,
		.username = nullptr,
		.text = text
	});

	EXPECT_EQ(cmd->to_string(), "error") << "Got wrong value";
}