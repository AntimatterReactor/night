#include "parser.hpp"
#include "lexer.hpp"
#include "parser_scope.hpp"
#include "bytecode.hpp"
#include "interpreter_scope.hpp"
#include "ast/ast.hpp"
#include "ast/expression.hpp"
#include "value_type.hpp"
#include "utils.hpp"
#include "error.hpp"
#include "debug.hpp"

#include <unordered_map>
#include <variant>
#include <string>
#include <assert.h>

AST_Block parse_file(std::string const& main_file)
{
	Lexer lexer(main_file);
	AST_Block stmts;

	while (lexer.curr().type != TokenType::END_OF_FILE)
	{
		auto stmt = parse_stmts(lexer, false);
		stmts.insert(std::end(stmts), std::begin(stmt), std::end(stmt));
	}

	return stmts;
}

AST_Block parse_stmts(Lexer& lexer, bool requires_curly)
{
	// two cases:
	//   { stmt1; stmt2; ... }
	//   stmt1;
	switch (lexer.curr().type)
	{
	case TokenType::OPEN_CURLY:
	{
		AST_Block stmts;

		lexer.eat();

		while (lexer.curr().type != TokenType::CLOSE_CURLY)
		{
			stmts.push_back(parse_stmt(lexer));

			if (lexer.curr().type == TokenType::END_OF_FILE)
				throw NIGHT_CREATE_FATAL("missing closing curly bracket");
		}

		lexer.eat();

		return stmts;
	}
	case TokenType::END_OF_FILE:
		return {};
	default:
		if (requires_curly)
			throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected opening curly bracket");

		return { parse_stmt(lexer) };
	}
}

std::shared_ptr<AST> parse_stmt(Lexer& lexer)
{
	switch (lexer.curr().type)
	{
	case TokenType::VARIABLE: return parse_var(lexer);
	case TokenType::IF:		  return std::make_shared<Conditional>(parse_if(lexer));
	case TokenType::ELIF:	  throw NIGHT_CREATE_FATAL("elif statement must come before an if or elif statement");
	case TokenType::ELSE:	  throw NIGHT_CREATE_FATAL("else statement must come before an if or elif statement");
	case TokenType::FOR:	  return std::make_shared<For>(parse_for(lexer));
	case TokenType::WHILE:	  return std::make_shared<While>(parse_while(lexer));
	case TokenType::DEF:	  return std::make_shared<Function>(parse_func(lexer));
	case TokenType::RETURN:	  return std::make_shared<Return>(parse_return(lexer));

	default: throw NIGHT_CREATE_FATAL("unknown syntax '" + lexer.curr().str + "'");
	}
}

std::shared_ptr<AST> parse_var(Lexer& lexer)
{
	std::string var_name = lexer.curr().str;

	switch (lexer.peek().type)
	{
	case TokenType::TYPE: {
		lexer.eat();

		auto const& ast = std::make_shared<VariableInit>(parse_var_init(lexer, var_name));

		lexer.eat();
		return ast;
	}
	case TokenType::ASSIGN: {
		lexer.eat();

		auto const& ast = std::make_shared<VariableAssign>(parse_var_assign(lexer, var_name));
		lexer.curr_check(TokenType::SEMICOLON);

		lexer.eat();
		return ast;
	}
	case TokenType::OPEN_SQUARE: {
		auto const& ast = std::make_shared<ArrayMethod>(parse_array_method(lexer, var_name));
		return ast;
	}
	case TokenType::OPEN_BRACKET: {
		lexer.eat();
		auto const& ast = std::make_shared<expr::FunctionCall>(parse_func_call(lexer, var_name));
		lexer.expect(TokenType::SEMICOLON);
		lexer.eat();
		return ast;
	}
	default:
		throw debug::unhandled_case((int)lexer.peek().type);
	}
}

VariableInit parse_var_init(Lexer& lexer, std::string const& var_name)
{
	assert(lexer.curr().type == TokenType::TYPE);

	ValueType var_type(token_var_type_to_val_type(lexer.curr().str));

	bool is_arr = false;
	std::vector<std::optional<expr::expr_p>> arr_sizes;
	while (lexer.eat().type == TokenType::OPEN_SQUARE)
	{
		arr_sizes.push_back(parse_expr(lexer, false));
		lexer.curr_check(TokenType::CLOSE_SQUARE);
		
		is_arr = true;
	}

	if (arr_sizes.size() > 255)
		throw NIGHT_CREATE_FATAL("RELAX WITH THE SUBSCRIPTS");

	var_type.dim = (int)arr_sizes.size();

	// default value
	expr::expr_p expr;
	if (is_arr)
		expr = std::make_shared<expr::Array>(lexer.loc, std::vector<expr::expr_p>());
	else
		expr = std::make_shared<expr::Value>(lexer.loc, var_type.type, "0");

	if (lexer.curr().type == TokenType::ASSIGN && lexer.curr().str == "=")
	{
		expr = parse_expr(lexer, true);
		lexer.curr_check(TokenType::SEMICOLON);
	}
	else if (lexer.curr().type == TokenType::ASSIGN && lexer.curr().str != "=")
	{
		throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected assignment '='")
	}
	else if (lexer.curr().type != TokenType::SEMICOLON)
	{
		throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "' expected semicolon or assignment after variable type");
	}

	return VariableInit(lexer.loc, var_name, var_type, arr_sizes, expr);
}

