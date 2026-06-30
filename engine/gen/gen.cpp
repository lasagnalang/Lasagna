#include "gen.hpp"
#include "../parser/parser.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>

code_generator::code_generator()
    : current_stack_offset(0), label_count(0), string_count(0)
{
}

std::string code_generator::get_code() const
{
    return out.str();
}

int code_generator::get_type_width(const std::string& type_name)
{
    if (type_name == "bool" || type_name == "char" || type_name == "int8" || type_name == "uint8")
        return 1;

    if (type_name == "int16" || type_name == "uint16")
        return 2;

    if (type_name == "int" || type_name == "int32" || type_name == "uint32" || type_name == "float")
        return 4;

    if (type_name == "int64" || type_name == "uint64" || type_name == "intptr" || type_name == "uintptr" || type_name == "string" || type_name == "double")
        return 8;

    auto it = struct_registry.find(type_name);

    if (it != struct_registry.end())
        return it->second.total_size;

    return 8;
}

void code_generator::gen_runtime_subsystem(std::ostream& dst)
{
    dst << "_las_las_strlen:\n";
    dst << "    push rbp\n";
    dst << "    mov rbp, rsp\n";
    dst << "    xor rax, rax\n";
    dst << ".loop:\n";
    dst << "    cmp byte [rcx + rax], 0\n";
    dst << "    je .done\n";
    dst << "    inc rax\n";
    dst << "    jmp .loop\n";
    dst << ".done:\n";
    dst << "    mov rsp, rbp\n";
    dst << "    pop rbp\n";
    dst << "    ret\n\n";
}

void code_generator::generate_program(const std::vector<node*>& program)
{
    auto& k32 = dynamic_imports["kernel32"];
    if (std::find_if(k32.begin(), k32.end(), [](const auto& f) { return f.int_name == "ExitProcess"; }) == k32.end()) {
        k32.push_back({ "ExitProcess", "ExitProcess" });
    }
    if (std::find_if(k32.begin(), k32.end(), [](const auto& f) { return f.int_name == "Sleep"; }) == k32.end()) {
        k32.push_back({ "Sleep", "Sleep" });
    }

    auto& msvcrt = dynamic_imports["msvcrt"];
    if (std::find_if(msvcrt.begin(), msvcrt.end(), [](const auto& f) { return f.int_name == "printf"; }) == msvcrt.end()) {
        msvcrt.push_back({ "printf", "printf" });
    }
    if (std::find_if(msvcrt.begin(), msvcrt.end(), [](const auto& f) { return f.int_name == "strlen"; }) == msvcrt.end()) {
        msvcrt.push_back({ "strlen", "strlen" });
    }

    for (node* n : program)
    {
        if (n && n->type == node_struct_decl)
        {
            gen_struct_decl(n);
        }
    }

    out << "format PE64 console\n";
    out << "entry start\n\n";
    out << "include 'INCLUDE/win64a.inc'\n\n";

    out << "section '.text' code readable executable\n\n";
    out << "start:\n";
    out << "    sub rsp, 40\n";
    out << "    call main\n";
    out << "    xor ecx, ecx\n";
    out << "    call [ExitProcess]\n\n";

    for (node* n : program)
    {
        if (n && n->type == node_function_decl)
        {
            gen_function(n);
        }
    }

    gen_runtime_subsystem(out);

    out << "section '.rdata' data readable\n";
    out << "    fmt_int db '%d', 10, 0\n";
    out << "    fmt_str db '%s', 10, 0\n";
    out << "    fmt_char db '%c', 10, 0\n";

    for (size_t i = 0; i < string_literals.size(); ++i)
    {
        std::string s = string_literals[i];

        if (s.length() >= 2 && s.front() == '"' && s.back() == '"') {
            s = s.substr(1, s.length() - 2);
        }

        out << "    _str_" << i << " db '";
        bool in_quotes = true;

        for (size_t j = 0; j < s.length(); ++j) {
            unsigned char c = s[j];

            if (c == '\\' && j + 1 < s.length() && s[j + 1] == 'n') {
                if (in_quotes) { out << "'"; in_quotes = false; }
                out << ", 10";
                j++;
            }
            else if (c < 32 || c == '\'' || c > 126) {
                if (in_quotes) { out << "'"; in_quotes = false; }
                out << ", " << (int)c;
            }
            else {
                if (!in_quotes) { out << ", '"; in_quotes = true; }
                out << c;
            }
        }
        out << (in_quotes ? "', 0\n" : ", 0\n");
    }
    out << "\n";

    out << "section '.idata' import data readable\n\n";
    out << "    library ";
    bool first_lib = true;
    for (auto const& [lib_name, funcs] : dynamic_imports) {
        if (funcs.empty()) continue;
        if (!first_lib) out << ",\\\n            ";
        out << lib_name << ", '" << lib_name << ".dll'";
        first_lib = false;
    }
    out << "\n\n";

    for (auto const& [lib_name, funcs] : dynamic_imports) {
        if (funcs.empty()) continue;
        out << "    import " << lib_name << ",\\\n";
        for (size_t i = 0; i < funcs.size(); ++i) {
            out << "           " << funcs[i].ext_name << ", '" << funcs[i].ext_name << "'";
            if (i < funcs.size() - 1) out << ",\\\n";
        }
        out << "\n\n";
    }
}

