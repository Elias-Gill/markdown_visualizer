// Disabe annoying braces warning from clay compilation
#pragma GCC diagnostic ignored "-Wmissing-braces"
// NOTE: keep this on top of the file in that specific order
#define CLAY_IMPLEMENTATION
#include "clay/clay.h"
#include "clay/clay_renderer_raylib.c"

#include "parser.h"
#include "md4c/md4c.h"

#include "render.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================

// Color palette
#ifdef WHITE_MODE
#define COLOR_BACKGROUND (Clay_Color){250, 250, 250, 255}  // #FAFAFA
#define COLOR_FOREGROUND (Clay_Color){33, 37, 41, 255}     // #212529
#define COLOR_DIM        (Clay_Color){180, 180, 180, 180}  // #B4B4B4
#define COLOR_PINK       (Clay_Color){203, 63, 140, 255}   // #CB3F8C
#define COLOR_BLUE       (Clay_Color){38, 139, 210, 255}   // #268BD2
#define COLOR_ORANGE     (Clay_Color){230, 140, 50, 255}   // #E68C32
#define COLOR_BORDER     (Clay_Color){210, 210, 210, 255}  // #D2D2D2
#define COLOR_DARK       (Clay_Color){238, 238, 238, 255}  // #EEEEEE
#define COLOR_HOVER      (Clay_Color){230, 230, 230, 255}  // #E6E6E6
#define COLOR_HIGHLIGHT  (Clay_Color){218, 232, 252, 255}  // #DAE8FC
#else // ----- DARK MODE -----
#define COLOR_BACKGROUND (Clay_Color){28, 28, 30, 255}     // #1C1C1E
#define COLOR_FOREGROUND (Clay_Color){230, 230, 230, 255}  // #E6E6E6
#define COLOR_DIM        (Clay_Color){90, 90, 90, 190}     // #5A5A5A
#define COLOR_PINK       (Clay_Color){235, 120, 175, 255}  // #EB78AF
#define COLOR_BLUE       (Clay_Color){122, 201, 255, 255}  // #7AC9FF
#define COLOR_ORANGE     (Clay_Color){255, 170, 100, 255}  // #FFAA64
#define COLOR_BORDER     (Clay_Color){60, 60, 60, 255}     // #3C3C3C
#define COLOR_DARK       (Clay_Color){20, 20, 20, 255}     // #141414
#define COLOR_HOVER      (Clay_Color){45, 45, 45, 255}     // #2D2D2D
#define COLOR_HIGHLIGHT  (Clay_Color){48, 60, 75, 255}     // #303C4B
#endif

// Utility macros
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }
#define MAIN_LAYOUT_ID "main_layout"
#define CONTENT_WIDTH_PX (GetScreenWidth() * 0.95f)
#define LEFT_PADDING_DIVISOR 8
#define HR_SCALING_FACTOR 0.8f

// List modes
typedef enum {
    LIST_MODE_UNORDERED = 0,
    LIST_MODE_ORDERED = 1
} ListMode;

// Font identifiers
typedef enum {
    FONT_ID_REGULAR = 0,
    FONT_ID_ITALIC,
    FONT_ID_SEMIBOLD,
    FONT_ID_SEMIBOLD_ITALIC,
    FONT_ID_BOLD,
    FONT_ID_EXTRABOLD,
    FONT_ID_EMOJI,
    FONT_COUNT
} FontId;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

#define BASE_FONT_SIZE 22
#define FONT_SCALE_FACTOR 2

static bool g_debug_enabled = true;
static int g_base_font_size = BASE_FONT_SIZE;
static ListMode g_current_list_mode = LIST_MODE_ORDERED;

// Fonts
static Font g_fonts[FONT_COUNT];

// Text styles
static Clay_TextElementConfig g_font_body_regular;
static Clay_TextElementConfig g_font_body_italic;
static Clay_TextElementConfig g_font_body_bold;
static Clay_TextElementConfig g_font_h1;
static Clay_TextElementConfig g_font_h2;
static Clay_TextElementConfig g_font_h3;
static Clay_TextElementConfig g_font_h4;
static Clay_TextElementConfig g_font_h5;
static Clay_TextElementConfig g_font_inline_code;

