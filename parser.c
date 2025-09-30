#include <stdio.h>
#include <string.h>
#include "md4c/md4c.h"

// Callbacks para procesar elementos MD
int enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
    printf("Entrando bloque: %d\n", type);
    return 0;  // 0 = éxito
}

int leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata) {
    printf("Saliendo bloque: %d\n", type);
    return 0;
}

int enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
    printf("Entrando span: %d\n", type);
    return 0;
}

int leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata) {
    printf("Saliendo span: %d\n", type);
    return 0;
}

int text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
    // Crear una cadena temporal para imprimir (no nula-terminada)
    char temp[256];
    size_t copy_size = (size < 255) ? size : 255;
    strncpy(temp, text, copy_size);
    temp[copy_size] = '\0';
    
    printf("Texto [tipo:%d]: '%.*s'\n", type, (int)size, text);
    return 0;
}

int parse_something(void) {
    MD_PARSER parser = {
        .abi_version = 0, 
        .flags = 0,
        .enter_block = enter_block_callback,
        .leave_block = leave_block_callback,
        .enter_span = enter_span_callback,  
        .leave_span = leave_span_callback,  
        .text = text_callback,              
        .debug_log = NULL, 
        .syntax = NULL
    };

    const char* text = "# Hola Mundo\nEsto es **markdown**";
    MD_SIZE size = strlen(text);

    int result = md_parse(text, size, &parser, NULL);

    printf("Resultado del parseo: %d\n", result);
    return result;
}