VariableAssign parse_var_assign(Lexer& lexer, std::string const& var_name)
{
	assert(lexer.curr().type == TokenType::ASSIGN);

	auto assign_op = lexer.curr().str;

	auto expr = parse_expr(lexer, true);

	return VariableAssign(lexer.loc, var_name, assign_op, expr);
}

ArrayMethod parse_array_method(Lexer& lexer, std::string const& var_name)
{
	assert(lexer.curr().type == TokenType::VARIABLE);

	std::vector<expr::expr_p> subscripts;

	while (lexer.eat().type == TokenType::OPEN_SQUARE)
	{
		subscripts.push_back(parse_expr(lexer, true));
		lexer.curr_check(TokenType::CLOSE_SQUARE);
	}


	if (lexer.curr().type == TokenType::SEMICOLON)
	{
		lexer.eat();
		return ArrayMethod(lexer.loc, var_name, subscripts, nullptr);
	}

	lexer.curr_check(TokenType::ASSIGN);

	auto assign_expr = parse_expr(lexer, true);
	lexer.curr_check(TokenType::SEMICOLON);

	lexer.eat();
	return ArrayMethod(lexer.loc, var_name, subscripts, assign_expr);
}

expr::FunctionCall parse_func_call(Lexer& lexer, std::string const& func_name)
{
	assert(lexer.curr().type == TokenType::OPEN_BRACKET);

	// parse argument expressions and types

	std::vector<expr::expr_p> arg_exprs;

	while (true)
	{
		auto expr = parse_expr(lexer, false);

		// case:
		//   func_call();
		if (!expr)
		{
			lexer.curr_check(TokenType::CLOSE_BRACKET);
			break;
		}

		arg_exprs.push_back(expr);

		if (lexer.curr().type == TokenType::CLOSE_BRACKET)
			break;

		lexer.curr_check(TokenType::COMMA);
	}

	return expr::FunctionCall(lexer.loc, func_name, arg_exprs);
}

Conditional parse_if(Lexer& lexer)
{
	assert(lexer.curr().type == TokenType::IF);

	std::vector<std::pair<expr::expr_p, AST_Block>> conditionals;

	do {
		// default for else statements
		expr::expr_p cond_expr =
			std::make_shared<expr::Value>(lexer.loc, ValueType::BOOL, "true");

		// parse condition
		if (lexer.curr().type != TokenType::ELSE)
		{
			lexer.expect(TokenType::OPEN_BRACKET);

			cond_expr = parse_expr(lexer, true);
			lexer.curr_check(TokenType::CLOSE_BRACKET);
		}

		lexer.eat();
		conditionals.push_back({ cond_expr, parse_stmts(lexer, false) });

	} while (lexer.curr().type == TokenType::IF	  ||
			 lexer.curr().type == TokenType::ELIF ||
			 lexer.curr().type == TokenType::ELSE);

	return Conditional(lexer.loc, conditionals);
}

While parse_while(Lexer& lexer)
{
	assert(lexer.curr().type == TokenType::WHILE);

	lexer.expect(TokenType::OPEN_BRACKET);

	auto cond_expr = parse_expr(lexer, true);
	
	lexer.curr_check(TokenType::CLOSE_BRACKET);

	lexer.eat();
	return While(lexer.loc, cond_expr, parse_stmts(lexer, false));
}

For parse_for(Lexer& lexer)
{
	assert(lexer.curr().type == TokenType::FOR);

	lexer.expect(TokenType::OPEN_BRACKET);

	// initialization

	auto var_init_name = lexer.expect(TokenType::VARIABLE).str;
	lexer.expect(TokenType::TYPE);

	auto var_init = parse_var_init(lexer, var_init_name);
	
	// condition

	auto cond_expr = parse_expr(lexer, true);
	lexer.curr_check(TokenType::SEMICOLON);

	// assignment

	std::string var_assign_name = lexer.expect(TokenType::VARIABLE).str;
	lexer.eat();

	auto var_assign = parse_var_assign(lexer, var_assign_name);
	lexer.curr_check(TokenType::CLOSE_BRACKET);

	// body

	lexer.eat();
	auto stmts = parse_stmts(lexer, false);

	// increment

	stmts.push_back(std::make_shared<VariableAssign>(var_assign));

	return For(lexer.loc, var_init, cond_expr, stmts);
}

