#include "front-end/front-end.hpp"
#include "back-end/interpreter.hpp"
#include "back-end/parser.hpp"
#include "back-end/lexer.hpp"
#include "back-end/token.hpp"
#include "error.hpp"
#include "cmakedef.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace night
{
	std::string const help_message =
" \
Usage: night <file>|<options>\n\
Options:\n\
    --help	Displays this message and exit\n\
    --version 	Displays night's current version\n\
";
}

void FrontEnd(int argc, char** argv)
{
	if (argc != 2) {
		throw NIGHT_PREPROCESS_ERROR(
			"invalid command line arguments",
			night::learn_run);
	}

	if (std::string(argv[1]) == "--help")
	{
		std::cout << night::help_message;
		return;
	}
	if (std::string(argv[1]) == "--version")
	{
		std::cout << "night v" << night_VERSION_MAJOR << "." << night_VERSION_MINOR << "." << night_VERSION_PATCH << "\n";
		return;
	}

	Lexer lexer(argv[1], true);
	Parser parser(lexer);

	Parser::ParserScope global_scope{ nullptr };

	std::vector<Stmt> stmts;

	auto token = lexer.eat(true);
	while (lexer.get_curr().type != TokenType::_EOF)
	{
		stmts.push_back(parser.parse_statement(global_scope));

		if (lexer.get_curr().type == TokenType::EOL)
			lexer.eat(true);
	}

	Interpreter::InterpreterScope interpret_scope{ nullptr };

	Interpreter interpreter;
	interpreter.interpret_statements(interpret_scope, stmts);
}
