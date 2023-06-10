#include "parser.hpp"
#include "lexer.hpp"
#include "scope.hpp"
#include "bytecode.hpp"
#include "expression.hpp"
#include "value.hpp"
#include "error.hpp"

#include <iostream>
#include <variant>
#include <string>
#include <assert.h>
#include <unordered_map>
#include <limits>

bytecodes_t parse_stmts(Lexer& lexer, Scope& upper_scope, bool* curly_enclosed)
{
	Scope scope{ upper_scope.vars };

	switch (lexer.eat().type)
	{
	case TokenType::OPEN_CURLY:
	{
		*curly_enclosed = true;

		bytecodes_t bytecodes;

		while (lexer.eat().type != TokenType::CLOSE_BRACKET)
		{
			if (lexer.curr().type == TokenType::END_OF_FILE)
				throw NIGHT_CREATE_FATAL("missing closing curly bracket");

			auto codes = parse_stmt(lexer, scope);
			bytecodes.insert(std::end(bytecodes), std::begin(codes), std::end(codes));
		}

		return bytecodes;
	}
	case TokenType::END_OF_FILE:
		return {};
	default:
		return parse_stmt(lexer, scope);
	}
}

bytecodes_t parse_stmt(Lexer& lexer, Scope& scope)
{
	assert(lexer.curr().type != TokenType::CLOSE_CURLY && "should be handled by caller");
	assert(lexer.curr().type != TokenType::END_OF_FILE && "should be handled by caller");

	bytecodes_t codes;

	switch (lexer.curr().type)
	{
	case TokenType::VARIABLE: parse_var(codes, lexer, scope); break;
	case TokenType::IF:		  return parse_if(lexer, scope, false);
	case TokenType::ELIF:	  return parse_if(lexer, scope, true);
	case TokenType::ELSE:	  return parse_else(lexer, scope);
	case TokenType::FOR:	  return parse_for(lexer, scope);
	case TokenType::WHILE:	  return parse_while(lexer, scope);
	case TokenType::DEF:	  return parse_func(lexer, scope);
	case TokenType::RETURN:	  return parse_rtn(lexer, scope);

	default: throw NIGHT_CREATE_FATAL("unknown syntax '" + lexer.curr().str + "'");
	}

	return codes;
}

void parse_var(bytecodes_t& codes, Lexer& lexer, Scope& scope)
{
	assert(lexer.curr().type == TokenType::VARIABLE);

	std::string const var_name = lexer.curr().str;

	lexer.eat();

	// cases:
	//   var int;
	//   var int = [expression];
	if (lexer.curr().is_type())
	{
		auto var_type = token_var_type_to_bytecode(lexer.curr().str);
		auto val_type = bytecode_type_to_val_type(var_type);

		scope.vars[var_name] = val_type;
		type_check::var_defined(lexer, scope, var_name);
		
		lexer.eat();

		if (lexer.curr().type == TokenType::SEMICOLON)
		{
			// case:
			//   var int;
			
			codes.push_back((bytecode_t)var_type);
			codes.push_back(0);
		}
		else if (lexer.curr().type == TokenType::ASSIGN)
		{
			// case:
			//   var int = [expression];

			auto expr = parse_toks_expr(lexer, scope);
			if (!expr)
				throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected expression after assignment");

			auto expr_type = parse_expr(expr, codes);
			if (expr_type != val_type)
				NIGHT_CREATE_MINOR("expression of type '" + val_type_to_str(val_type) + "' does not match with variable of type '" + bytecode_to_str(var_type) + "'");
		}
		else
		{
			throw NIGHT_CREATE_FATAL("expected semicolon or assignment after variable type");
		}

		codes.push_back((bytecode_t)BytecodeType::ASSIGN);
		codes.push_back(find_var_index(scope.vars, var_name));

		return;
	}

	if (lexer.curr().type != TokenType::ASSIGN)
	{
		throw NIGHT_CREATE_FATAL("expected assignment or variable type after variable name '" + var_name + "'");
	}

	parse_var_assign(lexer, scope, codes, var_name);
	if (lexer.curr().type != TokenType::SEMICOLON)
		throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "' expected variable type or assignment");
}

