#include "parser.hpp"

parser::parser(std::vector<lasagna_token>& tokens, ast& tree)
    : tokens(tokens), tree(tree), pos(0)
{
}

lasagna_token& parser::peek(int offset)
{
    if (pos + offset >= tokens.size())
    {
        static lasagna_token end_tok = { las_end, "", 0, 0 };
        return end_tok;
    }
    return tokens[pos + offset];
}

lasagna_token& parser::get()
{
    if (pos >= tokens.size())
    {
        static lasagna_token end_tok = { las_end, "", 0, 0 };
        return end_tok;
    }
    return tokens[pos++];
}

bool parser::match(lasagna_type type)
{
    if (peek().type == type)
    {
        get();
        return true;
    }
    return false;
}

std::string parser::expect_identifier()
{
    if (peek().type == las_identifier)
    {
        return get().text;
    }
    return "";
}

node* parser::make_literal_node(node_type nt)
{
    node* n = tree.make_node(nt, nullptr, nullptr);
    n->literal = get().text;
    return n;
}

node* parser::parse_binary(node* (parser::* next_tier)(), const std::unordered_map<lasagna_type, node_type>& op_mappings)
{
    node* left = (this->*next_tier)();
    while (op_mappings.find(peek().type) != op_mappings.end())
    {
        lasagna_type op = get().type;
        node* right = (this->*next_tier)();
        left = tree.make_node(op_mappings.at(op), left, right);
    }
    return left;
}

bool parser::is_type_keyword(lasagna_type type)
{
    switch (type)
    {
    case las_kw_int:    case las_kw_string:  case las_kw_bool:
    case las_kw_double: case las_kw_float:   case las_kw_char:
    case las_kw_uint8:  case las_kw_uint16:  case las_kw_uint32:
    case las_kw_uint64: case las_kw_uintptr: case las_kw_int8:
    case las_kw_int16:  case las_kw_int32:   case las_kw_int64:
    case las_kw_intptr:
        return true;
    default:
        return false;
    }
}

bool parser::is_container_keyword(lasagna_type type)
{
    return (type == las_kw_lvector || type == las_kw_lmap || type == las_kw_ltable);
}

node_type parser::get_var_decl_type(lasagna_type type)
{
    static const std::unordered_map<lasagna_type, node_type> decl_map = {
        {las_kw_int, node_int_var_decl},       {las_kw_string, node_string_var_decl},
        {las_kw_bool, node_bool_var_decl},     {las_kw_double, node_double_var_decl},
        {las_kw_float, node_float_var_decl},   {las_kw_char, node_char_var_decl},
        {las_kw_uint8, node_uint8_var_decl},   {las_kw_uint16, node_uint16_var_decl},
        {las_kw_uint32, node_uint32_var_decl}, {las_kw_uint64, node_uint64_var_decl},
        {las_kw_uintptr, node_uintptr_var_decl},{las_kw_int8, node_int8_var_decl},
        {las_kw_int16, node_int16_var_decl},   {las_kw_int32, node_int32_var_decl},
        {las_kw_int64, node_int64_var_decl},   {las_kw_intptr, node_intptr_var_decl},
        {las_kw_lvector, node_lvector_decl},   {las_kw_lmap, node_lmap_decl},
        {las_kw_ltable, node_ltable_decl}
    };

    auto it = decl_map.find(type);
    return (it != decl_map.end()) ? it->second : node_error;
}

bool parser::is_valid_lvalue(node* n)
{
    if (!n) return false;
    return (n->type == node_var || n->type == node_member_access || n->type == node_index);
}

void parser::synchronize()
{
    get();
    while (peek().type != las_end)
    {
        if (peek().type == las_semicolon)
        {
            get();
            return;
        }

        switch (peek().type)
        {
        case las_kw_int: case las_kw_string: case las_kw_bool:
        case las_kw_char: case las_kw_float: case las_kw_double:
        case las_kw_struct: case las_kw_lvector: case las_kw_lmap:
        case las_kw_ltable:
        case las_fun: case las_return: case las_if:
        case las_while: case las_else:
            return;
        default:
            get();
        }
    }
}

std::vector<node*> parser::parse_program()
{
    std::vector<node*> result;
    while (peek().type != las_end)
    {
        node* stmt = parse_statement();
        if (stmt) result.push_back(stmt);
        else synchronize();
    }
    return result;
}

void parser::parse_import_statement()
{
    match(las_kw_import);

    std::string lib_name = expect_identifier();
    if (match(las_dot)) expect_identifier();

    match(las_colon);
    std::string alias_name = expect_identifier();
    match(las_semicolon);

    if (!alias_name.empty() && !lib_name.empty())
    {
        libr_aliases[alias_name] = lib_name;
        this->current_lib = alias_name;
        dynamic_imports[lib_name] = std::vector<extern_func>();
    }
}

