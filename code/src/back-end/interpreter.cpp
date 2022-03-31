#include "back-end/interpreter.hpp"
#include "back-end/token.hpp"
#include "back-end/stmt.hpp"
#include "error.hpp"

#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <optional>
#include <cmath>
#include <string>
#include <vector>

Interpreter::Interpreter()
{
	night_funcs["input"] = {};
	night_funcs["int"] = {};
	night_funcs["float"] = {};
	night_funcs["str"] = {};
}

bool Interpreter::Data::is_num() const
{
	return type == Data::INT || type == Data::FLOAT;
}

std::string Interpreter::Data::to_str() const
{
	switch (type)
	{
	case T::BOOL:
		return "bool";
	case T::INT:
		return "int";
	case T::FLOAT:
		return "float";
	case T::STR:
		return "str";
	case T::ARR:
		return "arr";
	}

	throw std::runtime_error("Interpreter::Data::to_str(), missing type to string conversion");
}

void Interpreter::Data::print(Data const& data)
{
	switch (data.type)
	{
	case BOOL:
		std::cout << (std::get<bool>(data.val) ? "true" : "false");
		break;
	case INT:
		std::cout << std::get<int>(data.val);
		break;
	case FLOAT:
		std::cout << std::get<float>(data.val);
		break;
	case STR:
		std::cout << std::get<std::string>(data.val);
		break;
	case ARR: {
		auto& arr = std::get<std::vector<Data> >(data.val);

		std::cout << "[ ";
		for (int a = 0; a < (int)arr.size() - 1; ++a)
		{
			Data::print(arr[a]);
			std::cout << ", ";
		}

		if (!arr.empty())
		{
			Data::print(arr.back());
			std::cout << ' ';
		}

		std::cout << "]";

		break;
	}
	}

	std::cout.flush();
}

bool Interpreter::Data::compare_data(Data const& data1, Data const& data2)
{
	if (data1.type != data2.type)
		return false;

	switch (data1.type)
	{
	case Data::BOOL:
		return std::get<bool>(data1.val) == std::get<bool>(data2.val);
	case Data::INT:
		return std::get<int>(data1.val) == std::get<int>(data2.val);
	case Data::FLOAT:
		return std::get<float>(data1.val) == std::get<float>(data2.val);
	case Data::STR:
		return std::get<std::string>(data1.val) == std::get<std::string>(data2.val);
	case Data::ARR:
		return compare_array(data1, data2);
	default:
		throw std::runtime_error("Interpreter::Data::compare_data(), missing data type");
	}
}

bool Interpreter::Data::compare_array(Data const& data1, Data const& data2)
{
	auto& arr1 = std::get<std::vector<Data> >(data1.val);
	auto& arr2 = std::get<std::vector<Data> >(data2.val);

	if (arr1.size() != arr2.size())
		return false;

	for (std::size_t a = 0; a < arr1.size(); ++a)
	{
		if (!compare_data(arr1[a], arr2[a]))
			return false;
	}

	return true;
}

std::optional<Interpreter::Data> Interpreter::interpret_statements(
	InterpreterScope& upper_scope, std::vector<Stmt> const& stmts,
	NightVariableContainer* add_vars)
{
	InterpreterScope scope{ &upper_scope };
	scope.vars = add_vars == nullptr ? NightVariableContainer{} : *add_vars;

	for (auto const& stmt : stmts)
	{
		auto const rtn_val = interpret_statement(scope, stmt);

		// any new variables created in `interpret_statement` gets added to
		// `add_vars` to be used by the caller
		if (add_vars != nullptr)
			*add_vars = scope.vars;

		if (rtn_val.has_value())
			return rtn_val;
	}

	return std::nullopt;
}