bytecodes_t parse_if(Lexer& lexer, Scope& scope, bool is_elif)
{
	assert(lexer.curr().type == TokenType::IF);

	bytecodes_t codes;

	// parse condition

	lexer.expect(TokenType::OPEN_BRACKET);

	auto cond_expr = parse_expr_toks(lexer, scope);
	auto cond_type = parse_expr(cond_expr, codes);

	lexer.expect(TokenType::CLOSE_BRACKET);

	// parse statements

	bool curly_enclosed = false;
	auto stmt_codes = parse_stmts(lexer, scope, &curly_enclosed);

	codes.push_back({ lexer.loc, is_elif ? BytecodeType::ELIF : BytecodeType::IF, (int)stmt_codes.size() });
	codes.insert(std::end(codes), std::begin(stmt_codes), std::end(stmt_codes));

	// error messages

	if (cond_type != ValueType::BOOL)
		NIGHT_CREATE_MINOR("condition of type '" + val_type_to_str(cond_type) + "', expected type 'bool'");

	if (!curly_enclosed && stmt_codes.empty())
		NIGHT_CREATE_MINOR("if statement missing body");

	return codes;
}

bytecodes_t parse_else(Lexer& lexer, Scope& scope)
{	
	bytecodes_t codes;

	auto stmt_codes = parse_stmts(lexer, scope);

	codes.push_back({ lexer.loc, BytecodeType::ELSE, (int)stmt_codes.size() });
	codes.insert(std::end(codes), std::begin(stmt_codes), std::end(stmt_codes));

	return codes;
}

bytecodes_t parse_for(Lexer& lexer, Scope& scope)
{
	assert(lexer.curr().type == TokenType::FOR);

	lexer.expect(TokenType::OPEN_BRACKET);

	bytecodes_t codes;

	// variable

	std::string var_name = lexer.eat().str;
	auto bytes = parse_var(lexer, scope);
	codes.insert(std::end(codes), std::begin(bytes), std::end(bytes));

	lexer.expect(TokenType::SEMICOLON);
	
	// condition

	auto cond = parse_expr_toks(lexer, scope);
	auto cond_type = parse_expr(cond, codes);

	lexer.expect(TokenType::SEMICOLON);

	// increment

	lexer.expect(TokenType::VARIABLE);
	lexer.expect(TokenType::ASSIGN);

	parse_var_assign(lexer, scope, codes, var_name);

	lexer.expect(TokenType::CLOSE_BRACKET);

	// statements

	auto stmt_codes = parse_stmts(lexer, scope);

	codes.push_back({ lexer.loc, BytecodeType::FOR, (int)stmt_codes.size() });
	codes.insert(std::end(codes), std::begin(stmt_codes), std::end(stmt_codes));

	// type checks

	type_check::var_defined(lexer, scope, var_name);

	if (cond_type != ValueType::BOOL)
	{
		NIGHT_CREATE_MINOR("found '" + val_type_to_str(cond_type) + "' expression, expected boolean expression");
	}

	return codes;
}

bytecodes_t parse_while(Lexer& lexer, Scope& scope)
{
	assert(lexer.curr().type == TokenType::WHILE);

	bytecodes_t codes;

	lexer.expect(TokenType::OPEN_BRACKET);

	auto cond_expr = parse_expr_toks(lexer, scope);
	auto cond_type = parse_expr(cond_expr, codes);

	lexer.expect(TokenType::CLOSE_BRACKET);

	// statements

	auto stmt_codes = parse_stmts(lexer, scope);
	codes.insert(std::end(codes), std::begin(stmt_codes), std::end(stmt_codes));

	if (cond_type != ValueType::BOOL)
	{
		NIGHT_CREATE_MINOR("found '" + val_type_to_str(cond_type) + "' expression, expected boolean expression");
	}

	return codes;
}

