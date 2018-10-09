#include "lexer.hpp"
#include <cstdlib>
#include <string>
#include <sstream>
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wsign-compare"

std::stringstream strbuf;

%%{
	machine lexer;
	alphtype unsigned long;
	write data;

	get_by_id := |*
		[0-9]+ => {
			auto id = std::stoul(std::string(ts, te)); // std::from_chars -> charconv:410: undefined reference to `__muloti4'
			result = std::make_unique<get>(msg, id);
			fnext main;
		};
	*|;

	ws = [ \t\n]+;

	add_note := |*
		any+ => {
			if (te == eof) {
				result = std::make_unique<add>(msg, std::string(ts, te));
				fnext main;
			}
		};
	*|;

	get_by_rx := |*
		'\\'[\\[\]()] => {
			strbuf << (char) ts[1];

			if (te == eof) {
				result = std::make_unique<like>(msg, std::string(strbuf.str()));
				fnext main;
			}
		};

		[^\\] => {
			strbuf << std::string(ts, te);

			if (te == eof) {
				result = std::make_unique<like>(msg, std::string(strbuf.str()));
				fnext main;
			}
		};
	*|;

	main := |*
		'/add' ws => {
			strbuf.str("");
			fgoto add_note;
		};

		'/get' ws => {
			strbuf.str("");
			fgoto get_by_id;
		};

		'/like' ws => {
			strbuf.str("");
			fgoto get_by_rx; 
		};
	*|;
}%%

std::unique_ptr<command> lexer::parse(message const& msg) {
	%%write init;

	// set up the buffer here
	p = msg.text.data();
	pe = p + msg.text.size();
	eof = pe;

	std::unique_ptr<command> result;

	%%write exec;
	
	if (cs == lexer_error) {
		return std::make_unique<error>(msg);
	}

	return result;
}