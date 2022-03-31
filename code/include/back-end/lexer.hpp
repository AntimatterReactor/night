#pragma once

#include "token.hpp"

#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

// one lexer for each file
class Lexer
{
public:
	Lexer(std::string_view file_name, bool const main_file);

public:
	Token eat(bool const go_to_next_line);
	Token peek(bool const go_to_next_line);

	Token get_curr() const noexcept;
	Location get_loc() const noexcept;
	
private:
	bool next_token(const bool go_to_next_line) noexcept;
	bool next_line() noexcept;

	// when a string token has been scanned, this function is called to replace
	// escape strings with escape characters
	static constexpr char escape_char(const char c) noexcept
	{
		switch (c)
		{
#define CASE(x, s) case x: return s
		CASE('a', '\a');
		CASE('b', '\b');
		CASE('f', '\f');
		CASE('n', '\n');
		CASE('r', '\r');
		CASE('t', '\t');
		CASE('v', '\v');
		default: return c;
		}
#undef CASE
	}

private:
	std::ifstream code_file;
	Location loc;

	std::string code_line;
	std::size_t i;
	
	Token curr;

	static std::unordered_map<char, std::vector<std::pair<char, TokenType> > > const symbols;
	static std::unordered_map<std::string, TokenType> const keywords;
};