std::optional<Interpreter::Data> Interpreter::interpret_statement(
	InterpreterScope& scope, Stmt const& stmt)
{
	static long long const recursion_limit = 1000;
	static std::pair<NightFunctionContainer::const_iterator, int> recursion_calls = { {}, -1 };

	auto const& loc = stmt.loc;

	switch (stmt.type)
	{
	case StmtType::INIT: {
		auto const& stmt_init = std::get<StmtInit>(stmt.data);
		scope.vars[stmt_init.name] = { evaluate_expression(scope, stmt_init.expr) };
		
		return std::nullopt;
	}
	case StmtType::ASSIGN: {
		auto const& stmt_assign = std::get<StmtAssign>(stmt.data);

		auto chain = interpret_subscript_chain(scope, stmt_assign, loc);
		if (!chain.has_value())
			return std::nullopt;

		auto [curr_data, assign_data] = chain.value();

		switch (stmt_assign.type)
		{
		case StmtAssign::ASSIGN: {
			*curr_data = assign_data;
			break;
		}
		case StmtAssign::PLUS: {
			if (curr_data->type == Data::STR)
			{
				if (assign_data.type != Data::STR) {
					throw NIGHT_RUNTIME_ERROR(
						"value is type 'str' but expression is type '" + assign_data.to_str() + "'",
						"type 'str' can only be concatenated with type 'str'");
				}

				std::get<std::string>(curr_data->val) += std::get<std::string>(assign_data.val);
			}
			else if (curr_data->is_num())
			{
				if (!assign_data.is_num()) {
					throw NIGHT_RUNTIME_ERROR(
						"expression of type '" + assign_data.to_str() + "' can not be assigned using the assignment '+='",
						"assignment '+=' on that variable can only be used for expressions of type 'int' or 'float'");
				}

				if (curr_data->type == Data::INT)
				{
					std::get<int>(curr_data->val) += assign_data.type == Data::INT
						? std::get<int>(assign_data.val)
						: (int)std::get<float>(assign_data.val);
				}
				else
				{
					std::get<float>(curr_data->val) += assign_data.type == Data::INT
						? std::get<int>(assign_data.val)
						: std::get<float>(assign_data.val);

				}
			}
			else
			{
				throw NIGHT_RUNTIME_ERROR(
					"assignment operator '+=' can only be used on types 'int', 'float', or 'str'",
					"operator is currently being used on type '" + curr_data->to_str() + "'");
			}

			break;
		}
		case StmtAssign::MINUS:
			interpret_assignment(curr_data, assign_data, "-=", []<typename T>(T x, T y) -> T { return x - y; }, loc);
			break;
		case StmtAssign::TIMES:
			interpret_assignment(curr_data, assign_data, "*=", []<typename T>(T x, T y) -> T { return x * y; }, loc);
			break;
		case StmtAssign::DIVIDE:
			interpret_assignment(curr_data, assign_data, "/=", []<typename T>(T x, T y) -> T { return x / y; }, loc);
			break;
		case StmtAssign::MOD:
			interpret_assignment(curr_data, assign_data, "%=", []<typename T>(T x, T y) -> T { return (T)std::fmod(x, y); }, loc);
			break;
		}

		return std::nullopt;
	}
	case StmtType::IF: {
		auto const& stmt_if = std::get<StmtIf>(stmt.data);

		for (auto const& conditional : stmt_if.chains)
		{
			// if conditional is 'if' or 'elif'
			if (conditional.condition != nullptr)
			{
				// evaluate condition
				auto const condition_expr =
					evaluate_expression(scope, conditional.condition);

				if (condition_expr.type != Data::BOOL) {
					throw NIGHT_RUNTIME_ERROR(
						"if statement condition must be type 'bool'",
						"condition is currently type '" + condition_expr.to_str() + "'");
				}

				// if condition is not true, continue to next conditional
				if (!std::get<bool>(condition_expr.val))
					continue;
			}

			// if conditional is 'else', or if condition is true
			return interpret_statements(scope, conditional.body);
		}

		// if no conditional is true
		return std::nullopt;
	}
	case StmtType::FN: {
		StmtFn const& stmt_fn = std::get<StmtFn>(stmt.data);
		night_funcs[stmt_fn.name] = { stmt_fn.params, stmt_fn.body };

		return std::nullopt;
	}
	case StmtType::CALL: {
		auto& stmt_call = std::get<StmtCall>(stmt.data);

		// evaluate pre-defined functions first
		if (stmt_call.name == "print")
		{
			auto data = evaluate_expression(scope, stmt_call.args[0]);
			Data::print(data);

			return std::nullopt;
		}
		if (stmt_call.name == "input")
		{
			std::string user_input;
			getline(std::cin, user_input);

			return std::nullopt;
		}
		if (stmt_call.name == "system")
		{
			auto const arg = evaluate_expression(scope, stmt_call.args.at(0));

			if (arg.type != Data::STR) {
				throw NIGHT_RUNTIME_ERROR(
					"function call `system`, argument number 1, must be type `str`",
					"argument is currently type `" + arg.to_str() + "`");
			}

			std::system(std::get<std::string>(arg.val).c_str());

			return std::nullopt;
		}

		auto night_func = night_funcs.find(stmt_call.name);
		assert(night_func != night_funcs.end());

		auto vars = interpret_arguments(scope,
			night_func->second.params, stmt_call.args);

		// count number of recursive calls
		if (recursion_calls.second == -1)
		{
			recursion_calls = { night_funcs.find(stmt_call.name), 1 };
		}
		else if (stmt_call.name == recursion_calls.first->first)
		{
			recursion_calls.second++;
			if (recursion_calls.second > recursion_limit) {
				throw NIGHT_RUNTIME_ERROR(
					"function call `" + stmt_call.name + "` exceeds the recursion limit of " + std::to_string(recursion_limit),
					"");
			}
		}

		auto rtn_val = interpret_statements(scope, night_func->second.body, &vars);
		recursion_calls = { {}, -1 };
		
		return std::nullopt;
	}
	case StmtType::RETURN: {
		auto& stmt_rtn = std::get<StmtReturn>(stmt.data);

		return stmt_rtn.expr != nullptr
			? std::optional<Data>{ evaluate_expression(scope, stmt_rtn.expr) }
			: std::optional<Data>{ Data{} };
	}
	case StmtType::LOOP: {
		auto const& stmt_loop = std::get<StmtLoop>(stmt.data);
		
		InterpreterScope loop_scope{ &scope };
		for (auto const& section : stmt_loop.sections)
		{
			if (section.type == StmtLoopSectionType::INIT)
			{
				NightVariable const night_var{ evaluate_expression(scope, section.expr) };
				if (scope.get_var(section.it_name) != nullptr)
					scope.vars[section.it_name] = night_var;
				else
					loop_scope.vars[section.it_name] = night_var;
			}
		}

		for (int i = 0; true; ++i)
		{
			for (auto const& section : stmt_loop.sections)
			{
				if (section.type == StmtLoopSectionType::CONDITIONAL)
				{
					auto const condition = evaluate_expression(loop_scope, section.expr);
					if (condition.type != Data::BOOL) {
						throw NIGHT_RUNTIME_ERROR(
							"loop condition must be type 'bool'",
							"condition is currently type '" + condition.to_str() + "'");
					}

					if (!std::get<bool>(condition.val))
						goto STOP_LOOP;
				}
				else if (section.type == StmtLoopSectionType::RANGE)
				{
					auto const range = evaluate_expression(loop_scope, section.expr);
					if (range.type == Data::RNG)
					{
						if (pair_range->first + i == pair_range->second)
							goto STOP_LOOP;

						loop_scope.vars[section.it_name] = { Data{ Data::INT, pair_range->first + i } };
					}
					else if (range.type == Data::STR)
					{
						auto& str = std::get<std::string>(range.val);

						if (i == str.length())
							goto STOP_LOOP;

						loop_scope.vars[section.it_name] = { Data{ Data::STR, std::string(1, str[i]) } };
					}
					else if (range.type == Data::ARR)
					{
						auto& arr = std::get<std::vector<Data> >(range.val);

						if (i == arr.size())
							goto STOP_LOOP;

						loop_scope.vars[section.it_name] = { arr[i] };
					}
					else
					{
						throw NIGHT_RUNTIME_ERROR(
							"loop range must be type 'str', 'arr', or 'rng'",
							"range is currently type '" + range.to_str() + "'");
					}
				}
			}

			auto rtn_val = interpret_statements(scope, stmt_loop.body, &loop_scope.vars);

			// if body returns a value, stop the loop
			if (rtn_val.has_value())
				return rtn_val;
		}

		STOP_LOOP:;
		return std::nullopt;
	}
	case StmtType::METHOD: {
		auto const& method_stmt = std::get<StmtMethod>(stmt.data);
		evaluate_expression(scope, method_stmt.assign_expr);

		return std::nullopt;
	}
	default:
		return std::nullopt;
	}
}

