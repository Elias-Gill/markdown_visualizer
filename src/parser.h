#ifndef PARSER_H
#define PARSER_H

#include "md4c.h"
#include <stdbool.h>

// ------------------------------
//  ENUMS and basic STRUCTS
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
    bool is_enter;          // true = enter_span, false = leave_span
    MD_SPANTYPE type;
    void *detail;           // pointer from MD4C (no ownership)
    void *userdata;
} SpanNode;

typedef struct {
    bool is_enter;          // true = enter_block, false = leave_block
    MD_BLOCKTYPE type;
    void *detail;           // pointer from MD4C (no ownership)
    void *userdata;
} BlockNode;

// ------------------------------
//  Linked List Types
// ------------------------------

typedef struct MarkdownNode {
    NodeType type;
    struct MarkdownNode *next;
    union {
        TextNode text;
        SpanNode span;
        BlockNode block;
    } value;
} MarkdownNode;

int parse_markdown(const char* text);
void print_nodes(void);
void free_all_nodes(void);
MarkdownNode *next_node(void);

#endif // PARSER_H
