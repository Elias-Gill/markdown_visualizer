#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "parser.h"
#include "md4c.h"

/* Variables globales de la lista */
static MarkdownNode *head = NULL;
static MarkdownNode *tail = NULL;

/* ------------------------------------------
 *  Linked List (queue)
 * ------------------------------------------ */

MarkdownNode *create_node(NodeType type) {
    MarkdownNode *node = malloc(sizeof(MarkdownNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(MarkdownNode));
    node->type = type;
    node->next = NULL;
    return node;
}

MarkdownNode *insert_node(MarkdownNode *node) {
    if (!node) return NULL;

    if (head == NULL) {
        head = tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
    return node;
}

MarkdownNode *next_node(void) {
    if (head == NULL) return NULL;

    MarkdownNode *old_head = head;
    head = head->next;
    free(old_head);
    return head;
}

void free_all_nodes(void) {
    MarkdownNode *current = head;
    while (current) {
        MarkdownNode *next = current->next;
        if (current->type == NODE_TEXT && current->value.text.text)
            free(current->value.text.text);
        free(current);
        current = next;
    }
    head = tail = NULL;
}

/* ------------------------------------------
 *  Callbacks de MD4C
 * ------------------------------------------ */

int on_enter_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = create_node(NODE_BLOCK);
    if (!node) return -1;

    node->value.block.is_enter = true;
    node->value.block.type = type;
    node->value.block.detail = detail;
    node->value.block.userdata = userdata;

    insert_node(node);
    return 0;
}

int on_leave_block(MD_BLOCKTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = create_node(NODE_BLOCK);
    if (!node) return -1;

    node->value.block.is_enter = false;
    node->value.block.type = type;
    node->value.block.detail = detail;
    node->value.block.userdata = userdata;

    insert_node(node);
    return 0;
}

int on_enter_span(MD_SPANTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = create_node(NODE_SPAN);
    if (!node) return -1;

    node->value.span.is_enter = true;
    node->value.span.type = type;
    node->value.span.detail = detail;
    node->value.span.userdata = userdata;

    insert_node(node);
    return 0;
}

int on_leave_span(MD_SPANTYPE type, void *detail, void *userdata) {
    MarkdownNode *node = create_node(NODE_SPAN);
    if (!node) return -1;

    node->value.span.is_enter = false;
    node->value.span.type = type;
    node->value.span.detail = detail;
    node->value.span.userdata = userdata;

    insert_node(node);
    return 0;
}

int on_text(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *userdata) {
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
    insert_node(node);
    return 0;
}

/* ------------------------------------------
 *  Parser Markdown
 * ------------------------------------------ */

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

    return md_parse(text, size, &parser, NULL);
}

/* ------------------------------------------
 *  Helpers for debug
 * ------------------------------------------ */

void print_nodes(void) {
    MarkdownNode *cur = head;
    while (cur) {
        switch (cur->type) {
            case NODE_TEXT:
                printf("[TEXT] type=%d, text='%s'\n",
                       cur->value.text.type,
                       cur->value.text.text ? cur->value.text.text : "(null)");
                break;
            case NODE_SPAN:
                printf("[SPAN] %s type=%d\n",
                       cur->value.span.is_enter ? "ENTER" : "LEAVE",
                       cur->value.span.type);
                break;
            case NODE_BLOCK:
                printf("[BLOCK] %s type=%d\n",
                       cur->value.block.is_enter ? "ENTER" : "LEAVE",
                       cur->value.block.type);
                break;
        }
        cur = cur->next;
    }
}
