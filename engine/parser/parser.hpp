#pragma once

#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include "../lexer/lexer.hpp"
#include "../ast/ast.hpp"

struct extern_func
{
    std::string int_name;
    std::string ext_name;
};

inline std::map<std::string, std::vector<extern_func>> dynamic_imports;
inline std::map<std::string, std::string> libr_aliases;

class parser
{
public:
    parser(std::vector<lasagna_token>& tokens, ast& tree);
    std::vector<node*> parse_program();

private:
    lasagna_token& peek(int offset = 0);
    lasagna_token& get();
    bool match(lasagna_type type);
    void synchronize();
    bool is_type_keyword(lasagna_type type);
    bool is_container_keyword(lasagna_type type);
    node_type get_var_decl_type(lasagna_type type);
    bool is_valid_lvalue(node* n);

    node* parse_statement();
    node* parse_function();
    node* parse_block();
    node* parse_return();
    node* parse_struct_decl();
    node* parse_init_list();

    node* parse_expression();
    node* parse_assignment();
    node* parse_logical_or();
    node* parse_logical_and();
    node* parse_bitwise_or();
    node* parse_bitwise_xor();
    node* parse_bitwise_and();
    node* parse_equality();
    node* parse_shift();
    node* parse_additive();
    node* parse_term();
    node* parse_postfix();
    node* parse_unary();
    node* parse_factor();

    void parse_import_statement();

    std::string expect_identifier();
    node* make_literal_node(node_type nt);
    node* parse_binary(node* (parser::* next_tier)(), const std::unordered_map<lasagna_type, node_type>& op_mappings);

private:
    std::vector<lasagna_token>& tokens;
    std::string current_lib;
    ast& tree;
    size_t pos;
    bool in_function_body = false;
};