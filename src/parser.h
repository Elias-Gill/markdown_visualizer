#ifndef MD4C_USE_UTF8
#define MD4C_USE_UTF8
#endif  //MD4C_USE_UTF8

#ifndef PARSER_H
#define PARSER_H

#include "md4c.h"
#include <stdbool.h>

// ------------------------------
//  ENUMS y STRUCTS básicos
// ------------------------------

typedef enum {
    NODE_TEXT,
    NODE_SPAN,
    NODE_BLOCK
} NodeType;

typedef struct {
    MD_TEXTTYPE type;
    char *text;
    unsigned size;
    void *userdata;
} TextNode;

typedef struct {
    MD_SPANTYPE type;
    void *detail;           // pointer from MD4C (no ownership)
    void *userdata;
} SpanNode;

typedef struct {
    MD_BLOCKTYPE type;
    void *detail;           // pointer from MD4C (no ownership)
    void *userdata;
} BlockNode;

// ------------------------------
//  Tree node
// ------------------------------

typedef struct MarkdownNode {
    NodeType type;
    union {
        TextNode text;
        SpanNode span;
        BlockNode block;
    } value;

    struct MarkdownNode *parent;
    struct MarkdownNode *first_child;
    struct MarkdownNode *next_sibling;
} MarkdownNode;

// ------------------------------
//  Funciones principales
// ------------------------------

int parse_markdown(const char* text);
void free_tree(MarkdownNode *node);
void print_tree(const MarkdownNode *node, int indent);
MarkdownNode *get_root_node(void);  // Returns the root node

#endif // PARSER_H
