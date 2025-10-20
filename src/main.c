#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "render.h"

char* read_file(char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file");
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
    if (argc != 2) {
        fprintf(stderr, "Bad arguments provided. \nUsage: markdown_visualizer <filename>.md");
        exit(1);
    }

    char *filename = argv[1];
    parse_markdown(read_file(filename));
    print_tree(get_root_node(), 0);

    init_clay();
    start_main_loop();

    return 0;
}
