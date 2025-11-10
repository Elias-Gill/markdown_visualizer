// NOTE: keep it on top of the file in that specific order
#define CLAY_IMPLEMENTATION
#include "clay/clay.h"
#include "clay/clay_renderer_raylib.c"

#include "parser.h"
#include "md4c/md4c.h"

#include "render.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

// Color palette
#ifdef WHITE_MODE
#define COLOR_BACKGROUND (Clay_Color){244, 244, 244, 255}  // #f4f4f4
#define COLOR_FOREGROUND (Clay_Color){34, 34, 34, 255}     // #222222
#define COLOR_DIM        (Clay_Color){200, 200, 200, 180}  // #c8c8c8
#define COLOR_PINK       (Clay_Color){177, 68, 130, 255}   // #b14482
#define COLOR_BLUE       (Clay_Color){60, 163, 116, 255}   // #3CA374
#define COLOR_ORANGE     (Clay_Color){210, 132, 79, 255}   // #d2844f
#define COLOR_BORDER     (Clay_Color){199, 199, 199, 255}  // #c7c7c7
#define COLOR_DARK       (Clay_Color){232, 232, 232, 255}  // #e8e8e8
#define COLOR_HOVER      (Clay_Color){221, 221, 221, 255}  // #dddddd
#define COLOR_HIGHLIGHT  (Clay_Color){210, 210, 213, 255}  // #d2d2d5
#else // ----- DARK MODE -----
#define COLOR_BACKGROUND (Clay_Color){34, 35, 35, 255}     // #222323
#define COLOR_FOREGROUND (Clay_Color){222, 222, 222, 255}  // #dedede
#define COLOR_DIM        (Clay_Color){60, 60, 60, 190}     // #3C3C3C
#define COLOR_PINK       (Clay_Color){196, 146, 177, 255}  // #C492b1
#define COLOR_BLUE       (Clay_Color){151, 215, 189, 255}  // #97D7BD
#define COLOR_ORANGE     (Clay_Color){230, 185, 157, 255}  // #e6b99d
#define COLOR_BORDER     (Clay_Color){88, 88, 88, 255}     // #585858
#define COLOR_DARK       (Clay_Color){25, 25, 25, 255}     // #191919
#define COLOR_HOVER      (Clay_Color){47, 47, 47, 255}     // #2f2f2f
#define COLOR_HIGHLIGHT  (Clay_Color){59, 59, 62, 255}     // #3b3b3e
#endif

// Utility macros
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }
#define MAIN_LAYOUT_ID "main_layout"
bool g_debug_enabled = true;
#define CONTENT_WIDTH_PX (GetScreenWidth() * 0.95f)

#define LIST_MODE_ORDERED   1
#define LIST_MODE_UNORDERED 0
int list_mode = LIST_MODE_ORDERED;

// ------------- Fonts --------------
const uint32_t FONT_ID_REGULAR          = 0;
const uint32_t FONT_ID_ITALIC           = 1;
const uint32_t FONT_ID_SEMIBOLD         = 2;
const uint32_t FONT_ID_SEMIBOLD_ITALIC  = 3;
const uint32_t FONT_ID_BOLD             = 4;
const uint32_t FONT_ID_EXTRABOLD        = 5;
const uint32_t FONT_ID_EMOJI            = 6;
Font g_fonts[7];
int base_font_size = 22;

// Text styles
Clay_TextElementConfig font_body_regular;
Clay_TextElementConfig font_body_italic;
Clay_TextElementConfig font_body_bold;
Clay_TextElementConfig font_h1;
Clay_TextElementConfig font_h2;
Clay_TextElementConfig font_h3;
Clay_TextElementConfig font_h4;
Clay_TextElementConfig font_h5;
Clay_TextElementConfig inline_code;

// ============================================================================
// TEXT MANIPULATION FUNCTIONS
// ============================================================================

int available_characters = 0;

typedef struct {
    Clay_String string;
    Clay_TextElementConfig *config;
} TextElement;