bytecodes_t parse_func(Lexer& lexer, Scope& scope)
{
	assert(lexer.curr().type == TokenType::DEF);

	auto const& func_name = lexer.expect(TokenType::VARIABLE).str;

	lexer.expect(TokenType::OPEN_BRACKET);

	bytecodes_t codes;

	// parameters

	parse_comma_sep_stmts(lexer, scope, codes);

	// return values
	
	lexer.eat();
	if (lexer.curr().type != TokenType::BOOL_TYPE ||
		lexer.curr().type != TokenType::CHAR_TYPE ||
		lexer.curr().type != TokenType::INT_TYPE ||
		lexer.curr().type != TokenType::STR_TYPE)
	{
		throw NIGHT_CREATE_FATAL("expected type after parameters");
	}

	TokenType rtn_type = lexer.curr().type;

	// body

	bool curly_enclosed = false;
	auto stmt_codes = parse_stmts(lexer, scope, &curly_enclosed);
	if (!curly_enclosed)
		NIGHT_CREATE_MINOR("function body must be enclosed by curly brackets");

	codes.insert(std::end(codes), std::begin(stmt_codes), std::end(stmt_codes));

	codes.push_back({ lexer.loc, BytecodeType::RETURN, 0 });

	if (scope.funcs.contains(func_name))
		NIGHT_CREATE_MINOR("function already defined");

	return codes;
}

bytecodes_t parse_rtn(Lexer& lexer, Scope& scope)
{
	bytecodes_t codes;

	auto expr = parse_expr_toks(lexer, scope);
	auto type = parse_expr(expr, codes);

	codes.push_back({ lexer.loc, BytecodeType::RETURN });

	return codes;
}

BytecodeType token_var_type_to_bytecode(std::string const& type)
{
	if (type == "int8")
		return BytecodeType::S_INT1;
	else if (type == "int16")
		return BytecodeType::S_INT2;
	else if (type == "int32")
		return BytecodeType::S_INT4;
	else if (type == "int64")
		return BytecodeType::S_INT8;
	else if (type == "uint8")
		return BytecodeType::U_INT1;
	else if (type == "uint16")
		return BytecodeType::U_INT2;
	else if (type == "uint32")
		return BytecodeType::U_INT4;
	else if (type == "uint64")
		return  BytecodeType::U_INT8;
	else
		night::unhandled_case(type);
}

void number_to_bytecode(std::string const& s_num, bytecodes_t& codes)
{
	assert(s_num.length());

	if (s_num[0] != '-')
	{
		assert(s_num.length() > 1);

		uint64_t uint64 = std::stoull(s_num);

		if (uint64 <= std::numeric_limits<uint8_t>::max())
			codes.push_back((bytecode_size)BytecodeType::U_INT1);
		else if (uint64 <= std::numeric_limits<uint16_t>::max())
			codes.push_back((bytecode_size)BytecodeType::U_INT2);
		else if (uint64 <= std::numeric_limits<uint32_t>::max())
			codes.push_back((bytecode_size)BytecodeType::U_INT4);
		else if (uint64 <= std::numeric_limits<uint64_t>::max())
			codes.push_back((bytecode_size)BytecodeType::U_INT8);
		else {}

		do {
			codes.push_back(uint64 & 0xFF);
		} while (uint64 >>= 8);
	}
	else
	{
		int64_t int64 = std::stoll(s_num);

		if (int64 <= std::numeric_limits<uint8_t>::max())
			codes.push_back((bytecode_size)BytecodeType::S_INT1);
		else if (int64 <= std::numeric_limits<uint16_t>::max())
			codes.push_back((bytecode_size)BytecodeType::S_INT2);
		else if (int64 <= std::numeric_limits<uint32_t>::max())
			codes.push_back((bytecode_size)BytecodeType::S_INT4);
		else if (int64 <= std::numeric_limits<uint64_t>::max())
			codes.push_back((bytecode_size)BytecodeType::S_INT8);
		else {}

		do {
			codes.push_back(int64 & 0xFF);
		} while (int64 >>= 8);
	}
}

int find_var_index(var_container const& vars, std::string const& var_name)
{
	return (int)std::distance(std::begin(vars), vars.find(var_name));
}