std::optional<std::pair<Interpreter::Data*, Interpreter::Data> > Interpreter::interpret_subscript_chain(
	InterpreterScope& scope,
	StmtAssign const& stmt_assign,
	Location const& loc)
{
	auto const night_var = scope.get_var(stmt_assign.var_name);
	assert(night_var != nullptr);

	Data const assign_data = evaluate_expression(scope, stmt_assign.assign_expr);

	Data* curr_data = &night_var->second.data;
	for (std::size_t a = 0; a < stmt_assign.subscript_chain.size(); ++a)
	{
		// evaluate index
		Data const& index_data =
			evaluate_expression(scope, stmt_assign.subscript_chain[a]);

		if (index_data.type != Data::INT) {
			throw NIGHT_RUNTIME_ERROR(
				"subscript operator's index can only be type 'int'",
				"index is currently type '" + index_data.to_str() + "'");
		}

		// get integer value of index
		int const index = std::get<int>(index_data.val);

		if (index < 0) {
			throw NIGHT_RUNTIME_ERROR(
				"subscript operator can not contain a negative value",
				"operator can only be a non-negative integer");
		}


		// special case for strings
		if (curr_data->type == Data::STR)
		{
			auto& var_str = std::get<std::string>(curr_data->val);

			if (index >= (int)var_str.length()) {
				throw NIGHT_RUNTIME_ERROR(
					"subscript operator is out of range for string",
					"string length is " + std::to_string(var_str.length()));
			}
			if (stmt_assign.type != StmtAssign::ASSIGN) {
				throw NIGHT_RUNTIME_ERROR(
					"single characters in string can only be used with assignment operator", "");
			}

			auto const& assign_str = std::get<std::string>(assign_data.val);

			if (assign_str.length() != 1) {
				throw NIGHT_RUNTIME_ERROR(
					"characters can only be assigned to other characters",
					"character is currently assigned to string of length '" + std::to_string(assign_str.length()));
			}

			var_str[index] = assign_str[0];

			return std::nullopt;
		}

		if (curr_data->type != Data::ARR) {
			throw NIGHT_RUNTIME_ERROR(
				"subscript operator can only be used on type 'str' or 'arr'",
				"operator is currently used on type '" + curr_data->to_str() + "'");
		}

		auto& var_arr = std::get<std::vector<Data> >(curr_data->val);

		// check is index is out of bounds
		if (index >= (int)var_arr.size()) {
			throw NIGHT_RUNTIME_ERROR(
				"subscript operator is out of range for array",
				"array length is " + std::to_string(var_arr.size()));
		}

		curr_data = &var_arr[index];
	}

	return { { curr_data, assign_data } };
}

