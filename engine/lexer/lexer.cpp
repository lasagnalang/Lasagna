#include "lexer.hpp"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, lasagna_type> KEYWORDS = {
    {"int", las_kw_int}, {"string", las_kw_string}, {"bool", las_kw_bool},
    {"double", las_kw_double}, {"float", las_kw_float}, {"char", las_kw_char},
    {"uint8", las_kw_uint8}, {"uint16", las_kw_uint16}, {"uint32", las_kw_uint32},
    {"uint64", las_kw_uint64}, {"uintptr", las_kw_uintptr}, {"int8", las_kw_int8},
    {"int16", las_kw_int16}, {"int32", las_kw_int32}, {"int64", las_kw_int64},
    {"intptr", las_kw_intptr}, {"struct", las_kw_struct}, {"lvector", las_kw_lvector},
    {"lmap", las_kw_lmap}, {"ltable", las_kw_ltable}, {"func", las_fun},
    {"return", las_return}, {"import", las_kw_import}, {"using", las_kw_using},
    {"if", las_if}, {"while", las_while}, {"else", las_else}
};

std::vector<lasagna_token> lexer::tokenize(const std::string& code) {
    text = code;
    pos = 0;
    current_line = 1;
    current_column = 1;
    std::vector<lasagna_token> tokens;

    while (true) {
        lasagna_token tok = next_token();
        tokens.push_back(tok);
        if (tok.type == las_end) {
            break;
        }
    }

    return tokens;
}

char lexer::peek(int offset) {
    if (pos + offset >= text.length()) {
        return '\0';
    }
    return text[pos + offset];
}

char lexer::get() {
    if (pos >= text.length()) {
        return '\0';
    }
    char c = text[pos++];
    if (c == '\n') {
        current_line++;
        current_column = 1;
    }
    else {
        current_column++;
    }
    return c;
}

void lexer::skip_whitespace() {
    while (pos < text.length()) {
        char current = text[pos];

        if (current == ' ' || current == '\t' || current == '\r' || current == '\n') {
            get();
        }
        else if (current == '/' && peek(1) == '/') {
            get(); get();
            while (pos < text.length() && text[pos] != '\n') {
                get();
            }
        }
        else if (current == '/' && peek(1) == '*') {
            get(); get();
            while (pos < text.length()) {
                if (text[pos] == '*' && peek(1) == '/') {
                    get(); get();
                    break;
                }
                get();
            }
        }
        else {
            break;
        }
    }
}

bool lexer::match_next(char expected) {
    if (peek() == expected) {
        get();
        return true;
    }
    return false;
}