node* parser::parse_struct_decl()
{
    match(las_kw_struct);
    std::string name = expect_identifier();
    if (name.empty()) return tree.make_node(node_error);

    node* s_decl = tree.make_node(node_struct_decl, nullptr, nullptr);
    s_decl->name = name;

    match(las_lbrace);
    while (peek().type != las_rbrace && peek().type != las_end)
    {
        lasagna_type t_type = peek().type;
        std::string mem_type_str = "";

        if (is_type_keyword(t_type) || is_container_keyword(t_type))
        {
            mem_type_str = get().text;
        }
        else if (t_type == las_identifier)
        {
            mem_type_str = get().text;
        }
        else
        {
            return tree.make_node(node_error);
        }

        std::string mem_name = expect_identifier();
        match(las_semicolon);

        node* member = tree.make_node(node_struct_member, nullptr, nullptr);
        member->name = mem_name;
        member->generic_type = mem_type_str;
        s_decl->children.push_back(member);
    }
    match(las_rbrace);

    return s_decl;
}

node* parser::parse_init_list()
{
    match(las_lbrace);
    node* list_node = tree.make_node(node_init_list, nullptr, nullptr);

    if (peek().type != las_rbrace)
    {
        do {
            list_node->children.push_back(parse_expression());
        } while (match(las_comma));
    }
    match(las_rbrace);
    return list_node;
}

node* parser::parse_statement()
{
    lasagna_token token_type = peek();

    if (token_type.type == las_kw_import)
    {
        parse_import_statement();
        return tree.make_node(node_noop, nullptr, nullptr);
    }

    if (token_type.type == las_kw_struct)
    {
        return parse_struct_decl();
    }

    if (is_type_keyword(token_type.type) || is_container_keyword(token_type.type) ||
        (token_type.type == las_identifier && peek(1).type == las_identifier))
    {
        std::string base_type_str = get().text;
        node_type decl_type = get_var_decl_type(token_type.type);
        std::string generic_param = "";

        if (decl_type == node_error && token_type.type == las_identifier)
        {
            decl_type = node_custom_var_decl;
        }

        if (match(las_less))
        {
            generic_param = get().text;
            match(las_greater);
        }

        std::string name = expect_identifier();
        if (name.empty()) return tree.make_node(node_error);

        bool is_array = false;
        int arr_size = 0;
        if (match(las_lbracket))
        {
            if (peek().type == las_int_literal)
            {
                arr_size = std::stoi(get().text);
            }
            match(las_rbracket);
            is_array = true;
            decl_type = node_array_decl;
        }

        node* init = nullptr;
        if (match(las_assign))
        {
            if (peek().type == las_lbrace)
            {
                init = parse_init_list();
            }
            else
            {
                init = parse_expression();
            }
        }
        match(las_semicolon);

        node* decl = tree.make_node(decl_type, init, nullptr);
        decl->name = name;
        decl->generic_type = generic_param.empty() ? base_type_str : generic_param;
        decl->array_size = arr_size;
        return decl;
    }

    if (match(las_if))
    {
        match(las_lparen);
        node* condition = parse_expression();
        match(las_rparen);

        node* then_block = (peek().type == las_lbrace) ? parse_block() : parse_statement();
        node* else_block = nullptr;

        if (match(las_else))
        {
            else_block = (peek().type == las_lbrace) ? parse_block() : parse_statement();
        }

        node* if_node = tree.make_node(node_if, condition, then_block);
        if (else_block) if_node->children.push_back(else_block);
        return if_node;
    }

    if (match(las_while))
    {
        match(las_lparen);
        node* condition = parse_expression();
        match(las_rparen);

        node* body_block = (peek().type == las_lbrace) ? parse_block() : parse_statement();
        return tree.make_node(node_while, condition, body_block);
    }

    if (peek().type == las_fun)    return parse_function();
    if (peek().type == las_return) return parse_return();

    node* expr = parse_expression();
    match(las_semicolon);
    return expr;
}

node* parser::parse_function()
{
    match(las_fun);
    std::string name = expect_identifier();
    if (name.empty()) return tree.make_node(node_error);

    match(las_lparen);
    node* params = tree.make_node(node_parameters, nullptr, nullptr);
    if (peek().type != las_rparen)
    {
        do {
            if (!is_type_keyword(peek().type) && peek().type != las_identifier) return tree.make_node(node_error);
            std::string type_name = get().text;
            std::string p_name = expect_identifier();
            if (p_name.empty()) return tree.make_node(node_error);

            node* param_node = tree.make_node(node_param_decl, nullptr, nullptr);
            param_node->name = p_name;
            param_node->generic_type = type_name;
            params->children.push_back(param_node);
        } while (match(las_comma));
    }
    match(las_rparen);

    bool was_in_func = in_function_body;
    in_function_body = true;
    node* body = parse_block();
    in_function_body = was_in_func;

    node* func_node = tree.make_node(node_function_decl, params, body);
    func_node->name = name;
    return func_node;
}

node* parser::parse_block()
{
    if (!match(las_lbrace)) return tree.make_node(node_error);

    std::vector<node*> stmts;
    while (peek().type != las_rbrace && peek().type != las_end)
    {
        node* stmt = parse_statement();
        if (stmt) stmts.push_back(stmt);
        else synchronize();
    }
    match(las_rbrace);
    return tree.make_block(stmts);
}

