// NOTE: keep it on top of the file in that specific order
#define CLAY_IMPLEMENTATION
#include "clay/clay.h"
#include "clay/clay_renderer_raylib.c"

#include "parser.h"
#include "md4c/md4c.h"

#include "render.h"

// Color palette
#define COLOR_ORANGE     (Clay_Color) {225, 138, 50, 255}
#define COLOR_BLUE       (Clay_Color) {111, 173, 162, 255}
#define COLOR_FOREGROUND (Clay_Color) {23, 23, 23, 255}
#define COLOR_BACKGROUND (Clay_Color) {255, 255, 255, 255}
#define MAX_CONTAINER_WIDTH 900

// Utility macros
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }
#define MAIN_LAYOUT_ID "main_layout"
bool g_debug_enabled = true;

// ---------- Fonts ------------
const uint32_t FONT_ID_REGULAR          = 0;
const uint32_t FONT_ID_ITALIC           = 1;
const uint32_t FONT_ID_SEMIBOLD         = 2;
const uint32_t FONT_ID_SEMIBOLD_ITALIC  = 3;
const uint32_t FONT_ID_BOLD             = 4;
const uint32_t FONT_ID_EXTRABOLD        = 5;
Font g_fonts[6];

Clay_TextElementConfig font_body_regular = { .fontId = FONT_ID_REGULAR, .fontSize = 24, .textColor = COLOR_FOREGROUND };
Clay_TextElementConfig font_body_italic  = { .fontId = FONT_ID_ITALIC, .fontSize = 24, .textColor = COLOR_FOREGROUND };
Clay_TextElementConfig font_body_bold    = { .fontId = FONT_ID_BOLD, .fontSize = 24, .textColor = COLOR_FOREGROUND };

Clay_TextElementConfig font_h1 = { .fontId = FONT_ID_EXTRABOLD, .fontSize = 34, .textColor = COLOR_FOREGROUND };
Clay_TextElementConfig font_h2 = { .fontId = FONT_ID_EXTRABOLD, .fontSize = 30, .textColor = COLOR_FOREGROUND };
Clay_TextElementConfig font_h3 = { .fontId = FONT_ID_EXTRABOLD, .fontSize = 28, .textColor = COLOR_FOREGROUND };
Clay_TextElementConfig font_h4 = { .fontId = FONT_ID_BOLD, .fontSize = 26, .textColor = COLOR_FOREGROUND };

// NOTE: initialized on rendering and updated on every frame if the screen is resized
// Determines the number of characters that can fit inside a line.
int available_characters = 0;

typedef struct {
    Clay_String string;
    Clay_TextElementConfig *config;
} TextElement;

// Temporary text buffers registry (must live until after Clay_Raylib_Render)
static char **g_temp_text_buffers = NULL;
static int   g_temp_text_count = 0;
static int   g_temp_text_capacity = 0;

static void ensure_temp_text_capacity(int needed) {
    if (g_temp_text_capacity >= needed) return;
    int newcap = g_temp_text_capacity ? g_temp_text_capacity * 2 : 256;
    while (newcap < needed) newcap *= 2;
    g_temp_text_buffers = (char**)realloc(g_temp_text_buffers, newcap * sizeof(char*));
    g_temp_text_capacity = newcap;
}

static void push_temp_text_buffer(char *buf) {
    ensure_temp_text_capacity(g_temp_text_count + 1);
    g_temp_text_buffers[g_temp_text_count++] = buf;
}

static void free_all_temp_text_buffers(void) {
    for (int i = 0; i < g_temp_text_count; ++i) {
        free(g_temp_text_buffers[i]);
    }
    g_temp_text_count = 0;
    // keep g_temp_text_buffers allocated for reuse to avoid repeated realloc/free
}

