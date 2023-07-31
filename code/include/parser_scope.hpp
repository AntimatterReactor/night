#pragma once

#include "bytecode.hpp"
#include "value.hpp"

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

struct ParserVariable;
struct ParserFunction;

using scope_var_container   = std::unordered_map<std::string, ParserVariable>;
using scope_func_container  = std::unordered_map<std::string, ParserFunction>;

struct ParserVariable
{
	val::value_t type;
	bytecode_t id;
};

struct ParserFunction
{
	bytecode_t id;

	std::vector<std::string> param_names;
	std::vector<val::value_t> param_types;
	std::optional<val::value_t> rtn_type;
};

struct ParserScope
{
	static scope_func_container funcs;

	// curr_func is so that parse_return() can be type checked
	static scope_func_container::iterator curr_func;
	
	scope_var_container vars;

	// returns:
	//    empty string if successful
	//    error message if not successful
	std::string create_variable(std::string const& name, val::value_t type);
};