void code_generator::gen_struct_decl(node* n)
{
    struct_info info;
    info.total_size = 0;

    for (node* mem : n->children)
    {
        if (mem->type == node_struct_member)
        {
            struct_member sm;
            sm.name = mem->name;
            sm.type = mem->generic_type;
            sm.width = get_type_width(sm.type);

            int align = sm.width;
            if (align > 8) align = 8;
            if (info.total_size % align != 0) {
                info.total_size += align - (info.total_size % align);
            }

            sm.offset = info.total_size;
            info.total_size += sm.width;
            info.members.push_back(sm);
        }
    }

    if (info.total_size % 8 != 0) {
        info.total_size += 8 - (info.total_size % 8);
    }

    struct_registry[n->name] = info;
}

void code_generator::gen_function(node* n)
{
    scope_stack.clear();
    type_stack.clear();
    current_stack_offset = 0;

    scope_stack.push_back({});
    type_stack.push_back({});

    current_function_name = (n->name == "main") ? "main" : "_las_" + n->name;
    std::stringstream body_stream;

    if (!n->children.empty() && n->children[0]->type == node_parameters)
    {
        node* params_node = n->children[0];
        for (size_t i = 0; i < params_node->children.size(); ++i) {
            node* param = params_node->children[i];

            int p_width = get_type_width(param->generic_type);
            current_stack_offset += p_width;
            scope_stack.back()[param->name] = current_stack_offset;
            type_stack.back()[param->name] = param->generic_type;

            if (i == 0) body_stream << "    mov qword [rbp - " << current_stack_offset << "], rcx\n";
            else if (i == 1) body_stream << "    mov qword [rbp - " << current_stack_offset << "], rdx\n";
            else if (i == 2) body_stream << "    mov qword [rbp - " << current_stack_offset << "], r8\n";
            else if (i == 3) body_stream << "    mov qword [rbp - " << current_stack_offset << "], r9\n";
            else {
                int stack_param_offset = 16 + ((i - 4) * 8);
                body_stream << "    mov rax, qword [rbp + " << stack_param_offset << "]\n";
                body_stream << "    mov qword [rbp - " << current_stack_offset << "], rax\n";
            }
        }
    }

    if (n->children.size() >= 2 && n->children[1]->type == node_block)
    {
        gen_block(n->children[1], body_stream);
    }

    int needed_space = current_stack_offset;
    if (needed_space < 32) needed_space = 32;
    int remainder = needed_space % 16;
    if (remainder != 0) needed_space += (16 - remainder);

    out << current_function_name << ":\n";
    out << "    push rbp\n";
    out << "    mov rbp, rsp\n";
    out << "    sub rsp, " << needed_space << "\n";
    out << body_stream.str();
    out << "." << current_function_name << "_exit:\n";
    out << "    mov rsp, rbp\n";
    out << "    pop rbp\n";
    out << "    ret\n\n";
}