void parse_var_assign(Lexer& lexer, Scope& scope, bytecodes_t& codes, std::string const& var_name)
{
	assert(lexer.curr().type == TokenType::ASSIGN);
	
	BytecodeType assign_type;
	int var_index = (int)std::distance(std::begin(scope.vars), scope.vars.find(var_name));

	codes.push_back({ lexer.loc, BytecodeType::VARIABLE, var_index });

	if (lexer.curr().str == "+=")	   codes.push_back({ lexer.loc, BytecodeType::ADD });
	else if (lexer.curr().str == "-=") codes.push_back({ lexer.loc, BytecodeType::SUB });
	else if (lexer.curr().str == "*=") codes.push_back({ lexer.loc, BytecodeType::MULT });
	else if (lexer.curr().str == "/=") codes.push_back({ lexer.loc, BytecodeType::DIV });
	else if (lexer.curr().str != "=") throw std::runtime_error("parse_var_assign unhandled case " + lexer.curr().str);

	codes.push_back({ lexer.loc, BytecodeType::INT_ASSIGN, var_index });

	auto expr = parse_expr_toks(lexer, scope);
	if (!expr)
		throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected expression after assignment");

	auto expr_type = parse_expr(expr, codes);

	codes.push_back({ lexer.loc, assign_type, (int)std::distance(std::begin(scope.vars), scope.vars.find(var_name)) });


	type_check::var_undefined(lexer, scope, var_name);
	type_check::var_assign_type(lexer, scope, var_name, assign_type);
	type_check::var_expr_type(lexer, scope, var_name, expr_type);
}

void parse_comma_sep_stmts(Lexer& lexer, Scope& scope, bytecodes_t& codes)
{
	assert(lexer.curr().type == TokenType::OPEN_BRACKET);

	while (true)
	{
		auto const& var_name = lexer.expect(TokenType::VARIABLE).str;

		lexer.eat();
		if (lexer.curr().type == TokenType::BOOL_TYPE ||
			lexer.curr().type == TokenType::CHAR_TYPE ||
			lexer.curr().type == TokenType::INT_TYPE)
		{
			BytecodeType var_type;
			ValueType val_type;
			switch (lexer.curr().type)
			{
			case TokenType::BOOL_TYPE:
				var_type = BytecodeType::BOOL_ASSIGN;
				val_type = ValueType::BOOL;
				break;
			case TokenType::CHAR_TYPE:
				var_type = BytecodeType::CHAR_ASSIGN;
				val_type = ValueType::CHAR;
				break;
			case TokenType::INT_TYPE:
				var_type = BytecodeType::INT_ASSIGN;
				val_type = ValueType::INT;
				break;
			default:
				throw NIGHT_CREATE_FATAL("found '" + lexer.curr().str + "', expected variable type or assignment");
			}

			Scope func_scope;
			func_scope.vars[var_name] = val_type;

			  NIGHT_CREATE_FATAL("expected variable type");
		}
	}

	if (lexer.curr().type != TokenType::CLOSE_BRACKET)
	{

	}
}

expr_p parse_toks_expr(Lexer& lexer, Scope& scope, bool bracket)
{
	expr_p head(nullptr);
	bool prev_is_var = false;

	while (true)
	{
		switch (lexer.eat().type)
		{
		case TokenType::CHAR_LIT:
		{
			auto val = std::make_shared<ExprValue>(ValueType::CHAR, lexer.curr().str[0]);
			parse_expr_single(head, val);

			break;
		}
		case TokenType::INT_LIT:
		{
			auto val = std::make_shared<ExprValue>(ValueType::INT, std::stoi(lexer.curr().str));
			parse_expr_single(head, val);

			break;
		}
		case TokenType::VARIABLE:
		{
			prev_is_var = true;
			auto val = std::make_shared<ExprVar>(lexer.curr().str);
			parse_expr_single(head, val);

			break;
		}
		case TokenType::UNARY_OP:
		{
			auto val = std::make_shared<ExprUnary>(str_to_unary_type(lexer.curr().str), nullptr);
			parse_expr_single(head, val);

			break;
		}
		case TokenType::BINARY_OP:
		{
			auto tok_type = str_to_binary_type(lexer.curr().str);

			assert(head);
			if (!head->next())
			{
				head = std::make_shared<ExprBinary>(tok_type, head, nullptr);
			}
			else
			{
				expr_p curr(head);

				assert(curr->next());
				while (curr->next()->next() && prec(tok_type) >= curr->next()->prec())
					curr = curr->next();

				if (curr == head && curr->prec() >= prec(tok_type))
					head = std::make_shared<ExprBinary>(tok_type, head, nullptr);
				else
					curr->next() = std::make_shared<ExprBinary>(tok_type, curr->next(), nullptr);
			}

			break;
		}
		case TokenType::OPEN_BRACKET:
		{
			auto val = parse_expr_toks(lexer, scope, true);
			val->set_guard();

			parse_expr_single(head, val);

			break;
		}
		case TokenType::CLOSE_BRACKET:
		{
			return head;
		}
		default:
			return head;
		}
	}
}