typedef struct {
    TextElement elements[256];
    int count;
    int char_count;
} TextLine;

static TextLine line;

// Forward declarations
static inline Clay_String make_clay_string(char *text, long length);
static void render_text_elements(TextElement *elements, int count);

// Temporary text buffers
static char **g_temp_text_buffers = NULL;
static int g_temp_text_count = 0;
static int g_temp_text_capacity = 0;

static void ensure_temp_text_capacity(int needed) {
    if (g_temp_text_capacity >= needed) return;
    int newcap = g_temp_text_capacity ? g_temp_text_capacity * 2 : 256;
    while (newcap < needed) newcap *= 2;
    g_temp_text_buffers = realloc(g_temp_text_buffers, newcap * sizeof(char*));
    g_temp_text_capacity = newcap;
}

static void push_temp_text_buffer(char *buf) {
    ensure_temp_text_capacity(g_temp_text_count + 1);
    g_temp_text_buffers[g_temp_text_count++] = buf;
}

static void free_all_temp_text_buffers(void) {
    for (int i = 0; i < g_temp_text_count; ++i)
        free(g_temp_text_buffers[i]);
    g_temp_text_count = 0;
}

static void textline_init(void) {
    line.count = 0;
    line.char_count = 0;
}

static void textline_flush(void) {
    if (line.count == 0) return;
    render_text_elements(line.elements, line.count);
    line.count = 0;
    line.char_count = 0;
}

static void textline_push(const char *src, int len,
                          Clay_TextElementConfig *config) {
    if (line.count >= 256)
        textline_flush();

    char *buf = malloc(len + 1);
    memcpy(buf, src, len);
    buf[len] = '\0';
    push_temp_text_buffer(buf);

    line.elements[line.count].string = make_clay_string(buf, len);
    line.elements[line.count].config = config;
    line.count++;
    line.char_count += len;

    if (line.char_count >= available_characters)
        textline_flush();
}

// ============================================================================
// INIT FUNCTIONS
// ============================================================================

void reset_font_styles() {
    // Init font styles
    font_body_regular = (Clay_TextElementConfig) {
        .fontId = FONT_ID_REGULAR, .fontSize = base_font_size, .textColor = COLOR_FOREGROUND
    };
    font_body_italic = (Clay_TextElementConfig) {
        .fontId = FONT_ID_ITALIC, .fontSize = base_font_size, .textColor = COLOR_FOREGROUND
    };
    font_body_bold = (Clay_TextElementConfig) {
        .fontId = FONT_ID_BOLD, .fontSize = base_font_size, .textColor = COLOR_FOREGROUND
    };
    font_h1 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_EXTRABOLD, .fontSize = base_font_size + 14,
                                     .textColor = COLOR_FOREGROUND
    };
    font_h2 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_EXTRABOLD, .fontSize = base_font_size + 12,
                                     .textColor = COLOR_FOREGROUND
    };
    font_h3 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_EXTRABOLD, .fontSize = base_font_size + 6,
                                     .textColor = COLOR_FOREGROUND
    };
    font_h4 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_BOLD, .fontSize = base_font_size + 4, .textColor = COLOR_FOREGROUND
    };
    font_h5 = (Clay_TextElementConfig) {
        .fontId = FONT_ID_ITALIC, .fontSize = base_font_size + 2, .textColor = COLOR_FOREGROUND
    };
    inline_code = (Clay_TextElementConfig) {
        .fontId = FONT_ID_REGULAR, .fontSize = base_font_size, .textColor = COLOR_BLUE
    };
}