template <typename Operation>
void Interpreter::interpret_assignment(
	Data* const curr_data,
	Data const& assign_data,
	std::string const& op,
	Operation assign,
	Location const& loc)
{

	if (!curr_data->is_num()) {
		throw NIGHT_RUNTIME_ERROR(
			"value can not be assigned using the assignment '" + op + "'",
			"assignment '" + op + "' can only be used on variables of type 'int' or 'float'");
	}
	if (!assign_data.is_num()) {
		throw NIGHT_RUNTIME_ERROR(
			"expression of type '" + assign_data.to_str() + "' can not be assigned with assignment '" + op + "'",
			"assignment '" + op + "' can only assign expressions of type 'int' or 'float'");
	}

	float assign_num = assign_data.type == Data::INT
		? std::get<int>(assign_data.val)
		: std::get<float>(assign_data.val);

	if (curr_data->type == Data::INT)
		curr_data->val = assign(std::get<int>(curr_data->val), (int)assign_num);
	else
		curr_data->val = assign(std::get<float>(curr_data->val), assign_num);
}

Interpreter::NightVariableContainer Interpreter::interpret_arguments(
	InterpreterScope& scope,
	std::vector<std::string> const& param_names,
	ExprContainer const& param_exprs)
{
	assert(param_names.size() == param_exprs.size());

	NightVariableContainer vars;
	for (std::size_t a = 0; a < param_names.size(); ++a)
		vars[param_names[a]] = { evaluate_expression(scope, param_exprs[a]) };

	return vars;
}

