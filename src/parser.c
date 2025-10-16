#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "parser.h"
#include "md4c.h"

static MarkdownNode *root_node = NULL;
static MarkdownNode *current_node = NULL;

// -------------------------------
//  Node creation
// -------------------------------

static MarkdownNode *create_node(NodeType type) {
    MarkdownNode *node = malloc(sizeof(MarkdownNode));
    if (!node) return NULL;
    memset(node, 0, sizeof(MarkdownNode));
    node->type = type;
    return node;
}

static void insert_child_node(MarkdownNode *parent, MarkdownNode *child) {
    if (!parent || !child) return;
    child->parent = parent;
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        MarkdownNode *s = parent->first_child;
        while (s->next_sibling) s = s->next_sibling;
        s->next_sibling = child;
    }
}

// ------------------------------
//  MD4C Callbacks
// ------------------------------

static int on_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = create_node(NODE_BLOCK);
    if (!node) return -1;

    node->value.block.type = type;
    node->value.block.userdata = userdata;

    // Solo hacemos copia si es un encabezado (header)
    if (type == MD_BLOCK_H && detail) {
        MD_BLOCK_H_DETAIL *copy = malloc(sizeof(MD_BLOCK_H_DETAIL));
        if (!copy) return -1;  // chequeo simple de malloc
        *copy = *(MD_BLOCK_H_DETAIL*)detail;
        node->value.block.detail = copy;
    } else {
        node->value.block.detail = detail; // para otros bloques dejamos el puntero tal cual
    }

    // Insertamos en el árbol
    if (!root_node)
        root_node = node;
    else
        insert_child_node(current_node, node);

    current_node = node; // descendemos

    return 0;
}

static int on_leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (!current_node) return -1;
    // Ignore the rest of function parameters because the detais are actually passed 
    // on the oppening block
    current_node = current_node->parent; // ascendemos
    return 0;
}

static int on_enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = create_node(NODE_SPAN);
    if (!node) return -1;
    node->value.span.type = type;
    node->value.span.detail = detail;
    node->value.span.userdata = userdata;

    insert_child_node(current_node, node);
    current_node = node;
    return 0;
}

static int on_leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    if (!current_node) return -1;
    current_node->value.span.type = type;
    current_node->value.span.detail = detail;
    current_node->value.span.userdata = userdata;

    current_node = current_node->parent;
    return 0;
}

static int on_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    MarkdownNode *node = create_node(NODE_TEXT);
    if (!node) return -1;
    node->value.text.type = type;
    node->value.text.size = size;
    node->value.text.userdata = userdata;

    node->value.text.text = malloc(size + 1);
    if (!node->value.text.text) {
        free(node);
        return -1;
    }
    memcpy(node->value.text.text, text, size);
    node->value.text.text[size] = '\0';

    insert_child_node(current_node, node);
    return 0;
}

// ------------------------------
//  Parser Markdown
// ------------------------------

// Note: it works using md4c function callbacks to build a elements tree out of the parsing results.
int parse_markdown(const char* text) {
    MD_SIZE size = strlen(text);

    MD_PARSER parser = {
        .abi_version = 0,
        .flags = 0,
        .enter_block = on_enter_block,
        .leave_block = on_leave_block,
        .enter_span = on_enter_span,
        .leave_span = on_leave_span,
        .text = on_text,
        .debug_log = NULL,
        .syntax = NULL
    };

    root_node = NULL;
    current_node = NULL;

    return md_parse(text, size, &parser, NULL);
}

// ------------------------------
//  Tree operation methods
// ------------------------------

MarkdownNode *next_node(MarkdownNode *parent) {
    if (!parent) return NULL;
    return parent->first_child;
}

MarkdownNode *get_root_node(void) {
    return root_node;
}

void free_tree(MarkdownNode *node) {
    if (!node) return;

    // liberar hijos
    MarkdownNode *child = node->first_child;
    while (child) {
        MarkdownNode *next = child->next_sibling;
        free_tree(child);
        child = next;
    }

    // liberar texto heap
    if (node->type == NODE_TEXT && node->value.text.text) {
        free(node->value.text.text);
    }

    free(node);
}

// ------------------------------
//  Debug utilities
// ------------------------------

void print_tree(const MarkdownNode *node, int indent) {
    while (node) {
        for (int i = 0; i < indent; i++) printf("\t");

        switch (node->type) {
        case NODE_TEXT:
            if (node->value.text.type == MD_TEXT_SOFTBR) {
                printf("[TEXT] type=softbrake\n");
            } else {
                printf("[TEXT] type=%d, text='%s'\n", node->value.text.type,
                       node->value.text.text ? node->value.text.text : "(null)");
            }
            break;
        case NODE_SPAN:
            printf("[SPAN] type=%d\n",
                   node->value.span.type);
            break;
        case NODE_BLOCK:
            printf("[BLOCK] type=%d\n",
                   node->value.block.type);
            break;
        }

        if (node->first_child) {
            print_tree(node->first_child, indent + 1);
        }

        node = node->next_sibling;
    }
}