// ============================================================================
// INIT FUNCTIONS
// ============================================================================
void load_fonts(void) {
    g_fonts[FONT_ID_REGULAR] = LoadFontEx("resources/NotoSans-Regular.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_REGULAR].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_ITALIC] = LoadFontEx("resources/NotoSans-Italic.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_ITALIC].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_SEMIBOLD] = LoadFontEx("resources/NotoSans-SemiBold.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_SEMIBOLD].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_SEMIBOLD_ITALIC] = LoadFontEx("resources/NotoSans-SemiBoldItalic.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_SEMIBOLD_ITALIC].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_BOLD] = LoadFontEx("resources/NotoSans-Bold.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_BOLD].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_EXTRABOLD] = LoadFontEx("resources/NotoSans-ExtraBold.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_EXTRABOLD].texture, TEXTURE_FILTER_BILINEAR);

    Clay_SetMeasureTextFunction(Raylib_MeasureText, g_fonts);
}

// Handles Clay library errors and resizes buffers if needed
void handle_clay_errors(Clay_ErrorData error_data) {
    printf("%s", error_data.errorText.chars);
    if (error_data.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) {
        Clay_SetMaxElementCount(Clay_GetMaxElementCount() * 2);
        exit(1);
    } else if (error_data.errorType == CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {
        Clay_SetMaxMeasureTextCacheWordCount(Clay_GetMaxMeasureTextCacheWordCount() * 2);
        exit(1);
    }
}

void init_clay(void) {
    uint64_t total_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_memory = Clay_CreateArenaWithCapacityAndMemory(total_memory_size,
                             malloc(total_memory_size));
    Clay_Initialize(
        clay_memory,
    (Clay_Dimensions) {
        (float)GetScreenWidth(),
        (float)GetScreenHeight()
    },
    (Clay_ErrorHandler) {
        handle_clay_errors, 0
    });

    Clay_Raylib_Initialize(1024, 768, "Markdown Viewer", FLAG_WINDOW_RESIZABLE);
    load_fonts();
}

// ============================================================================
// REUSABLE COMPONENTS
// ============================================================================

// Creates a Clay_String from a C string
static inline Clay_String make_clay_string(char *text, long length, bool is_heap_allocated) {
    return (Clay_String) {
        .isStaticallyAllocated = !is_heap_allocated,
        .length = length,
        .chars = text,
    };
}

static void render_text_elements(TextElement *line, int count) {
    CLAY_AUTO_ID({
            .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = 2,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
            },
            .backgroundColor = COLOR_BACKGROUND,
            }) {
        for (int i = 0; i < count; ++i) {
            // config is stored as pointer to global config; dereference for CLAY_TEXT
            CLAY_TEXT(line[i].string, line[i].config);
        }
    }
}

// Inserta un fragmento de texto en la línea actual.
// Si la línea se llenó, la renderiza y vacía. No libera buffers aquí.
static void push_text_segment(
    TextElement *line,
    int *index,
    int *char_count,
    const char *src,
    int len,
    Clay_TextElementConfig *config
) {
    // Copiar el fragmento a un buffer temporal
    char *buf = (char*)malloc((len + 1));
    memcpy(buf, src, len);
    buf[len] = '\0';

    // Registrar buffer para liberar después del render
    push_temp_text_buffer(buf);

    line[*index].string = make_clay_string(buf, len, true);
    line[*index].config = config;
    (*index)++;
    (*char_count) += len;

    // Si la línea se llenó, renderiza y reinicia contadores (no liberar aquí)
    if (*char_count >= available_characters) {
        render_text_elements(line, *index);
        *index = 0;
        *char_count = 0;
    }
}