node* parser::parse_return()
{
    match(las_return);
    if (!in_function_body) return tree.make_node(node_error);

    node* expr = (!match(las_semicolon)) ? parse_expression() : nullptr;
    if (expr) match(las_semicolon);
    return tree.make_node(node_return, expr, nullptr);
}

node* parser::parse_expression()
{
    return parse_assignment();
}

node* parser::parse_assignment()
{
    node* left = parse_logical_or();

    static const std::unordered_map<lasagna_type, node_type> assign_ops = {
        {las_assign, node_assign},           {las_plus_assign, node_add_assign},
        {las_minus_assign, node_sub_assign}, {las_mul_assign, node_mul_assign},
        {las_div_assign, node_div_assign},   {las_mod_assign, node_mod_assign}
    };

    auto it = assign_ops.find(peek().type);
    if (it != assign_ops.end())
    {
        get();
        if (!is_valid_lvalue(left)) return tree.make_node(node_error);
        return tree.make_node(it->second, left, parse_assignment());
    }
    return left;
}

node* parser::parse_logical_or() { return parse_binary(&parser::parse_logical_and, { {las_or, node_or} }); }
node* parser::parse_logical_and() { return parse_binary(&parser::parse_bitwise_or, { {las_and, node_and} }); }
node* parser::parse_bitwise_or() { return parse_binary(&parser::parse_bitwise_xor, { {las_pipe, node_bitwise_or} }); }
node* parser::parse_bitwise_xor() { return parse_binary(&parser::parse_bitwise_and, { {las_caret, node_bitwise_xor} }); }
node* parser::parse_bitwise_and() { return parse_binary(&parser::parse_equality, { {las_ampersand, node_bitwise_and} }); }

node* parser::parse_equality()
{
    return parse_binary(&parser::parse_shift, {
        {las_equal_equal, node_eq}, {las_not_equal, node_ne}, {las_greater, node_gt},
        {las_less, node_lt},        {las_greater_equal, node_ge}, {las_less_equal, node_le}
        });
}

node* parser::parse_shift() { return parse_binary(&parser::parse_additive, { {las_lshift, node_lshift}, {las_rshift, node_rshift} }); }
node* parser::parse_additive() { return parse_binary(&parser::parse_term, { {las_plus, node_add}, {las_minus, node_sub} }); }
node* parser::parse_term() { return parse_binary(&parser::parse_postfix, { {las_mul, node_mul}, {las_div, node_div}, {las_mod, node_mod} }); }

node* parser::parse_postfix()
{
    node* expr = parse_unary();

    while (true)
    {
        if (match(las_lparen))
        {
            std::vector<node*> args;
            if (peek().type != las_rparen)
            {
                args.push_back(parse_expression());
                while (match(las_comma)) args.push_back(parse_expression());
            }
            match(las_rparen);
            expr = tree.make_call_node(expr, args);
        }
        else if (match(las_dot))
        {
            std::string member_name = expect_identifier();
            if (member_name.empty()) return tree.make_node(node_error);

            node* field_id = tree.make_node(node_var, nullptr, nullptr);
            field_id->name = member_name;
            expr = tree.make_node(node_member_access, expr, field_id);
        }
        else if (match(las_lbracket))
        {
            node* index_expr = parse_expression();
            match(las_rbracket);
            expr = tree.make_node(node_index, expr, index_expr);
        }
        else if (peek().type == las_decrement || peek().type == las_increment)
        {
            if (!is_valid_lvalue(expr)) return tree.make_node(node_error);
            lasagna_type op = get().type;
            expr = tree.make_node((op == las_decrement) ? node_dec : node_inc, expr, nullptr);
        }
        else break;
    }
    return expr;
}

node* parser::parse_unary()
{
    static const std::unordered_map<lasagna_type, node_type> unary_ops = {
        {las_not, node_not}, {las_minus, node_negate}, {las_tilde, node_bitwise_not},
        {las_increment, node_inc}, {las_decrement, node_dec}
    };

    auto it = unary_ops.find(peek().type);
    if (it != unary_ops.end())
    {
        node_type nt = it->second;
        get();
        node* operand = parse_unary();

        if ((nt == node_dec || nt == node_inc) && !is_valid_lvalue(operand))
        {
            return tree.make_node(node_error);
        }
        return tree.make_node(nt, operand, nullptr);
    }
    return parse_factor();
}

node* parser::parse_factor()
{
    switch (peek().type)
    {
    case las_int_literal:    return make_literal_node(node_int);
    case las_float_literal:  return make_literal_node(node_float);
    case las_char_literal:   return make_literal_node(node_char);
    case las_string_literal: return make_literal_node(node_string);

    case las_identifier: {
        node* n = tree.make_node(node_var, nullptr, nullptr);
        n->name = get().text;
        return n;
    }
    case las_lparen: {
        get();
        node* expr = parse_expression();
        match(las_rparen);
        return expr;
    }
    default:
        return tree.make_node(node_error);
    }
}