void load_emoji_font(int id, const char *font_path) {
    FT_Library ft;
    FT_Face face;
    FT_Init_FreeType(&ft);
    FT_New_Face(ft, font_path, 0, &face);

    int size = base_font_size * 2;
    FT_Set_Pixel_Sizes(face, 0, size);

    static const struct {
        int start, end;
    } emoji_ranges[] = {
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

    int maxCount = 0;
    for (int i = 0; i < (int)(sizeof(emoji_ranges) / sizeof(emoji_ranges[0])); i++)
        maxCount += (emoji_ranges[i].end - emoji_ranges[i].start + 1);

    int *codepoints = (int *)malloc(sizeof(int) * maxCount);
    int valid = 0;

    for (int i = 0; i < (int)(sizeof(emoji_ranges) / sizeof(emoji_ranges[0])); i++) {
        for (int cp = emoji_ranges[i].start; cp <= emoji_ranges[i].end; cp++) {
            if (FT_Get_Char_Index(face, cp))
                codepoints[valid++] = cp;
        }
    }

    g_fonts[id] = LoadFontEx(font_path, size, codepoints, valid);

    free(codepoints);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    SetTextureFilter(g_fonts[id].texture, TEXTURE_FILTER_BILINEAR);
    reset_font_styles();
}

void load_font(int id, const char *font_path) {
    FT_Library ft;
    FT_Face face;
    FT_Init_FreeType(&ft);
    FT_New_Face(ft, font_path, 0, &face);

    int size = base_font_size * 2;
    FT_Set_Pixel_Sizes(face, 0, size);

    // Rangos básicos y extendidos latinos
    static const struct {
        int start, end;
    } latin_ranges[] = {
        {0x0020, 0x007E},   // ASCII
        {0x00A0, 0x00FF},   // Latin-1 Supplement
        {0x0100, 0x017F},   // Latin Extended-A
        {0x0180, 0x024F},   // Latin Extended-B
    };

    int maxCount = 0;
    for (int i = 0; i < (int)(sizeof(latin_ranges)/sizeof(latin_ranges[0])); i++)
        maxCount += (latin_ranges[i].end - latin_ranges[i].start + 1);

    int *codepoints = (int *)malloc(sizeof(int) * maxCount);
    int valid = 0;

    // Agregar todos los codepoints de los rangos
    for (int i = 0; i < (int)(sizeof(latin_ranges)/sizeof(latin_ranges[0])); i++) {
        for (int cp = latin_ranges[i].start; cp <= latin_ranges[i].end; cp++) {
            if (FT_Get_Char_Index(face, cp))
                codepoints[valid++] = cp;
        }
    }

    // Asegurar los precompuestos comunes (áéíóúñ ÁÉÍÓÚÑ)
    static const int special[] = {
        0x00E1,0x00E9,0x00ED,0x00F3,0x00FA,0x00F1,  // minúsculas
        0x00C1,0x00C9,0x00CD,0x00D3,0x00DA,0x00D1   // mayúsculas
    };
    for (size_t k = 0; k < sizeof(special)/sizeof(special[0]); k++) {
        int cp = special[k];
        if (FT_Get_Char_Index(face, cp)) {
            int already = 0;
            for (int t = 0; t < valid; t++) if (codepoints[t] == cp) {
                    already = 1;
                    break;
                }
            if (!already) codepoints[valid++] = cp;
        }
    }

    g_fonts[id] = LoadFontEx(font_path, size, codepoints, valid);

    free(codepoints);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    SetTextureFilter(g_fonts[id].texture, TEXTURE_FILTER_BILINEAR);
    reset_font_styles();
}

void load_fonts(void) {
    load_font(FONT_ID_REGULAR, "resources/NotoSans-Regular.ttf");
    load_font(FONT_ID_ITALIC, "resources/NotoSans-Italic.ttf");
    load_font(FONT_ID_SEMIBOLD, "resources/NotoSans-SemiBold.ttf");
    load_font(FONT_ID_SEMIBOLD_ITALIC, "resources/NotoSans-SemiBoldItalic.ttf");
    load_font(FONT_ID_BOLD, "resources/NotoSans-Bold.ttf");
    load_font(FONT_ID_EXTRABOLD, "resources/NotoSans-ExtraBold.ttf");
    load_emoji_font(FONT_ID_EMOJI, "resources/NotoEmoji-Regular.ttf");

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
    SetTraceLogLevel(LOG_WARNING);

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
// RENDERING AND REUSABLE COMPONENTS
// ============================================================================

void render_node(MarkdownNode *current_node);

// Creates a Clay_String from a C string
static inline Clay_String make_clay_string(char *text, long length) {
    return (Clay_String) {
        .isStaticallyAllocated = false,
        .length = length,
        .chars = text,
    };
}

static void render_text_elements(TextElement *elements, int count) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = 0,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
    }) {
        for (int i = 0; i < count; ++i) {
            // config is stored as pointer to global config; dereference for CLAY_TEXT
            CLAY_TEXT(elements[i].string, elements[i].config);
        }
    }
}