void code_generator::gen_block(node* n, std::ostream& dst)
{
    for (node* child : n->children)
    {
        gen_statement(child, dst);
    }
}

std::string code_generator::find_external_name(const std::string& internal_name)
{
    for (auto const& [lib_name, funcs] : dynamic_imports)
    {
        for (auto const& f : funcs)
        {
            if (f.int_name == internal_name) return f.ext_name;
        }
    }
    return "";
}

void code_generator::gen_statement(node* n, std::ostream& dst)
{
    if (!n || n->type == node_noop || n->type == node_error || n->type == node_struct_decl) return;

    switch (n->type)
    {
    case node_block:
        scope_stack.push_back({});
        type_stack.push_back({});
        gen_block(n, dst);
        scope_stack.pop_back();
        type_stack.pop_back();
        break;

    case node_if:
    {
        int current_label = label_count++;
        node* cond = n->children[0];

        if (cond->type == node_eq || cond->type == node_ne || cond->type == node_gt ||
            cond->type == node_lt || cond->type == node_ge || cond->type == node_le)
        {
            gen_node(cond->children[0], dst);
            dst << "    push rax\n";
            gen_node(cond->children[1], dst);
            dst << "    mov rcx, rax\n";
            dst << "    pop rax\n";
            dst << "    cmp rax, rcx\n";

            switch (cond->type) {
            case node_eq: dst << "    jne .L_else_" << current_label << "\n"; break;
            case node_ne: dst << "    je .L_else_" << current_label << "\n"; break;
            case node_gt: dst << "    jle .L_else_" << current_label << "\n"; break;
            case node_lt: dst << "    jge .L_else_" << current_label << "\n"; break;
            case node_ge: dst << "    jl .L_else_" << current_label << "\n"; break;
            case node_le: dst << "    jg .L_else_" << current_label << "\n"; break;
            default: break;
            }
        }
        else {
            gen_node(cond, dst);
            dst << "    test rax, rax\n";
            dst << "    je .L_else_" << current_label << "\n";
        }

        if (n->children.size() >= 2)
        {
            bool explicit_block = (n->children[1]->type == node_block);
            if (!explicit_block) { scope_stack.push_back({}); type_stack.push_back({}); }
            gen_statement(n->children[1], dst);
            if (!explicit_block) { scope_stack.pop_back(); type_stack.pop_back(); }
        }
        dst << "    jmp .L_end_" << current_label << "\n";
        dst << ".L_else_" << current_label << ":\n";

        if (n->children.size() >= 3)
        {
            bool explicit_block = (n->children[2]->type == node_block);
            if (!explicit_block) { scope_stack.push_back({}); type_stack.push_back({}); }
            gen_statement(n->children[2], dst);
            if (!explicit_block) { scope_stack.pop_back(); type_stack.pop_back(); }
        }
        dst << ".L_end_" << current_label << ":\n";
    }
    break;

    case node_while:
    {
        int current_label = label_count++;
        dst << ".L_while_start_" << current_label << ":\n";
        node* cond = n->children[0];

        if (cond->type == node_eq || cond->type == node_ne || cond->type == node_gt ||
            cond->type == node_lt || cond->type == node_ge || cond->type == node_le)
        {
            gen_node(cond->children[0], dst);
            dst << "    push rax\n";
            gen_node(cond->children[1], dst);
            dst << "    mov rcx, rax\n";
            dst << "    pop rax\n";
            dst << "    cmp rax, rcx\n";

            switch (cond->type) {
            case node_eq: dst << "    jne .L_while_end_" << current_label << "\n"; break;
            case node_ne: dst << "    je .L_while_end_" << current_label << "\n"; break;
            case node_gt: dst << "    jle .L_while_end_" << current_label << "\n"; break;
            case node_lt: dst << "    jge .L_while_end_" << current_label << "\n"; break;
            case node_ge: dst << "    jl .L_while_end_" << current_label << "\n"; break;
            case node_le: dst << "    jg .L_while_end_" << current_label << "\n"; break;
            default: break;
            }
        }
        else {
            gen_node(cond, dst);
            dst << "    test rax, rax\n";
            dst << "    je .L_while_end_" << current_label << "\n";
        }

        if (n->children.size() >= 2)
        {
            bool explicit_block = (n->children[1]->type == node_block);
            if (!explicit_block) { scope_stack.push_back({}); type_stack.push_back({}); }
            gen_statement(n->children[1], dst);
            if (!explicit_block) { scope_stack.pop_back(); type_stack.pop_back(); }
        }
        dst << "    jmp .L_while_start_" << current_label << "\n";
        dst << ".L_while_end_" << current_label << ":\n";
    }
    break;

    case node_array_decl:
    {
        int element_width = get_type_width(n->generic_type);
        int total_array_size = element_width * n->array_size;

        current_stack_offset += total_array_size;
        scope_stack.back()[n->name] = current_stack_offset;
        type_stack.back()[n->name] = n->generic_type + "_array";

        if (!n->children.empty() && n->children[0]->type == node_init_list) {
            node* list_node = n->children[0];
            int current_elem_offset = current_stack_offset;
            for (size_t i = 0; i < list_node->children.size() && i < (size_t)n->array_size; ++i) {
                gen_node(list_node->children[i], dst);
                if (element_width == 1) dst << "    mov byte [rbp - " << current_elem_offset << "], al\n";
                else if (element_width == 2) dst << "    mov word [rbp - " << current_elem_offset << "], ax\n";
                else if (element_width == 4) dst << "    mov dword [rbp - " << current_elem_offset << "], eax\n";
                else dst << "    mov qword [rbp - " << current_elem_offset << "], rax\n";
                current_elem_offset -= element_width;
            }
        }
    }
    break;

    case node_lvector_decl:
    case node_lmap_decl:
    case node_ltable_decl:
    case node_custom_var_decl:
    case node_var_decl: case node_int_var_decl: case node_intptr_var_decl:
    case node_uint64_var_decl: case node_string_var_decl: case node_double_var_decl:
    case node_float_var_decl: case node_bool_var_decl: case node_char_var_decl:
    case node_int8_var_decl: case node_int16_var_decl: case node_int32_var_decl:
    case node_int64_var_decl: case node_uint8_var_decl: case node_uint16_var_decl:
    case node_uint32_var_decl: case node_uintptr_var_decl:
    {
        std::string mapping_type = n->generic_type;
        if (mapping_type.empty()) {
            if (n->type == node_string_var_decl) mapping_type = "string";
            else if (n->type == node_double_var_decl) mapping_type = "double";
            else if (n->type == node_float_var_decl) mapping_type = "float";
            else if (n->type == node_bool_var_decl) mapping_type = "bool";
            else if (n->type == node_char_var_decl) mapping_type = "char";
            else if (n->type == node_int16_var_decl) mapping_type = "int16";
            else if (n->type == node_int32_var_decl) mapping_type = "int32";
            else mapping_type = "int64";
        }

        int width = get_type_width(mapping_type);
        if (n->type == node_lvector_decl || n->type == node_lmap_decl || n->type == node_ltable_decl) {
            width = 24;
        }

        current_stack_offset += width;
        scope_stack.back()[n->name] = current_stack_offset;
        type_stack.back()[n->name] = mapping_type;

        if (!n->children.empty()) {
            if (n->type == node_lvector_decl && n->children[0]->type == node_init_list) {
            }
            else {
                gen_node(n->children[0], dst);
                if (width == 1) dst << "    mov byte [rbp - " << current_stack_offset << "], al\n";
                else if (width == 2) dst << "    mov word [rbp - " << current_stack_offset << "], ax\n";
                else if (width == 4) dst << "    mov dword [rbp - " << current_stack_offset << "], eax\n";
                else if (width == 8) dst << "    mov qword [rbp - " << current_stack_offset << "], rax\n";
            }
        }
    }
    break;

    case node_return:
        if (!n->children.empty())
        {
            gen_node(n->children[0], dst);
        }
        dst << "    jmp ." << current_function_name << "_exit\n";
        break;

    case node_assign:
    case node_add_assign:
    case node_sub_assign:
    case node_mul_assign:
    case node_div_assign:
    case node_mod_assign:
    {
        node* left_target = n->children[0];

        if (left_target->type == node_var)
        {
            gen_node(n->children[1], dst);

            int offset = get_var_offset(left_target->name);
            std::string v_type = get_var_type(left_target->name);
            int width = get_type_width(v_type);

            if (n->type == node_assign) {
                if (width == 1) dst << "    mov byte [rbp - " << offset << "], al\n";
                else if (width == 2) dst << "    mov word [rbp - " << offset << "], ax\n";
                else if (width == 4) dst << "    mov dword [rbp - " << offset << "], eax\n";
                else dst << "    mov qword [rbp - " << offset << "], rax\n";
            }
            else {
                if (width == 1) dst << "    movzx rcx, byte [rbp - " << offset << "]\n";
                else if (width == 2) dst << "    movzx rcx, word [rbp - " << offset << "]\n";
                else if (width == 4) dst << "    mov ecx, dword [rbp - " << offset << "]\n";
                else dst << "    mov rcx, qword [rbp - " << offset << "]\n";

                if (n->type == node_add_assign) { dst << "    add rcx, rax\n"; }
                else if (n->type == node_sub_assign) { dst << "    sub rcx, rax\n"; }
                else if (n->type == node_mul_assign) { dst << "    imul rcx, rax\n"; }
                else if (n->type == node_div_assign || n->type == node_mod_assign) {
                    dst << "    xchg rax, rcx\n";
                    dst << "    cqo\n";
                    dst << "    idiv rcx\n";
                    if (n->type == node_mod_assign) {
                        dst << "    mov rcx, rdx\n";
                    }
                }

                if (width == 1) dst << "    mov byte [rbp - " << offset << "], cl\n";
                else if (width == 2) dst << "    mov word [rbp - " << offset << "], cx\n";
                else if (width == 4) dst << "    mov dword [rbp - " << offset << "], ecx\n";
                else dst << "    mov qword [rbp - " << offset << "], rcx\n";
            }
        }
        else if (left_target->type == node_member_access)
        {
            gen_node(n->children[1], dst);
            dst << "    push rax\n";

            node* base = left_target->children[0];
            int base_offset = get_var_offset(base->name);
            std::string base_type = get_var_type(base->name);
            std::string member_name = left_target->children[1]->name;

            int member_offset = 0;
            int width = 8;

            if (struct_registry.count(base_type)) {
                for (const auto& member : struct_registry[base_type].members) {
                    if (member.name == member_name) {
                        member_offset = member.offset;
                        width = member.width;
                        break;
                    }
                }
            }

            int final_offset = base_offset - member_offset;
            dst << "    pop rcx\n";

            if (n->type == node_assign) {
                if (width == 1)      dst << "    mov byte [rbp - " << final_offset << "], cl\n";
                else if (width == 2) dst << "    mov word [rbp - " << final_offset << "], cx\n";
                else if (width == 4) dst << "    mov dword [rbp - " << final_offset << "], ecx\n";
                else                 dst << "    mov qword [rbp - " << final_offset << "], rcx\n";
            }
            else {
                if (width == 1)      dst << "    movzx rax, byte [rbp - " << final_offset << "]\n";
                else if (width == 2) dst << "    movzx rax, word [rbp - " << final_offset << "]\n";
                else if (width == 4) dst << "    mov eax, dword [rbp - " << final_offset << "]\n";
                else                 dst << "    mov rax, qword [rbp - " << final_offset << "]\n";

                if (n->type == node_add_assign) { dst << "    add rax, rcx\n"; }
                else if (n->type == node_sub_assign) { dst << "    sub rax, rcx\n"; }
                else if (n->type == node_mul_assign) { dst << "    imul rax, rcx\n"; }
                else if (n->type == node_div_assign || n->type == node_mod_assign) {
                    dst << "    cqo\n";
                    dst << "    idiv rcx\n";
                    if (n->type == node_mod_assign) {
                        dst << "    mov rax, rdx\n";
                    }
                }

                if (width == 1)      dst << "    mov byte [rbp - " << final_offset << "], al\n";
                else if (width == 2) dst << "    mov word [rbp - " << final_offset << "], ax\n";
                else if (width == 4) dst << "    mov dword [rbp - " << final_offset << "], eax\n";
                else                 dst << "    mov qword [rbp - " << final_offset << "], rax\n";
            }
        }
    }
    break;

    case node_dec:
    case node_inc:
        gen_node(n, dst);
        break;

    default:
        gen_node(n, dst);
        break;
    }
}

