#include "ast.hpp"

node* ast::make_node(node_type type, node* child1, node* child2)
{
    node* n = new node();
    n->type = type;
    n->name = "";
    n->literal = "";
    n->generic_type = "";
    n->array_size = 0;

    if (child1 != nullptr) n->children.push_back(child1);
    if (child2 != nullptr) n->children.push_back(child2);

    nodes.push_back(n);
    return n;
}

node* ast::make_block(const std::vector<node*>& stmts)
{
    node* n = new node();
    n->type = node_block;
    n->name = "";
    n->literal = "";
    n->generic_type = "";
    n->array_size = 0;
    n->children = stmts;

    nodes.push_back(n);
    return n;
}

node* ast::make_call_node(node* callable, const std::vector<node*>& args)
{
    node* n = new node();
    n->type = node_call;
    n->name = "";
    n->literal = "";
    n->generic_type = "";
    n->array_size = 0;

    n->children.push_back(callable);
    for (node* arg : args)
    {
        n->children.push_back(arg);
    }

    nodes.push_back(n);
    return n;
}

void ast::free_all()
{
    for (node* n : nodes)
    {
        delete n;
    }
    nodes.clear();
}