static void render_text_node(MarkdownNode *node) {
    const char *text = NULL;
    int len = 0;
    Clay_TextElementConfig *config = &font_body_regular;

    switch (node->type) {
    case NODE_TEXT:
        if (node->value.text.type == MD_TEXT_SOFTBR) {
            textline_push(" ", 1, &font_body_regular);
        }
        text = node->value.text.text;
        len = node->value.text.size;
        break;

    case NODE_SPAN:
        switch (node->value.span.type) {
        case MD_SPAN_EM:
            config = &font_body_italic;
            break;
        case MD_SPAN_STRONG:
            config = &font_body_bold;
            break;
        case MD_SPAN_CODE:
            config = &inline_code;
            break;
        default:
            return;
        }
        if (node->first_child && node->first_child->type == NODE_TEXT) {
            text = node->first_child->value.text.text;
            len = node->first_child->value.text.size;
        } else  {
            return;
        }
        break;

    default:
        return;
    }

    int consumed = 0;
    while (consumed < len) {
        int space = available_characters - line.char_count;
        if (space <= 0)
            textline_flush();

        int remaining = len - consumed;
        int take = remaining > space ? space : remaining;
        textline_push(text + consumed, take, config);
        consumed += take;
    }
}

static void render_heading(MarkdownNode *node) {
    MD_BLOCK_H_DETAIL *detail = (MD_BLOCK_H_DETAIL*) node->value.block.detail;
    unsigned level = detail->level;
    char *text = node->first_child->value.text.text;
    int size = node->first_child->value.text.size;

    Clay_TextElementConfig *cfg =
        level == 1 ? &font_h1 :
        level == 2 ? &font_h2 :
        level == 3 ? &font_h3 :
        level == 4 ? &font_h4 : &font_h5;

    CLAY_TEXT(make_clay_string(text, size), cfg);
}

static void render_hr(void) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            // WARNING: The 0.8 scaling factor is arbitrary
            .sizing = { .width = CLAY_SIZING_GROW(0, CONTENT_WIDTH_PX * 0.8) }
        },
        .backgroundColor = COLOR_BACKGROUND,
        .border = { .width = { .top = 2 }, .color = COLOR_DIM }
    }) {};
}

static void render_code_block(MarkdownNode *node) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 16, 16, 16, 16 }
        },
        .cornerRadius = 4,
        .backgroundColor = COLOR_DIM,
        .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() }
    }) {
        for (MarkdownNode *child = node->first_child; child; child = child->next_sibling) {
            CLAY_TEXT(make_clay_string(child->value.text.text, child->value.text.size),
                      &font_body_regular);
        }
    };
}

static void render_quote_block(MarkdownNode *node) {
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 16, 16, 16, 16 }
        },
        .cornerRadius = 4,
        .border = { .width = { .left = 3 }, .color = COLOR_PINK },
        .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() }
    }) {
        for (MarkdownNode *child = node->first_child; child; child = child->next_sibling) {
            render_node(child);
        }
    };
}

static void render_ordered_list(MarkdownNode *current_node) {
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
        for (MarkdownNode *child = current_node->first_child; child;
                child = child->next_sibling) {
            render_node(current_node->first_child);
        }
    }
}

static void render_unordered_list(MarkdownNode *current_node) {
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
        for (MarkdownNode *child = current_node->first_child; child;
                child = child->next_sibling) {
            render_node(child);
        }
    }
}

