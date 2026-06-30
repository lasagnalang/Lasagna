#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "../ast/ast.hpp"

struct struct_member
{
    std::string name;
    std::string type;
    int offset;
    int width;
};

struct struct_info
{
    std::vector<struct_member> members;
    int total_size;
};

inline std::unordered_map<std::string, struct_info> struct_registry;

class code_generator
{
public:
    code_generator();

    void generate_program(const std::vector<node*>& program);

    std::string get_code() const;

private:
    void gen_function(node* n);
    void gen_block(node* n, std::ostream& dst);
    void gen_statement(node* n, std::ostream& dst);
    void gen_node(node* n, std::ostream& dst);
    void gen_struct_decl(node* n);

    void gen_address(node* n, std::ostream& dst, std::string& out_type);

    void gen_runtime_subsystem(std::ostream& dst);

    int get_type_width(const std::string& type_name);

    void emit(const std::string& op)
    {
        out << "    " << op << "\n";
    }

    void emit(const std::string& op, const std::string& arg1)
    {
        out << "    " << op << " " << arg1 << "\n";
    }

    void emit(const std::string& op, const std::string& arg1, const std::string& arg2)
    {
        out << "    " << op << " " << arg1 << ", " << arg2 << "\n";
    }

    void emit_label(const std::string& label)
    {
        out << label << ":\n";
    }

    std::string find_external_name(const std::string& internal_name);

    int get_var_offset(const std::string& name) {
        for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
            if (it->count(name)) {
                return (*it)[name];
            }
        }
        return 0;
    }

    std::string get_var_type(const std::string& name) {
        for (auto it = type_stack.rbegin(); it != type_stack.rend(); ++it) {
            if (it->count(name)) {
                return (*it)[name];
            }
        }
        return "int";
    }

private:
    std::ostringstream out;

    std::vector<std::unordered_map<std::string, int>> scope_stack;
    std::vector<std::unordered_map<std::string, std::string>> type_stack;

    int current_stack_offset = 0;
    int label_count = 0;
    std::string current_function_name;

    std::vector<std::string> string_literals;
    int string_count = 0;
};