void render_block(MarkdownNode *current_node) {
    CLAY_AUTO_ID({
            .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = {1, 1, 1, 1},
            .childGap = 2,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
            },
            .backgroundColor = COLOR_BACKGROUND,
            }) {
    MarkdownNode *node = current_node;
    int node_type = node->value.block.type;

    switch (node_type) {
        case MD_BLOCK_P: {
            TextElement line[256];
            // Ensure a clean slate
            for (int i = 0; i < 256; ++i) {
                line[i].string.chars = NULL;
                line[i].string.length = 0;
                line[i].string.isStaticallyAllocated = true;
                line[i].config = &font_body_regular;
            }
            int index = 0;
            int char_count = 0;

            MarkdownNode *aux = node->first_child;
            while (aux) {
                const char *string = NULL;
                int size = 0;
                Clay_TextElementConfig *config = &font_body_regular;

                switch (aux->type) {
                    case NODE_TEXT:
                        // Ignore soft breaks
                        if (aux->value.text.type == MD_TEXT_SOFTBR) {
                            aux = aux->next_sibling;
                            continue;
                        }
                        string = aux->value.text.text;
                        size = aux->value.text.size;
                        config = &font_body_regular;
                        break;

                    case NODE_SPAN:
                        // Map span types to configs
                        switch (aux->value.span.type) {
                            case MD_SPAN_EM:
                                config = &font_body_italic;
                                break;
                            case MD_SPAN_STRONG:
                                config = &font_body_bold;
                                break;
                            default:
                                aux = aux->next_sibling;
                                continue;
                        }
                        if (aux->first_child && aux->first_child->type == NODE_TEXT) {
                            string = aux->first_child->value.text.text;
                            size = aux->first_child->value.text.size;
                        } else {
                            aux = aux->next_sibling;
                            continue;
                        }
                        break;

                    default:
                        aux = aux->next_sibling;
                        continue;
                }

                int consumed = 0;
                while (consumed < size) {
                    // If no space, flush current line
                    int space = available_characters - char_count;
                    if (space <= 0) {
                        if (index > 0) {
                            render_text_elements(line, index);
                            index = 0;
                            char_count = 0;
                        }
                        space = available_characters;
                    }

                    int remaining = size - consumed;
                    int take = remaining > space ? space : remaining;
                    push_text_segment(line, &index, &char_count, string + consumed, take, config);
                    consumed += take;
                }

                aux = aux->next_sibling;
            }

            // Render any remaining elements in the last line
            if (index > 0) {
                render_text_elements(line, index);
            }
            break;
        }
        default:
            break;
    }
    }
}

// ============================================================================
// MAIN LAYOUT
// ============================================================================

// Renders the complete markdown tree using Clay layout system
Clay_RenderCommandArray render_markdown_tree(void) {
    MarkdownNode *node = get_root_node();

    Clay_BeginLayout();

    int left_pad = (int) (GetScreenWidth() / 10); // 24 is the regular font size
    CLAY(CLAY_ID(MAIN_LAYOUT_ID), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = { left_pad, 0, 36, 36 },
            .childGap = 16,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
            .sizing = { .width = CLAY_SIZING_GROW(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
        .clip = {
            .vertical = true,
            .horizontal = true,
            .childOffset = Clay_GetScrollOffset()
        }
    }) {
        // The root node is a single node, it does not have siblings. All the childs of the
        // root node are always block nodes.
        if (node->first_child) {
            node = node->first_child;
            while (node) {
                render_block(node);
                node = node->next_sibling;
            }
        }
    }

    return Clay_EndLayout();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

// Updates the application state and renders the current frame
void update_frame(void) {
    // Debug key
    if (IsKeyPressed(KEY_D)) {
        g_debug_enabled = !g_debug_enabled;
        Clay_SetDebugModeEnabled(g_debug_enabled);
    }

    // Update dimensions to handle resizing
    Clay_SetLayoutDimensions((Clay_Dimensions) {
        .width = GetScreenWidth(),
        .height = GetScreenHeight()
    });

    // Calculate how many characters can be displayed in a single line.
    available_characters = (int) (GetScreenWidth() / 12); // 24 is the regular font size

    // Update scroll containers
    Vector2 mousePosition = GetMousePosition();
    Clay_SetPointerState(
        (Clay_Vector2) { mousePosition.x, mousePosition.y },
        IsMouseButtonDown(0)
    );
    Vector2 scrollDelta = GetMouseWheelMoveV();
    Clay_UpdateScrollContainers(
        true,
        (Clay_Vector2) { scrollDelta.x, scrollDelta.y * 3.5 },
        GetFrameTime()
    );

    // Generate the auto layout for rendering
    Clay_RenderCommandArray render_commands = render_markdown_tree();

    // RENDERING ---------------------------------
    BeginDrawing();
    ClearBackground(WHITE);
    Clay_Raylib_Render(render_commands, g_fonts);

    // Now it's safe to free all temporary text buffers created during layout
    free_all_temp_text_buffers();

    EndDrawing();
}

void start_main_loop() {
    while(!WindowShouldClose()){
        update_frame();
    }
}