// Text rendering system
static int g_available_characters = 0;

#define MAX_TEXT_ELEMENTS 256
#define INITIAL_TEMP_TEXT_CAPACITY 256

typedef struct {
    Clay_String string;
    Clay_TextElementConfig* config;
} TextElement;

typedef struct {
    TextElement elements[MAX_TEXT_ELEMENTS];
    int count;
    int char_count;
} TextLine;

static TextLine g_current_line;

// Temporary text buffers
static char** g_temp_text_buffers = NULL;
static int g_temp_text_count = 0;
static int g_temp_text_capacity = 0;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static inline Clay_String make_clay_string(char* text, long length) {
    return (Clay_String) {
        .isStaticallyAllocated = false,
        .length = length,
        .chars = text,
    };
}

static void ensure_temp_text_capacity(int needed_capacity) {
    if (g_temp_text_capacity >= needed_capacity) {
        return;
    }

    int new_capacity = g_temp_text_capacity ? g_temp_text_capacity * 2 :
                       INITIAL_TEMP_TEXT_CAPACITY;
    while (new_capacity < needed_capacity) {
        new_capacity *= 2;
    }

    g_temp_text_buffers = realloc(g_temp_text_buffers, new_capacity * sizeof(char*));
    g_temp_text_capacity = new_capacity;
}

static void push_temp_text_buffer(char* buffer) {
    ensure_temp_text_capacity(g_temp_text_count + 1);
    g_temp_text_buffers[g_temp_text_count++] = buffer;
}

static void free_all_temp_text_buffers(void) {
    for (int i = 0; i < g_temp_text_count; ++i) {
        free(g_temp_text_buffers[i]);
    }
    g_temp_text_count = 0;
}

// ============================================================================
// TEXT RENDERING SYSTEM
// ============================================================================

static void render_text_elements(TextElement* elements, int count) {
    CLAY_AUTO_ID({
            .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .childGap = 0,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
            },
            .backgroundColor = COLOR_BACKGROUND,
            }) {
        CLAY_AUTO_ID({
                .layout = {
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 0,
                .sizing = { .width = CLAY_SIZING_GROW(0) }
                },
                .backgroundColor = COLOR_BACKGROUND,
                }) {
            for (int i = 0; i < count; ++i) {
                CLAY_TEXT(elements[i].string, elements[i].config);
            }
        }
    }
}

static void textline_init(void) {
    g_current_line.count = 0;
    g_current_line.char_count = 0;
}

static void textline_flush(void) {
    if (g_current_line.count == 0) {
        return;
    }

    render_text_elements(g_current_line.elements, g_current_line.count);
    g_current_line.count = 0;
    g_current_line.char_count = 0;
}

static void textline_push(const char* source, int length,
                          Clay_TextElementConfig* config) {
    if (g_current_line.count >= MAX_TEXT_ELEMENTS) {
        textline_flush();
    }

    char* buffer = malloc(length + 1);
    memcpy(buffer, source, length);
    buffer[length] = '\0';
    push_temp_text_buffer(buffer);

    g_current_line.elements[g_current_line.count].string = make_clay_string(buffer, length);
    g_current_line.elements[g_current_line.count].config = config;
    g_current_line.count++;
    g_current_line.char_count += length;

    if (g_current_line.char_count >= g_available_characters) {
        textline_flush();
    }
}

// ============================================================================
// FONT MANAGEMENT
// ============================================================================

typedef struct {
    int start;
    int end;
} CodepointRange;

