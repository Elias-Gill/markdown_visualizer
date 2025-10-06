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

// Utility macros
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }
#define MAIN_LAYOUT_ID "main_layout"

bool g_debug_enabled = true;

// Fonts
const uint32_t FONT_ID_BODY_16 = 1;
const uint32_t FONT_ID_BODY_24 = 0;
Font g_fonts[2];

void render_node(MarkdownNode *current_node);

// Current text mode (used to render different type of text)
typedef enum {
    _TEXT_BOLD,
    _TEXT_ITALIC,
    _TEXT_REGULAR
} TextTypeMode;

TextTypeMode curr_text_type_mode = _TEXT_REGULAR;

// ============================================================================
// INIT FUNCTIONS
// ============================================================================

void load_fonts(void) {
    g_fonts[FONT_ID_BODY_24] = LoadFontEx("resources/Roboto-Regular.ttf", 48, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_BODY_24].texture, TEXTURE_FILTER_BILINEAR);

    g_fonts[FONT_ID_BODY_16] = LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400);
    SetTextureFilter(g_fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

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

// Renders a text node with appropriate styling
void render_text(MarkdownNode *node) {
    TextNode cast_node_value = node->value.text;

    // Ignore soft brakes ('\n')
    if (cast_node_value.type == MD_TEXT_SOFTBR) {
        return;
    }

    Clay_String text = make_clay_string(cast_node_value.text, cast_node_value.size, true);

    if (curr_text_type_mode == _TEXT_BOLD){
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = COLOR_ORANGE }));
    } else if (curr_text_type_mode == _TEXT_ITALIC) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = COLOR_BLUE }));
    }
    else { // text regular
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = COLOR_FOREGROUND }));
    }
}

// Renders a span node (inline elements like bold, italic, emphasis, etc)
// When a span type is detected, it changes the rendered text style (curr_text_type_mode)
// to match the desired style (bold, italic, etc). 
//
// After the render of its child elements, the span changes swaps the text mode again to the 
// previous style.
void render_span(MarkdownNode *current_node) {
    MarkdownNode *node = current_node;

    if (node->value.span.type == MD_SPAN_EM) { // Emphasis <em>...<\em>
        TextTypeMode previous_text_type_mode = curr_text_type_mode;
        curr_text_type_mode = _TEXT_ITALIC;
        if (node->first_child) {
            render_node(node->first_child);
        }
        curr_text_type_mode = previous_text_type_mode;
    }

    if (node->value.span.type == MD_SPAN_STRONG) {
        TextTypeMode previous_text_type_mode = curr_text_type_mode;
        curr_text_type_mode = _TEXT_BOLD;
        if (node->first_child) {
            render_node(node->first_child);
        }
        curr_text_type_mode = previous_text_type_mode;
    }

    if (node->value.span.type == MD_SPAN_A) {}
    if (node->value.span.type == MD_SPAN_IMG) {}
    if (node->value.span.type == MD_SPAN_CODE) {}
    if (node->value.span.type == MD_SPAN_DEL) {}
    if (node->value.span.type == MD_SPAN_LATEXMATH) {}
    if (node->value.span.type == MD_SPAN_LATEXMATH_DISPLAY) {}
    if (node->value.span.type == MD_SPAN_WIKILINK) {}
    if (node->value.span.type == MD_SPAN_U) {}
}

// Renders a block node (block-level elements like paragraphs, headers)
void render_block(MarkdownNode *current_node) {
    MarkdownNode *node = current_node;

    // TODO: distinguish block types
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .padding = {16, 16, 16, 16},
            .childGap = 16,
            .sizing = { .width = CLAY_SIZING_FIT(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
        .clip = { .vertical = true, .childOffset = (Clay_Vector2) {0 ,0} }
    }) {
        if (node->first_child) {
            render_node(node->first_child);
        }
    }
}

// ============================================================================
// MAIN LAYOUT
// ============================================================================

// Renders a markdown node and its siblings, recursively parsing all the childs first
void render_node(MarkdownNode *current_node) {
    MarkdownNode *node = current_node;
    while (node) {
        switch (node->type) {
            case NODE_TEXT:
                render_text(node);
                break;
            case NODE_SPAN:
                render_span(node);
                break;
            case NODE_BLOCK:
                render_block(node);
                break;
        }
        node = node->next_sibling;
    }
}

// Renders the complete markdown tree using Clay layout system
Clay_RenderCommandArray render_markdown_tree(void) {
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
        // The root node is a single node, it does not have siblings, so whe do not have to 
        // check and render its siblings
        if (node->first_child) {
            render_node(node->first_child);
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
        .height = GetScreenHeight() / 2
    });

    // Update scroll containers
    Vector2 mousePosition = GetMousePosition();
    Vector2 scrollDelta = GetMouseWheelMoveV();
    Clay_SetPointerState(
        (Clay_Vector2) { mousePosition.x, mousePosition.y },
        IsMouseButtonDown(0)
    );
    Clay_UpdateScrollContainers(
        true,
        (Clay_Vector2) { scrollDelta.x, scrollDelta.y },
        GetFrameTime()
    );

    // Generate the auto layout for rendering
    Clay_RenderCommandArray render_commands = render_markdown_tree();

    // RENDERING ---------------------------------
    BeginDrawing();
    ClearBackground(WHITE);
    Clay_Raylib_Render(render_commands, g_fonts);
    EndDrawing();
}

void start_main_loop() {
    while(!WindowShouldClose()){
        update_frame();
    }
}