Interpreter::Data Interpreter::evaluate_expression(
	InterpreterScope& scope,
	std::shared_ptr<ExprNode> const& expr)
{
	auto& loc = expr->loc;
	switch (expr->type)
	{
	case ExprNode::LITERAL: {
		auto& val = std::get<ValueLiteral>(expr->data);
		switch (val.type)
		{
		case ValueLiteral::BOOL:
			return { Data::BOOL, std::get<bool>(val.data) };
		case ValueLiteral::INT:
			return { Data::INT, std::get<int>(val.data) };
		case ValueLiteral::FLOAT:
			return { Data::FLOAT, std::get<float>(val.data) };
		case ValueLiteral::STR:
			return { Data::STR, std::get<std::string>(val.data) };
		}
	}
	case ExprNode::ARRAY: {
		auto const& arr = std::get<ValueArray>(expr->data);

		std::vector<Data> elem_data(arr.elem_exprs.size());
		for (std::size_t i = 0, k = 0; k < arr.elem_exprs.size(); ++i, ++k)
		{
			elem_data[i] = evaluate_expression(scope, arr.elem_exprs[k]);
			if (elem_data[i].type == Data::RNG)
			{
				elem_data.erase(elem_data.begin() + i);

				if (pair_range->first < pair_range->second)
				{
					for (int j = pair_range->first; j < pair_range->second; ++j, ++i)
						elem_data.insert(elem_data.begin() + i, Data{ Data::INT, j });
				}
				else
				{
					for (int j = pair_range->first - 1; j >= pair_range->second; --j, ++i)
						elem_data.insert(elem_data.begin() + i, Data{ Data::INT, j });
				}

				--i;
			}
		}

		return { Data::ARR, elem_data };
	}
	case ExprNode::VARIABLE: {
		auto& val = std::get<ValueVar>(expr->data);

		auto* const night_var = scope.get_var(val.name);
		assert(night_var != nullptr);

		return night_var->second.data;
	}
	case ExprNode::CALL: {
		auto const& val = std::get<ValueCall>(expr->data);
	
		auto const night_func = night_funcs.find(val.name);
		assert(night_func != night_funcs.end());

		if (night_func->first == "input")
		{
			std::string user_input;
			getline(std::cin, user_input);

			return Data{ Data::STR, user_input };
		}
		if (night_func->first == "int")
		{
			Data const param = evaluate_expression(scope, val.param_exprs.at(0));

			if (param.type == Data::INT)
			{
				return param;
			}
			if (param.type == Data::FLOAT)
			{
				return Data{ Data::INT, (int)std::get<float>(param.val) };
			}
			if (param.type == Data::STR)
			{
				try {
					return Data{ Data::INT, std::stoi(std::get<std::string>(param.val)) };
				}
				catch (std::invalid_argument const&) {
					throw NIGHT_RUNTIME_ERROR(
						"function call 'int', argument number 1, cannot be converted into type 'int'",
						"argument can only be a number in the form of a string");
				}
			}

			throw NIGHT_RUNTIME_ERROR(
				"function call 'int', argument number 1, is currently type '" + param.to_str() + "'",
				"argument can only be types 'int', 'float', or 'str'");
		}
		if (night_func->first == "float")
		{
			Data const param = evaluate_expression(scope, val.param_exprs.at(0));

			if (param.type == Data::INT)
			{
				return Data{ Data::INT, std::get<int>(param.val) };
			}
			if (param.type == Data::FLOAT)
			{
				return Data{ Data::FLOAT, std::get<float>(param.val) };
			}
			if (param.type == Data::STR)
			{
				try {
					return Data{ Data::FLOAT, std::stof(std::get<std::string>(param.val)) };
				}
				catch (std::invalid_argument const&) {
					throw NIGHT_RUNTIME_ERROR(
						"function call `float`, argument number 1, cannot be converted into type `float`",
						"argument can only be a number in the form of a string");
				}
			}

			throw NIGHT_RUNTIME_ERROR(
				"function call `int`, argument number 1, is currently type `" + param.to_str() + "`",
				"argument can only be types `int`, `float`, or `str`");
		}
		if (night_func->first == "str")
		{
			Data const param = evaluate_expression(scope, val.param_exprs.at(0));
			switch (param.type)
			{
			case Data::BOOL:
				return Data{ Data::STR, std::get<bool>(param.val) ? "true" : "false" };
			case Data::INT:
				return Data{ Data::STR, std::to_string(std::get<int>(param.val)) };
			case Data::FLOAT:
				return Data{ Data::STR, std::to_string(std::get<float>(param.val)) };
			case Data::STR:
				return param;
			case Data::ARR:
				throw NIGHT_RUNTIME_ERROR(
					"type 'arr' cannot be converted into type 'str'", "");
			}
		}
		if (night_func->first == "system")
		{
			auto const arg = evaluate_expression(scope, val.param_exprs.at(0));

			if (arg.type != Data::STR) {
				throw NIGHT_RUNTIME_ERROR(
					"function call `system`, argument number 1, must be type `str`",
					"argument is currently type `" + arg.to_str() + "`");
			}

			return Data{
				Data::INT,
				std::system(std::get<std::string>(arg.val).c_str())
			};
		}

		auto vars = interpret_arguments(scope,
			night_func->second.params, val.param_exprs);

		auto rtn_val = interpret_statements(scope, night_func->second.body, &vars);
		if (!rtn_val.has_value()) {
			throw NIGHT_RUNTIME_ERROR(
				"function call `" + val.name + "` does not return a value in expression",
				"functions must return a value when used in an expression");
		}

		return rtn_val.value();
	}
	case ExprNode::UNARY_OP: {
		auto const& unary_op = std::get<UnaryOPNode>(expr->data);
		
		if (unary_op.data == "-")
		{
			Data const value = evaluate_expression(scope, unary_op.value);
			if (!value.is_num()) {
				throw NIGHT_RUNTIME_ERROR(
					"left have value of operator `-` is currently type `" + value.to_str() + "`",
					"unary operator `-` can only be used on types `int or `float");
			}

			return value.type == Data::INT
				? Data{ value.type, -std::get<int>(value.val) }
				: Data{ value.type, -std::get<float>(value.val) };
		}
		if (unary_op.data == "!")
		{
			Data const value = evaluate_expression(scope, unary_op.value);
			if (value.type != Data::BOOL) {
				throw NIGHT_RUNTIME_ERROR(
					"operator  '!' is currently used on type '" + value.to_str() + "'",
					"operator '!' can only be used on type 'bool'");
			}

			return Data{ Data::BOOL, !std::get<bool>(value.val) };
		}
		if (unary_op.data == "[]")
		{
			Data const index_d = evaluate_expression(scope, unary_op.index);
			if (index_d.type != Data::INT) {
				throw NIGHT_RUNTIME_ERROR(
					"index for subscript operator must be type 'int'",
					"index is currently type '" + index_d.to_str() + "'");
			}

			int const index = std::get<int>(index_d.val);

			Data const array = evaluate_expression(scope, unary_op.value);
			if (array.type == Data::STR)
			{
				std::string const& str = std::get<std::string>(array.val);

				if (index < 0 || index >= (int)str.length()) {
					throw NIGHT_RUNTIME_ERROR(
						"index for subscript operator is out of range",
						"index " + std::to_string(index) + " is out of range for string length " + std::to_string(std::get<std::string>(array.val).length()) + "");
				}

				return Data{ Data::STR, std::string(1, str[(std::size_t)index]) };
			}
			if (array.type == Data::ARR)
			{
				std::vector<Data> const& arr = std::get<std::vector<Data>>(array.val);

				if (index < 0 || index >= (int)arr.size()) {
					throw NIGHT_RUNTIME_ERROR(
						"index for subscript operator is out of range",
						"index is value `" + std::to_string(index) + "` but array length is value `" + std::to_string(arr.size()) + "`");
				}

				return arr[(std::size_t)index];
			}
			
			throw NIGHT_RUNTIME_ERROR(
				"subscript operator can only be used on types `str` or `arr`",
				"subscript operator is currently used on type `" + array.to_str() + "`");
		}

		assert(false);
	}
	case ExprNode::BINARY_OP: {
		auto& binary_op = std::get<BinaryOPNode>(expr->data);

		switch (binary_op.type)
		{
		case BinaryOPNode::PLUS: {
			auto const left = evaluate_expression(scope, binary_op.left);
			auto const right = evaluate_expression(scope, binary_op.right);

			if (!left.is_num() && left.type != Data::STR && right.type != Data::STR) {
				throw NIGHT_RUNTIME_ERROR(
					"operator `+` can only be used on types `int`, `float`, or `str`",
					"left hand value of operator `+` currently is type `" + left.to_str() + "`");
			}

			if (!right.is_num() && right.type != Data::STR && left.type != Data::STR) {
				throw NIGHT_RUNTIME_ERROR(
					"operator `+` can only be used on types `int`, `float`, or `str`",
					"right hand value of operator `+` currently is type `" + right.to_str() + "`");
			}

			if (left.type == Data::STR && right.type == Data::STR)
			{
				return Data{ Data::STR,
					std::get<std::string>(left.val) + std::get<std::string>(right.val) };
			}

			if (left.type == Data::INT && right.type == Data::INT)
			{
				return Data{ Data::INT,
					std::get<int>(left.val) + std::get<int>(right.val) };
			}
			if (left.type == Data::FLOAT && right.type == Data::FLOAT)
			{
				return Data{ Data::FLOAT,
					std::get<float>(left.val) + std::get<float>(right.val) };
			}
			if (left.type == Data::FLOAT)
			{
				return Data{ Data::FLOAT,
					std::get<float>(left.val) + std::get<int>(right.val) };
			}
			if (right.type == Data::FLOAT)
			{
				return Data{ Data::FLOAT,
					std::get<int>(left.val) + std::get<float>(right.val) };
			}

			throw NIGHT_RUNTIME_ERROR(
				"operator `+` can only be used on types `int`, `float`, or two types both of `str`",
				"left hand value of the operator is currently type `" + left.to_str() + "`, and right hand value is currently type `" + right.to_str() + "`");
		}

		case BinaryOPNode::MINUS:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) -> T { return x - y; }, true);
		case BinaryOPNode::TIMES:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) -> T { return x * y; }, true);
		case BinaryOPNode::DIVIDE:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) -> T { return x / y; }, true);
		case BinaryOPNode::MOD:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) -> T { return (T)std::fmod(x, y); }, true);

		case BinaryOPNode::GREATER:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) { return x > y; }, false);
		case BinaryOPNode::SMALLER:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) { return x < y; }, false);
		case BinaryOPNode::GREATER_EQ:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) { return x >= y; }, false);
		case BinaryOPNode::SMALLER_EQ:
			return eval_expr_binary_num(scope, binary_op,
				[]<typename T>(T x, T y) { return x <= y; }, false);

		case BinaryOPNode::OR: {
			auto const& loc = binary_op.loc;

			auto const left = evaluate_expression(scope, binary_op.left);
			if (left.type != Data::BOOL) {
				throw NIGHT_RUNTIME_ERROR(
					"left hand value of operator '" + binary_op.data + "' has type '" + left.to_str() + "'",
					"operator can only be used on type 'bool'");
			}

			if (std::get<bool>(left.val))
				return Data{ Data::BOOL, true };

			auto const right = evaluate_expression(scope, binary_op.right);
			if (right.type != Data::BOOL) {
				throw NIGHT_RUNTIME_ERROR(
					"right hand value of operator '" + binary_op.data + "' has type '" + right.to_str() + "'",
					"operator can only be used on type 'bool'");
			}

			return Data{ Data::BOOL, std::get<bool>(right.val) };

		}
		case BinaryOPNode::AND: {
			auto const& loc = binary_op.loc;

			auto const left = evaluate_expression(scope, binary_op.left);
			if (left.type != Data::BOOL) {
				throw NIGHT_RUNTIME_ERROR(
					"left hand value of operator '" + binary_op.data + "' has type '" + left.to_str() + "'",
					"operator can only be used on type 'bool'");
			}

			if (!std::get<bool>(left.val))
				return Data{ Data::BOOL, false };

			auto const right = evaluate_expression(scope, binary_op.right);
			if (right.type != Data::BOOL) {
				throw NIGHT_RUNTIME_ERROR(
					"right hand value of operator '" + binary_op.data + "' has type '" + right.to_str() + "'",
					"operator can only be used on type 'bool'");
			}

			return Data{ Data::BOOL, std::get<bool>(right.val) };
		}

		case BinaryOPNode::EQUAL:
			return { Data::BOOL, eval_expr_binary_comp(scope, binary_op) };
		case BinaryOPNode::NOT_EQUAL:
			return { Data::BOOL, !eval_expr_binary_comp(scope, binary_op) };

		case BinaryOPNode::DOT: {
			Data object = evaluate_expression(scope, binary_op.left);
			if (object.type != Data::STR && object.type != Data::ARR) {
				throw NIGHT_RUNTIME_ERROR(
					"operator '" + binary_op.data + "' can only be used on objects",
					"operator is currently used on type '" + object.to_str() + "'");
			}

			auto& method = std::get<ValueCall>(binary_op.right->data);

			if (object.type == Data::ARR)
			{
				std::vector<Data>& obj_arr =
					std::get<std::vector<Data> >(object.val);

				if (method.name == "len")
				{
					return Data{ Data::INT, (int)obj_arr.size() };
				}
				if (method.name == "push" && method.param_exprs.size() == 1)
				{
					Data const value = evaluate_expression(scope, method.param_exprs[0]);
					obj_arr.push_back(value);

					auto* var = scope.get_var(std::get<ValueVar>(binary_op.left->data).name);
					assert(var != nullptr);

					var->second.data = object;

					return object;
				}
				if (method.name == "push" && method.param_exprs.size() == 2)
				{
					Data const value = evaluate_expression(scope, method.param_exprs[0]);
					Data const index = evaluate_expression(scope, method.param_exprs[1]);

					if (index.type != Data::INT) {
						throw NIGHT_RUNTIME_ERROR(
							"function call `" + method.name + "`, argument number `2` can only be type `int`",
							"argument is currently type `" + index.to_str() + "`");
					}

					obj_arr.insert(obj_arr.begin() + std::get<int>(index.val), value);

					return object;
				}
				if (method.name == "pop" && method.param_exprs.empty())
				{
					obj_arr.pop_back();
					return object;
				}
				if (method.name == "pop" && !method.param_exprs.size() == 1)
				{
					Data const index = evaluate_expression(scope, method.param_exprs[0]);
					if (index.type != Data::INT) {
						throw NIGHT_RUNTIME_ERROR(
							"index type is required to be type `int`",
							"index is currently type `" + index.to_str() + "`");
					}

					obj_arr.erase(obj_arr.begin() + std::get<int>(index.val));

					return object;
				}

				assert(false && "method exists in Parser, but not Interpreter");
			}
			if (object.type == Data::STR)
			{
				if (method.name == "len")
				{
					return Data{ Data::INT,
						(int)std::get<std::string>(object.val).length() };
				}

				assert(false && "method exists in Parser, but not Interpreter");
			}

			throw std::runtime_error("Interpreter::evaluate_expression(), missing dot operator method");
		}

		case BinaryOPNode::RANGE: {
			auto const left  = evaluate_expression(scope, binary_op.left);
			if (!left.is_num()) {
				throw NIGHT_RUNTIME_ERROR(
					"operator `..` can only be used on types `int` or `float`",
					"left hand value of operator currently is type `" + left.to_str() + "`");
			}

			auto const right = evaluate_expression(scope, binary_op.right);
			if (!right.is_num()) {
				throw NIGHT_RUNTIME_ERROR(
					"operator `..` can only be used on types `int` or `float`",
					"right hand value of operator currently is type `" + right.to_str() + "`");
			}

			if (left.type == Data::INT && right.type == Data::INT)
				pair_range = { std::get<int>(left.val), std::get<int>(right.val) };
			//if (left.type == Data::FLOAT && right.type == Data::FLOAT)
				//pair_range = { std::get<float>(left.val), std::get<float>(right.val) };
			//if (left.type == Data::FLOAT)
				//pair_range = { std::get<float>(left.val), std::get<int>(right.val) };
			//if (right.type == Data::FLOAT)
				//pair_range = { std::get<int>(left.val), std::get<float>(right.val) };

			return Data{ Data::RNG };
		}

		default:
			throw std::runtime_error("Interpreter::evaluate_expression(), missing BinaryOPNode type");
		}
	}
	default:
		throw std::runtime_error("Interpreter::evaluate_expression(), missing ExprNode type");
	}

}

