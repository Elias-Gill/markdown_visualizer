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
#define COLOR_BACKGROUND (Clay_Color) {200, 200, 255, 255}

// Utility macros
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }

#define MAIN_LAYOUT_ID "main_layout"
bool g_debug_enabled = false;

// Fonts
const uint32_t FONT_ID_BODY_16 = 1;
const uint32_t FONT_ID_BODY_24 = 0;
Font g_fonts[2];

// ============================================================================
// INIT FUNCTIONS
// ============================================================================
void load_fonts() {
    // prepare used fonts
    g_fonts[FONT_ID_BODY_24] = LoadFontEx("resources/Roboto-Regular.ttf", 48, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_BODY_24].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_BODY_16] = LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

    Clay_SetMeasureTextFunction(Raylib_MeasureText, g_fonts);
}

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

void init_clay() {
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

static inline Clay_String make_clay_string(char *text, long length, bool heap_allocated) {
    return (Clay_String) {
            .isStaticallyAllocated = !heap_allocated,
            .length = length,
            .chars = text,
    };
}

void RenderText(MarkdownNode *node) {
    TextNode cast_node_value = node->value.text;

    Clay_String text = make_clay_string(cast_node_value.text, cast_node_value.size, true);
    // TODO: hacer diferentes cosas segun el tipo de texto
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = COLOR_ORANGE }));
}

// ============================================================================
// LAYOUT PRINCIPAL
// ============================================================================

void RenderNode(MarkdownNode *cur_node) {
    MarkdownNode *node = cur_node;

    while (node) {
        switch (node->type) {
            case NODE_TEXT:
                RenderText(node);
                break;
            case NODE_SPAN:
                // TODO: hacer un wrap de ese if con estilos
                if (node->first_child) {
                    RenderNode(node->first_child);
                }
                break;
            case NODE_BLOCK:
                // TODO: hacer un wrap de ese if con estilos segun el tipo de nodo
                if (node->first_child) {
                    RenderNode(node->first_child);
                }
                break;
        }
        node = node->next_sibling;
    }
}

Clay_RenderCommandArray render_markdown_tree() {
    MarkdownNode *node = get_root_node();

    Clay_BeginLayout();

    CLAY(CLAY_ID(MAIN_LAYOUT_ID), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = {16, 16, 16, 16},
            .childGap = 16,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
        .clip = {
            .vertical = true, .childOffset = (Clay_Vector2) {
                0,0
            }
        }
    }) {
        // Manda renderiza solo los nodos de la raiz, pero no hace nada de logica 
        // a parte.
        while (node) {
            RenderNode(node);
            node = node->next_sibling;
        }
    }

    return Clay_EndLayout();
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void update_frame() {
    if (IsKeyPressed(KEY_D)) {
        g_debug_enabled = !g_debug_enabled;
        Clay_SetDebugModeEnabled(g_debug_enabled);
    }

    Clay_Vector2 mouse_position = RAYLIB_VECTOR2_TO_CLAY_VECTOR2(GetMousePosition());
    Clay_SetPointerState(mouse_position, IsMouseButtonDown(0));

    Clay_SetLayoutDimensions((Clay_Dimensions) {
        (float)GetScreenWidth(), (float)GetScreenHeight()
    });

    // Generate the auto layout for rendering
    Clay_RenderCommandArray render_commands = render_markdown_tree();

    // RENDERING ---------------------------------
    BeginDrawing();
    ClearBackground(WHITE);
    Clay_Raylib_Render(render_commands, g_fonts);
    EndDrawing();
}