static void reset_font_styles(void) {
    g_font_body_regular = (Clay_TextElementConfig) {
        .fontId = FONT_ID_REGULAR,
        .fontSize = g_base_font_size,
        .textColor = COLOR_FOREGROUND
    };

    g_font_body_italic = (Clay_TextElementConfig) {
        .fontId = FONT_ID_ITALIC,
        .fontSize = g_base_font_size,
        .textColor = COLOR_FOREGROUND
    };

    g_font_body_bold = (Clay_TextElementConfig) {
        .fontId = FONT_ID_BOLD,
        .fontSize = g_base_font_size,
        .textColor = COLOR_FOREGROUND
    };

    g_font_h1 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_EXTRABOLD,
        .fontSize = g_base_font_size + 14,
        .textColor = COLOR_FOREGROUND
    };

    g_font_h2 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_EXTRABOLD,
        .fontSize = g_base_font_size + 12,
        .textColor = COLOR_FOREGROUND
    };

    g_font_h3 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_EXTRABOLD,
        .fontSize = g_base_font_size + 6,
        .textColor = COLOR_FOREGROUND
    };

    g_font_h4 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_BOLD,
        .fontSize = g_base_font_size + 4,
        .textColor = COLOR_FOREGROUND
    };

    g_font_h5 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_ITALIC,
        .fontSize = g_base_font_size + 2,
        .textColor = COLOR_FOREGROUND
    };

    g_font_inline_code = (Clay_TextElementConfig) {
        .fontId = FONT_ID_REGULAR,
        .fontSize = g_base_font_size,
        .textColor = COLOR_BLUE
    };
}


static void load_font_with_ranges(int font_id, const char* font_path,
                                  const CodepointRange* ranges,
                                  int range_count,
                                  const int* additional_codepoints,
                                  int additional_count) {
    FT_Library freetype_lib;
    FT_Face face;

    FT_Init_FreeType(&freetype_lib);
    FT_New_Face(freetype_lib, font_path, 0, &face);

    int pixel_size = g_base_font_size * FONT_SCALE_FACTOR;
    FT_Set_Pixel_Sizes(face, 0, pixel_size);

    // Calculate total capacity needed
    int max_codepoint_count = 0;
    for (int i = 0; i < range_count; i++) {
        max_codepoint_count += (ranges[i].end - ranges[i].start + 1);
    }
    max_codepoint_count += additional_count;

    int* codepoints = malloc(sizeof(int) * max_codepoint_count);
    int valid_count = 0;

    // Add codepoints from ranges
    for (int i = 0; i < range_count; i++) {
        for (int codepoint = ranges[i].start; codepoint <= ranges[i].end; codepoint++) {
            if (FT_Get_Char_Index(face, codepoint)) {
                codepoints[valid_count++] = codepoint;
            }
        }
    }

    // Add additional codepoints
    for (int i = 0; i < additional_count; i++) {
        int codepoint = additional_codepoints[i];
        if (FT_Get_Char_Index(face, codepoint)) {
            // Check for duplicates
            int is_duplicate = 0;
            for (int j = 0; j < valid_count; j++) {
                if (codepoints[j] == codepoint) {
                    is_duplicate = 1;
                    break;
                }
            }

            if (!is_duplicate) {
                codepoints[valid_count++] = codepoint;
            }
        }
    }

    g_fonts[font_id] = LoadFontEx(font_path, pixel_size, codepoints, valid_count);

    free(codepoints);
    FT_Done_Face(face);
    FT_Done_FreeType(freetype_lib);

    SetTextureFilter(g_fonts[font_id].texture, TEXTURE_FILTER_BILINEAR);
}

static void load_emoji_font(int font_id, const char* font_path) {
    static const CodepointRange emoji_ranges[] = {
        {0x1F300, 0x1F5FF},
        {0x1F600, 0x1F64F},
        {0x1F680, 0x1F6FF},
        {0x1F700, 0x1F77F},
        {0x1F780, 0x1F7FF},
        {0x1F800, 0x1F8FF},
        {0x1F900, 0x1F9FF},
        {0x1FA00, 0x1FAFF},
        {0x2600,  0x26FF},
        {0x2700,  0x27BF},
    };

    load_font_with_ranges(font_id, font_path,
                          emoji_ranges,
                          sizeof(emoji_ranges) / sizeof(emoji_ranges[0]),
                          NULL, 0);
}