template <typename Operation>
Interpreter::Data Interpreter::eval_expr_binary_num(
	InterpreterScope& scope,
	BinaryOPNode const& binary_op,
	Operation const& operation,
	bool num_rtn_type)
{
	auto const& loc = binary_op.loc;

	auto const lhs = evaluate_expression(scope, binary_op.left);
	if (!lhs.is_num()) {
		throw NIGHT_RUNTIME_ERROR(
			"binary operator '" + binary_op.data + "' can only be used on types 'int' or 'float'",
			"left hand value of operator is currently type '" + lhs.to_str() + "'");
	}

	auto const rhs = evaluate_expression(scope, binary_op.right);
	if (!rhs.is_num()) {
		throw NIGHT_RUNTIME_ERROR(
			"binary operator '" + binary_op.data + "' can only be used on types 'int' or 'float'",
			"right hand value of operator is currently type '" + lhs.to_str() + "'");
	}

	if (lhs.type == Data::INT && rhs.type == Data::INT)
		return Data{ num_rtn_type ? Data::INT : Data::BOOL, operation(std::get<int>(lhs.val), std::get<int>(rhs.val)) };
	if (lhs.type == Data::INT)
		return Data{ num_rtn_type ? Data::FLOAT : Data::BOOL, operation(std::get<int>(lhs.val), (int)std::get<float>(rhs.val)) };
	if (rhs.type == Data::INT)
		return Data{ num_rtn_type ? Data::FLOAT : Data::BOOL, operation(std::get<float>(lhs.val), (float)std::get<int>(rhs.val)) };
	return Data{ num_rtn_type ? Data::FLOAT : Data::BOOL, operation(std::get<float>(lhs.val), std::get<float>(rhs.val))};
}

