#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "parser.h"
#include "md4c.h"

#define MD4C_USE_UTF8

static MarkdownNode *root_node = NULL;
static MarkdownNode *current_node = NULL;

static bool parsing_code_block = false;
static MarkdownNode *accumulated_text_node = NULL;

static void start_text_accumulation(void) {
    parsing_code_block = true;

    accumulated_text_node = malloc(sizeof(MarkdownNode));
    if (!accumulated_text_node) {
        exit(1);
    }

    accumulated_text_node->type = NODE_TEXT;
    accumulated_text_node->value.text.type = MD_TEXT_NORMAL;
    accumulated_text_node->value.text.text = NULL;
    accumulated_text_node->value.text.size = 0;

    // ensure clean linkage
    accumulated_text_node->parent = NULL;
    accumulated_text_node->first_child = NULL;
    accumulated_text_node->next_sibling = NULL;
}

static void accumulate_text(const MD_CHAR *text, MD_SIZE size) {
    // old_size stores the stored buffer size including the terminating '\0' when present
    MD_SIZE old_size = accumulated_text_node->value.text.size;
    MD_SIZE old_len  = (old_size > 0) ? old_size - 1 : 0; // length without '\0'

    MD_SIZE new_len  = old_len + size; // new length without '\0'
    MD_SIZE new_size = new_len + 1;    // include '\0'

    MD_CHAR *old_text = accumulated_text_node->value.text.text;
    MD_CHAR *new_text = malloc(sizeof(MD_CHAR) * new_size);

    // copy existing content (if any)
    if (old_text && old_len > 0) {
        memcpy(new_text, old_text, old_len);
    }

    // append new chunk
    if (size > 0) {
        memcpy(new_text + old_len, text, size);
    }
    new_text[new_len] = '\0';

    // replace buffer and free old
    free(old_text);
    accumulated_text_node->value.text.text = new_text;
    accumulated_text_node->value.text.size = new_size;
}

// -------------------------------
//  Node creation
// -------------------------------

static MarkdownNode *should_create_node(NodeType type) {
    MarkdownNode *node = malloc(sizeof(MarkdownNode));
    if (!node) {
        printf("Cannot allocate heap memmory");
        exit(1);
    }
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
// NOTE: MD4C does not allocate heap memmory for most of detail structs, so we need to
// handle this allocation manually.

static int on_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = should_create_node(NODE_BLOCK);

    node->value.block.type = type;
    node->value.block.userdata = userdata;

    // Cast and store the block element’s details on the heap
    if (type == MD_BLOCK_H && detail) {
        MD_BLOCK_H_DETAIL *copy = malloc(sizeof(MD_BLOCK_H_DETAIL));
        if (!copy) {
            exit(1);
        }
        *copy = *(MD_BLOCK_H_DETAIL*)detail;
        node->value.block.detail = copy;
    } else if (type == MD_BLOCK_CODE) {
        start_text_accumulation();
    } else {
        // TODO: handle all the detail cases
        node->value.block.detail = detail;
    }

    // Insert node
    if (!root_node) {
        root_node = node;
    } else {
        insert_child_node(current_node, node);
    }

    current_node = node; // descend

    return 0;
}

static int on_leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    if (type == MD_BLOCK_CODE) {
        parsing_code_block = false;
        insert_child_node(current_node, accumulated_text_node);
    }
    // Ignore the remaining function parameters, as the details are actually passed
    // in the opening block.
    current_node = current_node->parent; // ascend
    return 0;
}

static int on_enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = should_create_node(NODE_SPAN);

    node->value.span.type = type;
    node->value.span.detail = detail;
    node->value.span.userdata = userdata;

    insert_child_node(current_node, node);
    current_node = node;
    return 0;
}

static int on_leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    current_node->value.span.type = type;
    // Same as on_leave_block, whe can ignore the parameters as the details are passed in the
    // opening block.
    current_node = current_node->parent;
    return 0;
}

static int on_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
    if (parsing_code_block) {
        accumulate_text(text, size);
        return 0;
    }

    MarkdownNode *node = should_create_node(NODE_TEXT);
    node->value.text.type = type;
    node->value.text.size = size;
    node->value.text.userdata = userdata;

    node->value.text.text = malloc(size + 1);
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
//  Tree traverse operations API
// ------------------------------

MarkdownNode *get_root_node(void) {
    return root_node;
}

void free_tree(MarkdownNode *node) {
    if (!node) return;

    MarkdownNode *child = node->first_child;
    while (child) {
        MarkdownNode *next = child->next_sibling;
        free_tree(child);
        child = next;
    }
    if (node->type == NODE_TEXT && node->value.text.text) {
        free(node->value.text.text);
    }
    if (node->type == NODE_BLOCK &&
            node->value.block.type == MD_BLOCK_H &&
            node->value.block.detail) {
        free(node->value.block.detail);
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