void code_generator::gen_node(node* n, std::ostream& dst)
{
    if (!n) return;

    if (n->type == node_int)
    {
        dst << "    mov rax, " << std::stoll(n->literal) << "\n";
        return;
    }

    if (n->type == node_string)
    {
        int id = string_count++;
        string_literals.push_back(n->literal);
        dst << "    lea rax, [_str_" << id << "]\n";
        return;
    }

    if (n->type == node_char)
    {
        dst << "    mov rax, " << (int)(n->literal[0]) << "\n";
        return;
    }

    if (n->type == node_var)
    {
        int offset = get_var_offset(n->name);
        std::string v_type = get_var_type(n->name);
        int width = get_type_width(v_type);

        if (width == 1) {
            dst << "    xor rax, rax\n";
            dst << "    mov al, byte [rbp - " << offset << "]\n";
        }
        else if (width == 2) {
            dst << "    xor rax, rax\n";
            dst << "    mov ax, word [rbp - " << offset << "]\n";
        }
        else if (width == 4) {
            dst << "    xor rax, rax\n";
            dst << "    mov eax, dword [rbp - " << offset << "]\n";
        }
        else {
            dst << "    mov rax, qword [rbp - " << offset << "]\n";
        }
        return;
    }

    if (n->type == node_negate || n->type == node_not || n->type == node_bitwise_not)
    {
        gen_node(n->children[0], dst);
        if (n->type == node_negate) {
            dst << "    neg rax\n";
        }
        else if (n->type == node_bitwise_not) {
            dst << "    not rax\n";
        }
        else if (n->type == node_not) {
            dst << "    test rax, rax\n";
            dst << "    sete al\n";
            dst << "    movzx rax, al\n";
        }
        return;
    }

    if (n->type == node_inc || n->type == node_dec)
    {
        node* target = n->children[0];
        if (target->type == node_var) {
            int offset = get_var_offset(target->name);
            std::string v_type = get_var_type(target->name);
            int width = get_type_width(v_type);
            bool is_inc = (n->type == node_inc);

            if (width == 1) dst << "    movzx rax, byte [rbp - " << offset << "]\n";
            else if (width == 2) dst << "    movzx rax, word [rbp - " << offset << "]\n";
            else if (width == 4) dst << "    mov eax, dword [rbp - " << offset << "]\n";
            else dst << "    mov rax, qword [rbp - " << offset << "]\n";

            bool is_postfix = (n->literal == "postfix");
            if (is_postfix) {
                dst << "    push rax\n";
            }

            if (is_inc) dst << "    inc rax\n";
            else dst << "    dec rax\n";

            if (width == 1) dst << "    mov byte [rbp - " << offset << "], al\n";
            else if (width == 2) dst << "    mov word [rbp - " << offset << "], ax\n";
            else if (width == 4) dst << "    mov dword [rbp - " << offset << "], eax\n";
            else dst << "    mov qword [rbp - " << offset << "], rax\n";

            if (is_postfix) {
                dst << "    pop rax\n";
            }
        }
        return;
    }

    if (n->type == node_address_of || n->type == node_reference)
    {
        node* target = n->children[0];
        int offset = get_var_offset(target->name);
        dst << "    lea rax, [rbp - " << offset << "]\n";
        return;
    }

    if (n->type == node_string_len)
    {
        gen_node(n->children[0], dst);
        dst << "    mov rcx, rax\n";
        dst << "    sub rsp, 40\n";
        dst << "    call [strlen]\n";
        dst << "    add rsp, 40\n";
        return;
    }

    if (n->type == node_and)
    {
        int label_id = label_count++;
        gen_node(n->children[0], dst);
        dst << "    test rax, rax\n";
        dst << "    je .L_and_false_" << label_id << "\n";

        gen_node(n->children[1], dst);
        dst << "    test rax, rax\n";
        dst << "    setne al\n";
        dst << "    movzx rax, al\n";
        dst << ".L_and_false_" << label_id << ":\n";
        return;
    }

    if (n->type == node_or)
    {
        int label_id = label_count++;
        gen_node(n->children[0], dst);
        dst << "    test rax, rax\n";
        dst << "    jne .L_or_true_" << label_id << "\n";

        gen_node(n->children[1], dst);
        dst << "    test rax, rax\n";
        dst << "    setne al\n";
        dst << "    movzx rax, al\n";
        dst << "    jmp .L_or_end_" << label_id << "\n";
        dst << ".L_or_true_" << label_id << ":\n";
        dst << "    mov rax, 1\n";
        dst << ".L_or_end_" << label_id << ":\n";
        return;
    }

    if (n->type == node_eq || n->type == node_ne || n->type == node_gt ||
        n->type == node_lt || n->type == node_ge || n->type == node_le)
    {
        gen_node(n->children[0], dst);
        dst << "    push rax\n";
        gen_node(n->children[1], dst);
        dst << "    mov rcx, rax\n";
        dst << "    pop rax\n";
        dst << "    cmp rax, rcx\n";

        switch (n->type)
        {
        case node_eq: dst << "    sete al\n";  break;
        case node_ne: dst << "    setne al\n"; break;
        case node_gt: dst << "    setg al\n";  break;
        case node_lt: dst << "    setl al\n";  break;
        case node_ge: dst << "    setge al\n"; break;
        case node_le: dst << "    setle al\n"; break;
        default: break;
        }
        dst << "    movzx rax, al\n";
        return;
    }

    if (n->type == node_call)
    {
        node* callable_target = n->children[0];
        std::string func_name = callable_target->name;
        std::string explicit_scope = "";

        if (callable_target->type == node_member_access || func_name.empty())
        {
            if (!callable_target->children.empty())
            {
                if (callable_target->children.size() >= 2)
                {
                    explicit_scope = callable_target->children[0]->name;
                    func_name = callable_target->children[1]->name;
                }
                else if (callable_target->children.size() == 1)
                {
                    explicit_scope = callable_target->name;
                    func_name = callable_target->children[0]->name;
                }
            }
        }

        if (explicit_scope.empty() && func_name.empty() && !callable_target->name.empty())
        {
            func_name = callable_target->name;
        }

        std::string target_dll = explicit_scope;
        if (!explicit_scope.empty())
        {
            auto alias_it = libr_aliases.find(explicit_scope);
            if (alias_it != libr_aliases.end())
            {
                target_dll = alias_it->second;
            }
        }

        if (!target_dll.empty()) {
            auto& dll_funcs = dynamic_imports[target_dll];
            if (std::find_if(dll_funcs.begin(), dll_funcs.end(), [&](const auto& f) { return f.int_name == func_name; }) == dll_funcs.end()) {
                dll_funcs.push_back({ func_name, func_name });
            }
        }

        std::vector<node*> call_args(n->children.begin() + 1, n->children.end());

        for (size_t i = call_args.size(); i > 0; --i) {
            gen_node(call_args[i - 1], dst);
            dst << "    push rax\n";
        }

        size_t extra_args = (call_args.size() > 4) ? (call_args.size() - 4) : 0;
        size_t stack_pushed_bytes = extra_args * 8;
        size_t shadow_space = 32;
        size_t total_needed = shadow_space + stack_pushed_bytes;
        size_t padding = (16 - (total_needed % 16)) % 16;
        size_t final_rsp_sub = total_needed + padding;

        if (final_rsp_sub > 0) {
            dst << "    sub rsp, " << final_rsp_sub << "\n";
        }

        if (call_args.size() >= 1) dst << "    mov rcx, qword [rsp + " << final_rsp_sub << "]\n";
        if (call_args.size() >= 2) dst << "    mov rdx, qword [rsp + " << (final_rsp_sub + 8) << "]\n";
        if (call_args.size() >= 3) dst << "    mov r8, qword [rsp + " << (final_rsp_sub + 16) << "]\n";
        if (call_args.size() >= 4) dst << "    mov r9, qword [rsp + " << (final_rsp_sub + 24) << "]\n";

        for (size_t i = 0; i < extra_args; ++i) {
            dst << "    mov rax, qword [rsp + " << (final_rsp_sub + 32 + (i * 8)) << "]\n";
            dst << "    mov qword [rsp + " << (32 + (i * 8)) << "], rax\n";
        }

        std::string ext_name = find_external_name(func_name);
        if (ext_name.empty() && !target_dll.empty()) {
            ext_name = func_name;
        }

        if (func_name == "las_print") {
            dst << "    call [printf]\n";
        }
        else if (!ext_name.empty()) {
            dst << "    call [" << ext_name << "]\n";
        }
        else {
            dst << "    call _las_" << func_name << "\n";
        }

        size_t total_cleanup = final_rsp_sub + (call_args.size() * 8);
        if (total_cleanup > 0) {
            dst << "    add rsp, " << total_cleanup << "\n";
        }
        return;
    }

    if (n->type == node_member_access)
    {
        node* base = n->children[0];

        int base_offset = get_var_offset(base->name);
        std::string base_type = get_var_type(base->name);
        std::string member_name = n->children[1]->name;

        if (!struct_registry.count(base_type))
        {
            return;
        }

        dst << "    lea rax, [rbp - " << base_offset << "]\n";

        for (const auto& member : struct_registry[base_type].members)
        {
            if (member.name != member_name)
            {
                continue;
            }

            if (member.offset != 0)
            {
                dst << "    add rax, " << member.offset << "\n";
            }

            if (member.width == 1)
            {
                dst << "    movzx rax, byte [rax]\n";
            }
            else if (member.width == 2)
            {
                dst << "    movzx rax, word [rax]\n";
            }
            else if (member.width == 4)
            {
                dst << "    mov eax, dword [rax]\n";
            }
            else
            {
                dst << "    mov rax, qword [rax]\n";
            }

            break;
        }

        return;
    }

    if (n->type == node_index)
    {
        gen_node(n->children[0], dst);
        dst << "    push rax\n";
        gen_node(n->children[1], dst);

        std::string arr_type = get_var_type(n->children[0]->name);
        std::string base_type = arr_type.substr(0, arr_type.find("_array"));
        int element_width = get_type_width(base_type);

        dst << "    imul rax, " << element_width << "\n";
        dst << "    pop rcx\n";
        dst << "    sub rcx, rax\n";

        if (element_width == 1) dst << "    movzx rax, byte [rcx]\n";
        else if (element_width == 2) dst << "    movzx rax, word [rcx]\n";
        else if (element_width == 4) dst << "    mov eax, dword [rcx]\n";
        else dst << "    mov rax, qword [rcx]\n";
        return;
    }

    if (n->children.size() >= 2) {
        gen_node(n->children[0], dst);
        dst << "    push rax\n";
        gen_node(n->children[1], dst);
        dst << "    mov rcx, rax\n";
        dst << "    pop rax\n";

        switch (n->type)
        {
        case node_add: dst << "    add rax, rcx\n"; break;
        case node_sub: dst << "    sub rax, rcx\n"; break;
        case node_mul: dst << "    imul rax, rcx\n"; break;
        case node_div: dst << "    cqo\n    idiv rcx\n"; break;
        case node_mod: dst << "    cqo\n    idiv rcx\n    mov rax, rdx\n"; break;
        case node_lshift: dst << "    shl rax, cl\n"; break;
        case node_rshift: dst << "    shr rax, cl\n"; break;
        case node_bitwise_and: dst << "    and rax, rcx\n"; break;
        case node_bitwise_or:  dst << "    or rax, rcx\n";  break;
        case node_bitwise_xor: dst << "    xor rax, rcx\n"; break;
        default: break;
        }
    }
}