lasagna_token lexer::next_token() {
    skip_whitespace();

    size_t start_line = current_line;
    size_t start_column = current_column;

    if (pos >= text.length()) {
        return { las_end, "", start_line, start_column };
    }

    char current = peek();

    if (std::isalpha(current) || current == '_') {
        std::string raw_text = "";
        while (std::isalnum(peek()) || peek() == '_') {
            raw_text += get();
        }

        auto it = KEYWORDS.find(raw_text);
        lasagna_type type = (it != KEYWORDS.end()) ? it->second : las_identifier;

        return { type, raw_text, start_line, start_column };
    }

    if (std::isdigit(current)) {
        std::string raw_text = "";
        bool is_float = false;

        while (std::isdigit(peek())) {
            raw_text += get();
        }

        if (peek() == '.' && std::isdigit(peek(1))) {
            is_float = true;
            raw_text += get();
            while (std::isdigit(peek())) {
                raw_text += get();
            }
        }

        lasagna_type type = is_float ? las_float_literal : las_int_literal;
        return { type, raw_text, start_line, start_column };
    }

    if (current == '"') {
        std::string raw_text = "";
        raw_text += get();

        while (peek() != '"' && peek() != '\0') {
            if (peek() == '\\') {
                raw_text += get();
                if (peek() != '\0') {
                    raw_text += get();
                }
            }
            else {
                raw_text += get();
            }
        }

        if (peek() == '"') {
            raw_text += get();
            return { las_string_literal, raw_text, start_line, start_column };
        }

        return { las_error, raw_text, start_line, start_column };
    }

    if (current == '\'') {
        std::string raw_text = "";
        raw_text += get();

        size_t content_length = 0;
        if (peek() != '\'' && peek() != '\0') {
            if (peek() == '\\') {
                raw_text += get();
                if (peek() != '\0') {
                    raw_text += get();
                    content_length = 1;
                }
            }
            else {
                raw_text += get();
                content_length = 1;
            }
        }

        if (peek() == '\'' && content_length == 1) {
            raw_text += get();
            return { las_char_literal, raw_text, start_line, start_column };
        }

        while (peek() != '\'' && peek() != '\0') {
            raw_text += get();
        }
        if (peek() == '\'') {
            raw_text += get();
        }

        return { las_error, raw_text, start_line, start_column };
    }

    current = get();

    switch (current) {
    case '.': return { las_dot, ".", start_line, start_column };
    case '?': return { las_question, "?", start_line, start_column };
    case ';': return { las_semicolon, ";", start_line, start_column };
    case ':': return { las_colon, ":", start_line, start_column };
    case ',': return { las_comma, ",", start_line, start_column };
    case '~': return { las_tilde, "~", start_line, start_column };
    case '^': return { las_caret, "^", start_line, start_column };
    case '(': return { las_lparen, "(", start_line, start_column };
    case ')': return { las_rparen, ")", start_line, start_column };
    case '{': return { las_lbrace, "{", start_line, start_column };
    case '}': return { las_rbrace, "}", start_line, start_column };
    case '[': return { las_lbracket, "[", start_line, start_column };
    case ']': return { las_rbracket, "]", start_line, start_column };

    case '+':
        if (match_next('=')) return { las_plus_assign, "+=", start_line, start_column };
        if (match_next('+')) return { las_increment, "++", start_line, start_column };
        return { las_plus, "+", start_line, start_column };

    case '-':
        if (match_next('=')) return { las_minus_assign, "-=", start_line, start_column };
        if (match_next('-')) return { las_decrement, "--", start_line, start_column };
        if (match_next('>')) return { las_arrow, "->", start_line, start_column };
        return { las_minus, "-", start_line, start_column };

    case '*':
        if (match_next('=')) return { las_mul_assign, "*=", start_line, start_column };
        return { las_mul, "*", start_line, start_column };

    case '/':
        if (match_next('=')) return { las_div_assign, "/=", start_line, start_column };
        return { las_div, "/", start_line, start_column };

    case '%':
        if (match_next('=')) return { las_mod_assign, "%=", start_line, start_column };
        return { las_mod, "%", start_line, start_column };

    case '=':
        if (match_next('=')) return { las_equal_equal, "==", start_line, start_column };
        return { las_assign, "=", start_line, start_column };

    case '!':
        if (match_next('=')) return { las_not_equal, "!=", start_line, start_column };
        return { las_not, "!", start_line, start_column };

    case '>':
        if (match_next('=')) return { las_greater_equal, ">=", start_line, start_column };
        if (match_next('>')) return { las_rshift, ">>", start_line, start_column };
        return { las_greater, ">", start_line, start_column };

    case '<':
        if (match_next('=')) return { las_less_equal, "<=", start_line, start_column };
        if (match_next('<')) return { las_lshift, "<<", start_line, start_column };
        return { las_less, "<", start_line, start_column };

    case '&':
        if (match_next('&')) return { las_and, "&&", start_line, start_column };
        return { las_ampersand, "&", start_line, start_column };

    case '|':
        if (match_next('|')) return { las_or, "||", start_line, start_column };
        return { las_pipe, "|", start_line, start_column };
    }

    return { las_error, std::string(1, current), start_line, start_column };
}