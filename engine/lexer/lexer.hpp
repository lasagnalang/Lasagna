#pragma once

#include <string>
#include <vector>
#include <iostream>

enum lasagna_type
{
    las_identifier,
    las_int_literal,
    las_float_literal,
    las_char_literal,
    las_string_literal,

    las_kw_int,
    las_kw_string,
    las_kw_bool,
    las_kw_double,
    las_kw_float,
    las_kw_char,
    las_kw_uint8,
    las_kw_uint16,
    las_kw_uint32,
    las_kw_uint64,
    las_kw_uintptr,
    las_kw_int8,
    las_kw_int16,
    las_kw_int32,
    las_kw_int64,
    las_kw_intptr,
    las_kw_struct,
    las_kw_lvector,
    las_kw_lmap,
    las_kw_ltable,
    las_fun,
    las_return,
    las_kw_import,
    las_kw_using,
    las_if,
    las_while,
    las_else,

    las_dot,
    las_arrow,
    las_question,
    las_semicolon,
    las_colon,
    las_comma,
    las_tilde,
    las_caret,

    las_lparen,
    las_rparen,
    las_lbrace,
    las_rbrace,
    las_lbracket,
    las_rbracket,

    las_assign,
    las_plus_assign,
    las_minus_assign,
    las_mul_assign,
    las_div_assign,
    las_mod_assign,

    las_increment,
    las_decrement,

    las_plus,
    las_minus,
    las_mul,
    las_div,
    las_mod,

    las_equal_equal,
    las_not_equal,
    las_greater,
    las_less,
    las_greater_equal,
    las_less_equal,

    las_not,
    las_and,
    las_or,
    las_ampersand,
    las_pipe,
    las_lshift,
    las_rshift,

    las_end,
    las_error
};

struct lasagna_token
{
    lasagna_type type;
    std::string text;
    size_t line;
    size_t column;
};

class lexer
{
public:
    std::vector<lasagna_token> tokenize(const std::string& code);

private:
    lasagna_token next_token();
    char peek(int offset = 0);
    char get();
    void skip_whitespace();
    bool match_next(char expected);

private:
    std::string text;
    size_t pos = 0;
    size_t current_line = 1;
    size_t current_column = 1;
};