static void load_standard_font(int font_id, const char* font_path) {
    static const CodepointRange latin_ranges[] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
        {0x0180, 0x024F},   // Latin Extended-B
    };

    static const int special_codepoints[] = {
        0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA, 0x00F1,  // Lowercase
        0x00C1, 0x00C9, 0x00CD, 0x00D3, 0x00DA, 0x00D1   // Uppercase
    };

    load_font_with_ranges(font_id, font_path,
                          latin_ranges,
                          sizeof(latin_ranges) / sizeof(latin_ranges[0]),
                          special_codepoints,
                          sizeof(special_codepoints) / sizeof(special_codepoints[0]));
}

static void load_fonts(void) {
    load_standard_font(FONT_ID_REGULAR, "resources/NotoSans-Regular.ttf");
    load_standard_font(FONT_ID_ITALIC, "resources/NotoSans-Italic.ttf");
    load_standard_font(FONT_ID_SEMIBOLD, "resources/NotoSans-SemiBold.ttf");
    load_standard_font(FONT_ID_SEMIBOLD_ITALIC, "resources/NotoSans-SemiBoldItalic.ttf");
    load_standard_font(FONT_ID_BOLD, "resources/NotoSans-Bold.ttf");
    load_standard_font(FONT_ID_EXTRABOLD, "resources/NotoSans-ExtraBold.ttf");
    load_emoji_font(FONT_ID_EMOJI, "resources/NotoEmoji-Regular.ttf");

    Clay_SetMeasureTextFunction(Raylib_MeasureText, g_fonts);
    reset_font_styles();
}

// ============================================================================
// CLAY INITIALIZATION AND ERROR HANDLING
// ============================================================================

static void handle_clay_errors(Clay_ErrorData error_data) {
    printf("%s", error_data.errorText.chars);

    if (error_data.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) {
        Clay_SetMaxElementCount(Clay_GetMaxElementCount() * 2);
        exit(1);
    }
    else if (error_data.errorType == CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {
        Clay_SetMaxMeasureTextCacheWordCount(Clay_GetMaxMeasureTextCacheWordCount() * 2);
        exit(1);
    }
}

static void initialize_clay(void) {
    SetTraceLogLevel(LOG_WARNING);

    uint64_t total_memory_size = Clay_MinMemorySize();
    Clay_Arena clay_memory = Clay_CreateArenaWithCapacityAndMemory(
                                 total_memory_size,
                                 malloc(total_memory_size)
                             );

    Clay_Initialize(
        clay_memory,
    (Clay_Dimensions) {
        (float)GetScreenWidth(),
        (float)GetScreenHeight()
    },
    (Clay_ErrorHandler) {
        handle_clay_errors, 0
    }
    );

    Clay_Raylib_Initialize(1024, 768, "Markdown Viewer", FLAG_WINDOW_RESIZABLE);
    load_fonts();
}

// ============================================================================
// NODE RENDERING FUNCTIONS
// ============================================================================

static void render_node(MarkdownNode* current_node);

static void render_text_node(MarkdownNode* node) {
    const char* text = NULL;
    int length = 0;
    Clay_TextElementConfig* config = &g_font_body_regular;

    switch (node->type) {
    case NODE_TEXT:
        if (node->value.text.type == MD_TEXT_SOFTBR) {
            textline_push(" ", 1, &g_font_body_regular);
        }
        text = node->value.text.text;
        length = node->value.text.size;
        break;

    case NODE_SPAN:
        switch (node->value.span.type) {
        case MD_SPAN_EM:
            config = &g_font_body_italic;
            break;
        case MD_SPAN_STRONG:
            config = &g_font_body_bold;
            break;
        case MD_SPAN_CODE:
            config = &g_font_inline_code;
            break;
        default:
            return;
        }

        if (node->first_child && node->first_child->type == NODE_TEXT) {
            text = node->first_child->value.text.text;
            length = node->first_child->value.text.size;
        } else {
            return;
        }
        break;

    default:
        return;
    }

    int consumed = 0;
    while (consumed < length) {
        int available_space = g_available_characters - g_current_line.char_count;
        if (available_space <= 0) {
            textline_flush();
        }

        int remaining = length - consumed;
        int chunk_size = (remaining > available_space) ? available_space : remaining;
        textline_push(text + consumed, chunk_size, config);
        consumed += chunk_size;
    }
}

static void render_heading(MarkdownNode* node) {
    MD_BLOCK_H_DETAIL* detail = (MD_BLOCK_H_DETAIL*) node->value.block.detail;
    unsigned int level = detail->level;
    char* text = node->first_child->value.text.text;
    int size = node->first_child->value.text.size;

    Clay_TextElementConfig* config = NULL;
    switch (level) {
    case 1:
        config = &g_font_h1;
        break;
    case 2:
        config = &g_font_h2;
        break;
    case 3:
        config = &g_font_h3;
        break;
    case 4:
        config = &g_font_h4;
        break;
    default:
        config = &g_font_h5;
        break;
    }

    CLAY_TEXT(make_clay_string(text, size), config);
}

static void render_horizontal_rule(void) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { .width = CLAY_SIZING_GROW(0, CONTENT_WIDTH_PX * HR_SCALING_FACTOR) }
        },
        .border = { .width = { .top = 2 }, .color = COLOR_BLUE }
    }) {};
}

