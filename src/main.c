#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "render.h"

#define VERSION "1.0.0"

// Global debug flag
static int debug_mode = 0;

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <filename.md>\n", program_name);
    printf("Options:\n");
    printf("  --debug    Print AST tree for debugging\n");
    printf("  --help     Show this help message\n");
    printf("  --version  Show version information\n");
    printf("\nExamples:\n");
    printf("  %s document.md\n", program_name);
    printf("  %s --debug document.md\n", program_name);
}

void print_version() {
    printf("Markdown Visualizer v%s\n", VERSION);
}

char* read_file(const char *file_name) {
    // Check file extension
    const char *ext = strrchr(file_name, '.');
    if (ext == NULL || strcmp(ext, ".md") != 0) {
        fprintf(stderr, "Warning: File '%s' doesn't have .md extension\n", file_name);
    }

    FILE *file = fopen(file_name, "rb"); // Use binary mode for consistent reading
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", file_name);
        exit(1);
    }

    // Determine file size more reliably
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error seeking file");
        fclose(file);
        exit(1);
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        perror("Error getting file size");
        fclose(file);
        exit(1);
    }

    if (file_size == 0) {
        fprintf(stderr, "Error: File '%s' is empty\n", file_name);
        fclose(file);
        exit(1);
    }

    rewind(file);

    // Allocate buffer with error checking
    char *buffer = (char *)malloc(file_size + 1);
    if (buffer == NULL) {
        perror("Error: Memory allocation failed");
        fclose(file);
        exit(1);
    }

    // Read file content with error checking
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        if (ferror(file)) {
            perror("Error reading file");
            free(buffer);
            fclose(file);
            exit(1);
        }
    }

    fclose(file);
    buffer[bytes_read] = '\0';

    return buffer;
}

int main(int argc, char *argv[]) {
    const char *filename = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (filename != NULL) {
                fprintf(stderr, "Error: Multiple filenames specified\n");
                print_usage(argv[0]);
                return 1;
            }
            filename = argv[i];
        }
    }

    // Validate that we have a filename
    if (filename == NULL) {
        fprintf(stderr, "Error: No filename provided\n");
        print_usage(argv[0]);
        return 1;
    }

    // Read and parse markdown
    char *file_content = read_file(filename);

    // Performance: measure parsing time if in debug mode
    if (debug_mode) {
        printf("=== DEBUG MODE ===\n");
        printf("Parsing file: %s\n", filename);
    }

    parse_markdown(file_content);

    // Print AST tree if debug mode is enabled
    if (debug_mode) {
        printf("\n=== AST TREE ===\n");
        print_tree(get_root_node(), 0);
        printf("================\n\n");
    }

    // Initialize and run the renderer
    initialize_application();

    // Cleanup
    free(file_content);
    free_tree(get_root_node());

    return 0;
}
