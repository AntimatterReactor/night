#include "front-end/front-end.hpp"
#include "back-end/interpreter.hpp"
#include "back-end/parser.hpp"
#include "back-end/lexer.hpp"
#include "back-end/token.hpp"

#include <iostream>
#include <string>
#include <vector>

void front_end(std::string_view file_name)
{
	Lexer lexer(file_name, true);
	Parser parser(lexer);

	Parser::ParserScope global_scope{};

	std::vector<Stmt> stmts;

	Token token = lexer.eat(true);
	while (!lexer.get_curr().feof())
	{
		stmts.push_back(parser.parse_statement(global_scope));

		if (lexer.get_curr().feol()) lexer.eat(true);
	}

	Interpreter::InterpreterScope interpret_scope{};

	Interpreter interpreter;
	interpreter.interpret_statements(interpret_scope, stmts);
}
