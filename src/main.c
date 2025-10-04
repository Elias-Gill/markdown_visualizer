#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "render.h"

char* read_file(char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read the file content
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        exit(1);
    }

    fread(buffer, 1, file_size, file);
    fclose(file);

    buffer[file_size] = '\0';

    return buffer;
}

int main(int argc, char *argv[]) {
    init_clay();
    parse_markdown(read_file("/home/elias/Descargas/test.md"));

    while(true){
        update_frame();
    }

    return 0;
}
