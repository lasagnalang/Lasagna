#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include "engine/lexer/lexer.hpp"
#include "engine/ast/ast.hpp"
#include "engine/parser/parser.hpp"
#include "engine/gen/gen.hpp"

std::string read_file(const std::string& path)
{
    std::ifstream file(path);
    std::string text;
    std::string line;

    while (std::getline(file, line))
    {
        text += line + "\n";
    }
    return text;
}

int main(int argc, char* argv[])
{
    std::string source = read_file(argv[1]);

    lexer lex;
    std::vector<lasagna_token> tokens = lex.tokenize(source);

    ast tree;
    parser p(tokens, tree);
    std::vector<node*> program = p.parse_program();

    std::ofstream asm_file("output.asm");
    code_generator gen;
    gen.generate_program(program);

    asm_file << gen.get_code();
    asm_file.close();

    tree.free_all();

    system("fasm.exe output.asm output.exe");

    return 0;
}