Function parse_func(Lexer& lexer)
{
	assert(lexer.curr().type == TokenType::DEF);

	std::string func_name = lexer.expect(TokenType::VARIABLE).str;
	lexer.expect(TokenType::OPEN_BRACKET);

	// parse function header

	std::vector<std::string> param_names;
	std::vector<std::string> param_types;

	lexer.eat();
	while (true)
	{
		if (lexer.curr().type == TokenType::CLOSE_BRACKET)
			break;

		lexer.curr_check(TokenType::VARIABLE);
		param_names.push_back(lexer.curr().str);

		lexer.expect(TokenType::TYPE);

		param_types.push_back(lexer.curr().str);

		lexer.eat();

		if (lexer.curr().type == TokenType::CLOSE_BRACKET)
			break;

		lexer.curr_check(TokenType::COMMA);
		lexer.eat();
	}

	auto rtn_type = lexer.eat();

	if (rtn_type.type != TokenType::TYPE && rtn_type.type != TokenType::VOID)
		throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected return type");

	lexer.eat();
	auto body = parse_stmts(lexer, true);

	return Function(lexer.loc, func_name, param_names, param_types, rtn_type.str, body);
}

Return parse_return(Lexer& lexer)
{
	assert(lexer.curr().type == TokenType::RETURN);

	auto const& expr = parse_expr(lexer, false);
	lexer.curr_check(TokenType::SEMICOLON);

	lexer.eat();

	return Return(lexer.loc, expr);
}

expr::expr_p parse_expr(Lexer& lexer, bool err_on_empty)
{
	expr::expr_p head(nullptr);
	bool allow_unary_next = true;
	bool was_variable = false;

	while (true)
	{
		lexer.eat();

		expr::expr_p node(nullptr);

		auto curr = lexer.curr();

		if (curr.type == TokenType::TYPE)
			curr.type = TokenType::VARIABLE;

		switch (curr.type)
		{
		case TokenType::BOOL_LIT:
		{
			node = std::make_shared<expr::Value>(lexer.loc, ValueType::BOOL, lexer.curr().str);
			allow_unary_next = false;
			was_variable = false;
			break;
		}
		case TokenType::CHAR_LIT:
		{
			node = std::make_shared<expr::Value>(lexer.loc, ValueType::CHAR, lexer.curr().str);
			allow_unary_next = false;
			was_variable = false;
			break;
		}
		case TokenType::INT_LIT:
		{
			node = std::make_shared<expr::Value>(lexer.loc, ValueType::INT, lexer.curr().str);
			allow_unary_next = false;
			was_variable = false;
			break;
		}
		case TokenType::FLOAT_LIT:
		{
			node = std::make_shared<expr::Value>(lexer.loc, ValueType::FLOAT, lexer.curr().str);
			allow_unary_next = false;
			was_variable = false;
			break;
		}
		case TokenType::STRING_LIT:
		{
			node = std::make_shared<expr::Value>(lexer.loc, ValueType::STR, lexer.curr().str);
			allow_unary_next = false;
			was_variable = true;
			break;
		}
		case TokenType::VARIABLE:
		{
			auto var_name = lexer.curr().str;

			if (lexer.peek().type == TokenType::OPEN_BRACKET)
			{
				lexer.eat();
				node = std::make_shared<expr::FunctionCall>(parse_func_call(lexer, var_name));
			}
			else
				node = std::make_shared<expr::Variable>(lexer.loc, var_name);

			allow_unary_next = false;
			was_variable = true;
			break;
		}
		case TokenType::OPEN_SQUARE:
		{
			if (was_variable)
			{
				auto index_expr = parse_expr(lexer, true);
				lexer.curr_check(TokenType::CLOSE_SQUARE);

				node = std::make_shared<expr::BinaryOp>(lexer.loc, expr::BinaryOpType::SUBSCRIPT);
				node->insert_node(index_expr);

				allow_unary_next = false;
				was_variable = true;
			}
			else
			{
				std::vector<expr::expr_p> arr;
				while (true)
				{
					auto elem = parse_expr(lexer, false);
					if (!elem)
					{
						lexer.curr_check(TokenType::CLOSE_SQUARE);
						break;
					}

					arr.push_back(elem);

					if (lexer.curr().type == TokenType::CLOSE_SQUARE)
						break;

					lexer.curr_check(TokenType::COMMA);
				}

				node = std::make_shared<expr::Array>(lexer.loc, arr);
				allow_unary_next = false;
				was_variable = false;
			}

			break;
		}
		case TokenType::UNARY_OP:
		{
			node = std::make_shared<expr::UnaryOp>(lexer.loc, lexer.curr().str);
			allow_unary_next = true;
			break;
		}
		case TokenType::BINARY_OP:
		{
			if (allow_unary_next && lexer.curr().str == "-")
			{
				node = std::make_shared<expr::UnaryOp>(lexer.loc, lexer.curr().str);
			}
			else
			{
				node = std::make_shared<expr::BinaryOp>(lexer.loc, lexer.curr().str);
				allow_unary_next = true;
			}

			was_variable = false;

			break;
		}
		case TokenType::OPEN_BRACKET:
		{
			node = parse_expr(lexer, err_on_empty);
			lexer.curr_check(TokenType::CLOSE_BRACKET);

			node->guard = true;

			allow_unary_next = false;
			was_variable = false;
			break;
		}
		default:
		{
			if (err_on_empty && !head)
				throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected expression");

			return head;
		}
		}

		if (!head)
			head = node;
		else
			head->insert_node(node, &head);
	}
}