static void render_list_item(MarkdownNode *current_node) {
    textline_init();
    if (!current_node->first_child) {
        return;
    }

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
            .padding = { 0, 8, 0, 0 }
        },
    }) {

        if (list_mode == LIST_MODE_ORDERED) {
            CLAY_TEXT(CLAY_STRING("* "), &font_body_regular);
        } else {
            CLAY_TEXT(CLAY_STRING("- "), &font_body_regular);
        }

        CLAY_AUTO_ID({
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { .width = CLAY_SIZING_FIT(0, CONTENT_WIDTH_PX) },
                .padding = { 0, 8, 0, 0 }
            },
        }) {
            for (MarkdownNode *child = current_node->first_child; child;
                    child = child->next_sibling) {
                render_node(child);
            }
        }
    }

    textline_flush();
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
        int last_mode = list_mode;

        switch (current_node->value.block.type) {
        case MD_BLOCK_P:
            textline_init();
            for (MarkdownNode *child = current_node->first_child; child;
                    child = child->next_sibling) {
                render_text_node(child);
            }
            textline_flush();
            break;
        case MD_BLOCK_H:
            render_heading(current_node);
            break;
        case MD_BLOCK_HR:
            render_hr();
            break;
        case MD_BLOCK_CODE:
            render_code_block(current_node);
            break;
        case MD_BLOCK_QUOTE:
            render_quote_block(current_node);
            break;
        case MD_BLOCK_UL:
            list_mode = LIST_MODE_UNORDERED;
            render_unordered_list(current_node);
            break;
        case MD_BLOCK_OL:
            list_mode = LIST_MODE_ORDERED;
            render_ordered_list(current_node);
            break;
        case MD_BLOCK_LI:
            render_list_item(current_node);
            break;
        default:
            break;
        }

        list_mode = last_mode;
    }
}

void render_node(MarkdownNode *current_node) {
    switch(current_node->type) {
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
// MAIN LAYOUT
// ============================================================================

// Renders the complete markdown tree using Clay layout system
Clay_RenderCommandArray render_markdown_tree(void) {
    MarkdownNode *node = get_root_node();

    Clay_BeginLayout();

    // The apps main container
    int left_pad = (int) (GetScreenWidth() / 8); // Why 8 ? I don't know
    CLAY(CLAY_ID(MAIN_LAYOUT_ID), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = { left_pad, 0, 46, 56 },
            .childGap = 16,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) }
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
        for (MarkdownNode *child = node->first_child; child; child = child->next_sibling) {
            render_node(child);
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
    available_characters = (int) (GetScreenWidth() / (base_font_size / 2));

    // Update scroll containers
    Vector2 mousePosition = GetMousePosition();
    Clay_SetPointerState(
    (Clay_Vector2) {
        mousePosition.x, mousePosition.y
    },
    IsMouseButtonDown(0)
    );
    Vector2 scrollDelta = GetMouseWheelMoveV();
    Clay_UpdateScrollContainers(
        true,
    (Clay_Vector2) {
        scrollDelta.x, scrollDelta.y * 4.5
    },
    GetFrameTime()
    );

    // Generate the auto layout for rendering
    Clay_RenderCommandArray render_commands = render_markdown_tree();

    // RENDERING ---------------------------------
    BeginDrawing();
    ClearBackground(WHITE);
    Clay_Raylib_Render(render_commands, g_fonts, FONT_ID_EMOJI);

    // Now it's safe to free all temporary text buffers created during layout
    free_all_temp_text_buffers();

    EndDrawing();
}

void start_main_loop() {
    while(!WindowShouldClose()) {
        if (IsKeyPressed(KEY_Q)) {
            break;
        }
        if (IsKeyPressed(KEY_EQUAL)) {
            base_font_size += 2;
            reset_font_styles();
        }
        if (IsKeyPressed(KEY_MINUS)) {
            base_font_size -= 2;
            reset_font_styles();
        }
        update_frame();
    }
    CloseWindow();
}
