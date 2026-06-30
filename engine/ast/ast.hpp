#pragma once

#include <vector>
#include <string>

enum node_type
{
    node_int,
    node_float,
    node_char,
    node_string,

    node_var,
    node_index,
    node_member_access,

    node_not,
    node_negate,
    node_bitwise_not,
    node_bitwise_xor,
    node_dec,
    node_inc,
    node_address_of,
    node_reference,

    node_add,
    node_sub,
    node_mul,
    node_div,
    node_mod,
    node_lshift,
    node_rshift,

    node_and,
    node_or,
    node_bitwise_and,
    node_bitwise_or,

    node_eq,
    node_ne,
    node_gt,
    node_lt,
    node_ge,
    node_le,

    node_assign,
    node_add_assign,
    node_sub_assign,
    node_mul_assign,
    node_div_assign,
    node_mod_assign,

    node_block,
    node_parameters,
    node_param_decl,
    node_function_decl,
    node_call,
    node_return,
    node_if,
    node_while,
    node_string_len,

    node_struct_decl,
    node_struct_member,
    node_init_list,

    node_var_decl,
    node_int_var_decl,
    node_string_var_decl,
    node_bool_var_decl,
    node_double_var_decl,
    node_float_var_decl,
    node_char_var_decl,
    node_int8_var_decl,
    node_int16_var_decl,
    node_int32_var_decl,
    node_int64_var_decl,
    node_intptr_var_decl,
    node_uint8_var_decl,
    node_uint16_var_decl,
    node_uint32_var_decl,
    node_uint64_var_decl,
    node_uintptr_var_decl,

    node_array_decl,
    node_lvector_decl,
    node_lmap_decl,
    node_ltable_decl,
    node_custom_var_decl,

    node_noop,
    node_error
};

struct node
{
    node_type type;
    std::string name;
    std::string literal;
    std::string generic_type;
    int array_size;
    std::vector<node*> children;
};

class ast
{
public:
    node* make_node(node_type type, node* child1 = nullptr, node* child2 = nullptr);
    node* make_block(const std::vector<node*>& stmts);
    node* make_call_node(node* callable, const std::vector<node*>& args);
    void free_all();

private:
    std::vector<node*> nodes;
};