ValueType expr_type_check(expr_p const& expr)
{
	switch (expr->type)
	{
	case ExprType::VALUE:
		return expr->val_type;
	case ExprType::UNARY:
		parse_expr(expr->lhs, codes);
		break;
	case ExprType::BINARY:
		parse_expr(expr->rhs, codes);
		parse_expr(expr->lhs, codes);
		break;
	default:
		throw std::runtime_error("parse_expr, missing case for ExprType '" + std::to_string((int)expr->type) + "'");
	}
}

ValueType parse_expr(expr_p const& expr, bytecodes_t& codes)
{
	assert(expr && "nullptr 'expr' should be handled by the caller");

	codes.push_back(expr->to_bytecode());

	switch (expr->type)
	{
	case ExprType::VALUE:
		break;
	case ExprType::UNARY:
		parse_expr(expr->lhs, codes);
		break;
	case ExprType::BINARY:
		parse_expr(expr->rhs, codes);
		parse_expr(expr->lhs, codes);
		break;
	default:
		throw std::runtime_error("parse_expr, missing case for ExprType '" + std::to_string((int)expr->type) + "'");
	}
}

void parse_expr_single(expr_p& head, expr_p const& val)
{
	if (!head)
	{
		head = val;
	}
	else
	{
		expr_p curr(head);
		while (curr->next())
			curr = curr->next();

		curr->next() = val;
	}
}

ExprUnaryType str_to_unary_type(std::string const& str)
{
	if (str == "!")
		return ExprUnaryType::NOT;

	throw std::runtime_error("str_to_unary_type, missing case for '" + str + "'");
}

ExprBinaryType str_to_binary_type(std::string const& str)
{
	if (str == "+")
		return ExprBinaryType::ADD;
	if (str == "-")
		return ExprBinaryType::SUB;
	if (str == "*")
		return ExprBinaryType::MULT;
	if (str == "/")
		return ExprBinaryType::DIV;

	throw std::runtime_error("str_to_binary_type, missing case for '" + str + "'");
}

void type_check::var_defined(Lexer const& lexer, Scope const& scope, std::string const& var_name)
{
	if (scope.vars.find(var_name) != std::end(scope.vars))
	{
		NIGHT_CREATE_MINOR("variable '" + var_name + "' is already defined");
	}
}

void type_check::var_undefined(Lexer const& lexer, Scope const& scope, std::string const& var_name)
{
	if (scope.vars.find(var_name) == std::end(scope.vars))
	{
		NIGHT_CREATE_MINOR("variable '" + var_name + "' is undefined");
	}
}

void type_check::var_assign_type(Lexer const& lexer, Scope& scope, std::string const& var_name, BytecodeType assign_type)
{
	switch (scope.vars[var_name])
	{
	case ValueType::INT:
		break;
	case ValueType::CHAR:
			NIGHT_CREATE_MINOR("variable '" + var_name + "' has type 'char', which is not compatable with operator '");
		break;
	default:
		throw std::runtime_error("typecheck::var_assign_type, unhandled case");
	}
}

void type_check::var_expr_type(Lexer const& lexer, Scope& scope, std::string const& var_name, ValueType expr_type)
{
	if (scope.vars[var_name] != expr_type)
	{
		NIGHT_CREATE_MINOR(std::string("expression of type '") + "expr_type" + "' is incompatable with variable '" + var_name + "' of type '" + "var_type");
	}
}