static void render_code_block(MarkdownNode* node) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 16, 16, 16, 16 }
        },
        .cornerRadius = 4,
        .backgroundColor = COLOR_DIM,
        .clip = {
            .vertical = false,
            .horizontal = true,
            .childOffset = Clay_GetScrollOffset()
        }
    }) {
        for (MarkdownNode* child = node->first_child; child; child = child->next_sibling) {
            CLAY_TEXT(
                make_clay_string(child->value.text.text, child->value.text.size),
                &g_font_body_regular
            );
        }
    };
}

static void render_quote_block(MarkdownNode* node) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 16, 16, 16, 16 }
        },
        .cornerRadius = 4,
        .border = { .width = { .left = 3 }, .color = COLOR_PINK },
        .clip = {
            .vertical = false,
            .horizontal = true,
            .childOffset = Clay_GetScrollOffset()
        }
    }) {
        for (MarkdownNode* child = node->first_child; child; child = child->next_sibling) {
            render_node(child);
        }
    }
}

static void render_ordered_list(MarkdownNode* current_node) {
    if (!current_node->first_child) {
        return;
    }

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 16, 0, 8, 8 }
        },
    }) {
        for (MarkdownNode* child = current_node->first_child; child;
                child = child->next_sibling) {
            render_node(child);
        }
    }
}

static void render_unordered_list(MarkdownNode* current_node) {
    if (!current_node->first_child) {
        return;
    }

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 16, 0, 8, 8 }
        },
    }) {
        for (MarkdownNode* child = current_node->first_child; child; child = child->next_sibling) {
            render_node(child);
        }
    }
}

static void render_list_item(MarkdownNode* current_node) {
    if (!current_node->first_child) return;

    textline_init();

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 0, 8, 0, 0 },
            .childGap = 4
        },
    }) {
        if (g_current_list_mode == LIST_MODE_ORDERED) {
            CLAY_TEXT(CLAY_STRING("*"), &g_font_body_regular);
        } else {
            CLAY_TEXT(CLAY_STRING("-"), &g_font_body_regular);
        }

        for (MarkdownNode* child = current_node->first_child; child; child = child->next_sibling) {
            render_node(child);
        }
        textline_flush();
    }

}