template <typename Operation>
Interpreter::Data Interpreter::eval_expr_binary_bool(
	InterpreterScope& scope,
	BinaryOPNode const& binary_op,
	Operation const& operation)
{
	auto const& loc = binary_op.loc;

	auto const left = evaluate_expression(scope, binary_op.left);
	if (left.type != Data::BOOL) {
		throw NIGHT_RUNTIME_ERROR(
			"left hand value of operator '" + binary_op.data + "' has type '" + left.to_str() + "'",
			"operator can only be used on type 'bool'");
	}

	auto const right = evaluate_expression(scope, binary_op.right);
	if (right.type != Data::BOOL) {
		throw NIGHT_RUNTIME_ERROR(
			"right hand value of operator '" + binary_op.data + "' has type '" + right.to_str() + "'",
			"operator can only be used on type 'bool'");
	}

	return Data{
		Data::BOOL,
		operation(std::get<bool>(left.val), std::get<bool>(right.val))
	};
}

bool Interpreter::eval_expr_binary_comp(
	InterpreterScope& scope,
	BinaryOPNode const& binary_op)
{
	auto const& loc = binary_op.loc;

	auto const left  = evaluate_expression(scope, binary_op.left);
	auto const right = evaluate_expression(scope, binary_op.right);

	if (left.type != right.type) {
		throw NIGHT_RUNTIME_ERROR(
			"operator '" + binary_op.data + "' can only be used on values with the same type",
			"left hand value has type '" + left.to_str() + "' but right hand value has type '" + right.to_str() + "'");
	}

	return Data::compare_data(left, right);
}

std::optional<std::pair<int, int> > Interpreter::pair_range = std::nullopt;