static void render_block(MarkdownNode* current_node) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = {1, 1, 1, 1},
            .childGap = 2,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
    }) {
        ListMode previous_list_mode = g_current_list_mode;

        switch (current_node->value.block.type) {
        case MD_BLOCK_P:
            textline_init();
            for (MarkdownNode* child = current_node->first_child; child;
                    child = child->next_sibling) {
                render_text_node(child);
            }
            textline_flush();
            break;

        case MD_BLOCK_H:
            render_heading(current_node);
            break;

        case MD_BLOCK_HR:
            render_horizontal_rule();
            break;

        case MD_BLOCK_CODE:
            render_code_block(current_node);
            break;

        case MD_BLOCK_QUOTE:
            render_quote_block(current_node);
            break;

        case MD_BLOCK_UL:
            g_current_list_mode = LIST_MODE_UNORDERED;
            render_unordered_list(current_node);
            break;

        case MD_BLOCK_OL:
            g_current_list_mode = LIST_MODE_ORDERED;
            render_ordered_list(current_node);
            break;

        case MD_BLOCK_LI:
            render_list_item(current_node);
            break;

        default:
            break;
        }

        g_current_list_mode = previous_list_mode;
    }
}

static void render_node(MarkdownNode* current_node) {
    switch (current_node->type) {
    case NODE_BLOCK:
        render_block(current_node);
        break;

    case NODE_TEXT:
    case NODE_SPAN:
        render_text_node(current_node);
        break;
    }
}

// ============================================================================
// MAIN LAYOUT AND RENDERING
// ============================================================================

static Clay_RenderCommandArray render_markdown_tree(void) {
    MarkdownNode* root_node = get_root_node();

    Clay_BeginLayout();

    int left_padding = (int)(GetScreenWidth() / LEFT_PADDING_DIVISOR);

    CLAY(CLAY_ID(MAIN_LAYOUT_ID), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = { left_padding, 0, 46, 56 },
            .childGap = 16,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
            .sizing = {
                .width = CLAY_SIZING_GROW(0),
                .height = CLAY_SIZING_GROW(0)
            }
        },
        .backgroundColor = COLOR_BACKGROUND,
        .clip = {
            .vertical = true,
            .horizontal = false,
            .childOffset = Clay_GetScrollOffset()
        }
    }) {
        for (MarkdownNode* child = root_node->first_child; child; child = child->next_sibling) {
            render_node(child);
        }
    }

    return Clay_EndLayout();
}

// ============================================================================
// SCROLL INPUT HANDLING
// ============================================================================

// TODO: make the scroll behavior change acording to the screen size and make it more fluid
#define SCROLL_KEY_REPEAT_START_DELAY 0.25f
#define SCROLL_KEY_REPEAT_DELAY 0.10f
#define SCROLL_KEY_SPEED 1.2f
#define SCROLL_MULTIPLIER 4.9f

typedef struct {
    KeyboardKey key;
    Vector2 direction;
    float timer;
    bool repeating;
    float speed_multiplier;
} ScrollKey;

static ScrollKey g_scroll_keys[] = {
    {.key = KEY_J, .direction = {0, -1}, .timer = 0, .repeating = false, .speed_multiplier = 1.0f},  // down
    {.key = KEY_K, .direction = {0,  1}, .timer = 0, .repeating = false, .speed_multiplier = 1.0f},  // up
    {.key = KEY_H, .direction = {-1, 0}, .timer = 0, .repeating = false, .speed_multiplier = 1.0f},  // left
    {.key = KEY_L, .direction = {1,  0}, .timer = 0, .repeating = false, .speed_multiplier = 1.0f},  // right
    {.key = KEY_D, .direction = {0, -1}, .timer = 0, .repeating = false, .speed_multiplier = 6.2f},  // half page down
    {.key = KEY_U, .direction = {0,  1}, .timer = 0, .repeating = false, .speed_multiplier = 6.2f}   // half page up
};

static void handle_vim_scroll_motions(void) {
    float delta_time = GetFrameTime();
    Vector2 scroll_delta = {0};

    for (int i = 0; i < (int)(sizeof(g_scroll_keys) / sizeof(g_scroll_keys[0])); i++) {
        ScrollKey *k = &g_scroll_keys[i];

        if (IsKeyDown(k->key)) {
            k->timer += delta_time;
            if (!k->repeating) {
                // First press should actuate inmediatelly
                if (k->timer == delta_time) {
                    scroll_delta.x += k->direction.x * SCROLL_KEY_SPEED * k->speed_multiplier;
                    scroll_delta.y += k->direction.y * SCROLL_KEY_SPEED * k->speed_multiplier;
                }
                // Repetetion starts after the initial start_delay
                else if (k->timer >= SCROLL_KEY_REPEAT_START_DELAY) {
                    k->repeating = true;
                    k->timer = 0;
                }
            } else {
                // The key is now repited (with less delay)
                if (k->timer >= SCROLL_KEY_REPEAT_DELAY) {
                    scroll_delta.x += k->direction.x * SCROLL_KEY_SPEED * k->speed_multiplier;
                    scroll_delta.y += k->direction.y * SCROLL_KEY_SPEED * k->speed_multiplier;
                    k->timer = 0;
                }
            }
        } else {
            k->timer = 0;
            k->repeating = false;
        }
    }

    // Update CLAY scroll containers
    if (scroll_delta.x != 0 || scroll_delta.y != 0) {
        Clay_UpdateScrollContainers(
            true,
        (Clay_Vector2) {
            scroll_delta.x, scroll_delta.y * SCROLL_MULTIPLIER
        },
        delta_time
        );
    }
}

// ============================================================================
// MAIN LOOP AND APPLICATION CONTROL
// ============================================================================

static void update_frame(void) {
    // Handle debug toggle
    if (IsKeyPressed(KEY_BACKSPACE)) {
        g_debug_enabled = !g_debug_enabled;
        Clay_SetDebugModeEnabled(g_debug_enabled);
    }

    // Handle font size changes
    if (IsKeyPressed(KEY_EQUAL)) {
        g_base_font_size += 2;
        reset_font_styles();
    }
    if (IsKeyPressed(KEY_MINUS)) {
        g_base_font_size -= 2;
        reset_font_styles();
    }

    // Update layout dimensions for window resizing
    Clay_SetLayoutDimensions((Clay_Dimensions) {
        .width = GetScreenWidth(),
        .height = GetScreenHeight()
    });

    // Calculate available characters per line
    g_available_characters = (int)(GetScreenWidth() / (g_base_font_size / 2));

    // Update input state
    Vector2 mouse_position = GetMousePosition();
    Clay_SetPointerState(
    (Clay_Vector2) {
        mouse_position.x, mouse_position.y
    },
    IsMouseButtonDown(0)
    );

    // Handle mouse wheel scrolling
    Vector2 scroll_delta = GetMouseWheelMoveV();
    if (scroll_delta.x != 0 || scroll_delta.y != 0) {
        Clay_UpdateScrollContainers(
            true,
        (Clay_Vector2) {
            scroll_delta.x, scroll_delta.y * SCROLL_MULTIPLIER
        },
        GetFrameTime()
        );
    } else {
        // Handle vim-style keyboard scrolling
        handle_vim_scroll_motions();
    }

    // Generate render commands
    Clay_RenderCommandArray render_commands = render_markdown_tree();

    // Render frame
    BeginDrawing();
    ClearBackground(WHITE);
    Clay_Raylib_Render(render_commands, g_fonts, FONT_ID_EMOJI);

    // Clean up temporary text buffers
    free_all_temp_text_buffers();

    EndDrawing();
}

void start_main_loop(void) {
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_Q)) {
            break;
        }

        if (IsKeyPressed(KEY_EQUAL)) {
            g_base_font_size += 1;
            reset_font_styles();
        }

        if (IsKeyPressed(KEY_MINUS)) {
            g_base_font_size -= 1;
            reset_font_styles();
        }

        if (IsKeyPressed(KEY_ZERO)) {
            g_base_font_size = BASE_FONT_SIZE;
            reset_font_styles();
        }

        update_frame();
    }

    CloseWindow();
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void cleanup_application(void) {
    if (g_temp_text_buffers) {
        free_all_temp_text_buffers();
        free(g_temp_text_buffers);
        g_temp_text_buffers = NULL;
    }
}

void initialize_application(void) {
    initialize_clay();
    start_main_loop();
    cleanup_application();
}
