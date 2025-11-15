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

#include <stdbool.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

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
#define COLOR_BLUE       (Clay_Color){120, 180, 190, 200}  // #78B4BEC8
#define COLOR_ORANGE     (Clay_Color){230, 140, 50, 255}   // #E68C32
#define COLOR_BORDER     (Clay_Color){60, 60, 60, 255}     // #3C3C3C
#define COLOR_DARK       (Clay_Color){20, 20, 20, 255}     // #141414
#define COLOR_HOVER      (Clay_Color){45, 45, 45, 255}     // #2D2D2D
#define COLOR_HIGHLIGHT  (Clay_Color){48, 60, 75, 255}     // #303C4B
#endif

// Utility macros
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }
#define MAIN_LAYOUT_ID "main_layout"
#define IMG_SCALING_FACTOR 0.6f

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

static char g_resource_path[PATH_MAX];

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

// --- Text rendering system ---

// Represents the amount of chars that can be displayed inside a single line of text with the
// current screen size. It is calculated on every loop cicle using the screen size and the font
// size.
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

// Images storage
typedef struct {
    char *path;
    unsigned path_size;
    bool is_image_loaded;
    Texture2D image;
} ImageInfo;

// This array stores the image data to then be freed when the app is cleaned
// I mean, more than 200 images inside a markdown file is just absurd man.
ImageInfo images[256];
int images_array_pointer = -1;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// --- RESOURCES INITIALIZATION ---

void init_resource_path(const char *app_root) {
    char exe_path[PATH_MAX];
    realpath(app_root, exe_path);
    char *dir = dirname(exe_path);
    snprintf(g_resource_path, sizeof(g_resource_path), "%s/resources", dir);
}

// --- TEXT MANIPULATION FUNCTIONS ---

static inline Clay_String make_clay_string(char* text, long length) {
    return (Clay_String) {
        .isStaticallyAllocated = false,
        .length = length,
        .chars = text,
    };
}

static inline Clay_String make_clay_string_copy(const char* text, size_t length) {
    char* copy = malloc(length);
    if (!copy) exit(1);
    memcpy(copy, text, length);
    return (Clay_String) {
        .isStaticallyAllocated = false,
        .length = length,
        .chars = copy,
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

// --- IMAGE LOADING FUNCTIONS ---

// Search for an image inside the images array, if not found, then loads it and returns the
// pointer to that element.
ImageInfo* find_or_load_image(const char *raw_path, unsigned path_size) {
    // Copy image path
    char path[512];
    if (path_size >= sizeof(path)) {
        fprintf(stderr, "Image URL too long\n");
        exit(1);
    }
    memcpy(path, raw_path, path_size);
    path[path_size] = '\0';

    // Search for the image if already loaded
    for (int i = 0; i <= images_array_pointer; i++) {
        if (strcmp(path, images[i].path) == 0) {
            return &images[i];
        }
    }

    // Load the image inside the images array
    images_array_pointer++;
    if (images_array_pointer == 256) {
        fprintf(stderr, "You have too many images inside this file.\n");
        exit(1);
    }

    // Duplicate the path string to store it
    char *stored_path = strdup(path);
    if (!stored_path) {
        perror("Error duplicating image path str: strdup");
        exit(1);
    }

    Texture2D image;

    bool is_image_loaded = true;
    if (access(path, F_OK) == 0) {
        image = LoadTexture(path);
        printf("Loaded image: '%.*s'\n", path_size, path);
    } else {
        printf("Cannot load image: '%.*s'\n", path_size, path);
        is_image_loaded = false;
    }

    images[images_array_pointer] = (ImageInfo) {
        .image = image,
        .path = stored_path,
        .path_size = path_size,
        .is_image_loaded = is_image_loaded
    };

    return &images[images_array_pointer];
}

// Cleans the images array, unloading textures and temporary path strings.
void clean_images_array() {
    if (images_array_pointer < 0) {
        return;
    }
    for (int i = 0; i < images_array_pointer; i++) {
        ImageInfo info = images[i];
        if (info.is_image_loaded && info.image.id > 0) {
            UnloadTexture(info.image);
        }
        if (info.path) {
            free(info.path);
        }
    }
    images_array_pointer = -1;
}

// ============================================================================
// TEXT RENDERING SYSTEM
// ============================================================================

static void textline_init(void) {
    g_current_line.count = 0;
    g_current_line.char_count = 0;
}

static void textline_flush() {
    if (g_current_line.count == 0) {
        return;
    }

    // Line container
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = 0,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
    }) {
        // render each text element
        for (int i = 0; i < g_current_line.count; ++i) {
            CLAY_TEXT(g_current_line.elements[i].string, g_current_line.elements[i].config);
        }
    }

    g_current_line.count = 0;
    g_current_line.char_count = 0;
}

static void textline_push(const char* source, int length,
                          Clay_TextElementConfig* config) {
    // Disable clays text wrapping
    config->wrapMode = CLAY_TEXT_WRAP_NONE;

    // If line is full, flush before pushing more
    if (g_current_line.count >= MAX_TEXT_ELEMENTS) {
        textline_flush();
    }

    int remaining = g_available_characters - g_current_line.char_count;

    // Check if adding this text exceeds max characters allowed
    if (g_current_line.char_count + length > g_available_characters) {
        // Find last space within the allowed range to wrap
        int wrap_pos = -1;
        int max_len = remaining;
        for (int i = max_len; i > 0; i--) {
            if (source[i-1] == ' ') {
                wrap_pos = i;
                break;
            }
        }

        // If there is no space and before wrapping in the middle of a word, try placing the
        // entire chunk on a new line (only works if it fits in an empty line).
        if (wrap_pos == -1 && length <= g_available_characters) {
            textline_flush();
            textline_push(source, length, config);
            return;
        }

        // If no space found and it cannot be place onto a new line, just break at max_len
        if (wrap_pos == -1) {
            wrap_pos = max_len;
        }

        // Push first part
        textline_push(source, wrap_pos, config);
        textline_flush();

        // Push remainder recursively (skip space if any)
        int remainder_start = wrap_pos;
        while (remainder_start < length && source[remainder_start] == ' ') {
            remainder_start++;
        }
        if (remainder_start < length) {
            textline_push(source + remainder_start, length - remainder_start, config);
        }
        return;
    }

    // Normal push if space is available
    char* buffer = malloc(length + 1);
    memcpy(buffer, source, length);
    buffer[length] = '\0';
    push_temp_text_buffer(buffer);

    g_current_line.elements[g_current_line.count].string = make_clay_string(buffer, length);
    g_current_line.elements[g_current_line.count].config = config;
    g_current_line.count++;
    g_current_line.char_count += length;

    // Flush if reached limit exactly
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
        {0x1F300, 0x1F5FF}, // Miscellaneous and Pictographs: objects, nature, and symbols
        {0x1F600, 0x1F64F}, // Facial expressions and human emotions
        {0x1F680, 0x1F6FF}, // Transport and Map Symbols
        {0x1F700, 0x1F77F}, // Alchemical Symbols
        {0x1F780, 0x1F7FF}, // Geometric Shapes Extended
        {0x1F800, 0x1F8FF}, // Supplemental Arrows
        {0x1F900, 0x1F9FF}, // Supplemental Symbols and Pictographs
        {0x1FA00, 0x1FAFF}, // Symbols and Pictographs Extended-A
        {0x2600,  0x26FF},  // More miscellaneous Symbols (like weather)
        {0x2700,  0x27BF},  // Decorative marks and bullet-like symbols
        {0x25A0,  0x25FF},  // Geometric Shapes
        {0x2B50,  0x2B55},  // More miscellaneous symbols and arrows
        {0x1F536, 0x1F53F}, // Colored geometric shapes
        {0x1F7E0, 0x1F7EB}, // Geometric Shapes Extended subset
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
        {0x2000, 0x206F},   // List bullet points
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
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/NotoSans-Regular.ttf", g_resource_path);
    load_standard_font(FONT_ID_REGULAR, path);
    snprintf(path, sizeof(path), "%s/NotoSans-Italic.ttf", g_resource_path);
    load_standard_font(FONT_ID_ITALIC, path);
    snprintf(path, sizeof(path), "%s/NotoSans-SemiBold.ttf", g_resource_path);
    load_standard_font(FONT_ID_SEMIBOLD, path);
    snprintf(path, sizeof(path), "%s/NotoSans-SemiBoldItalic.ttf", g_resource_path);
    load_standard_font(FONT_ID_SEMIBOLD_ITALIC, path);
    snprintf(path, sizeof(path), "%s/NotoSans-Bold.ttf", g_resource_path);
    load_standard_font(FONT_ID_BOLD, path);
    snprintf(path, sizeof(path), "%s/NotoSans-ExtraBold.ttf", g_resource_path);
    load_standard_font(FONT_ID_EXTRABOLD, path);
    snprintf(path, sizeof(path), "%s/NotoEmoji-Regular.ttf", g_resource_path);
    load_emoji_font(FONT_ID_EMOJI, path);

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

static void render_node(MarkdownNode* current_node, float available_width);

static void render_text_node(MarkdownNode* node, float available_width) {
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
        case MD_SPAN_A:
        /*MD_ATTRIBUTE href;*/
        /*MD_ATTRIBUTE title;*/
        /*int is_autolink;            [> nonzero if this is an autolink <]*/
        /*MD_SPAN_A_DETAIL;*/
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

    textline_push(text, length, config);
}

static void render_heading(MarkdownNode* node, float available_width) {
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

    // This adds a little dinamyc padding to the right
    float content_width = available_width * 0.95;
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0, content_width) }
        },
    }) {
        CLAY_TEXT(make_clay_string(text, size), config);
    };
}

static void render_horizontal_rule(float available_width) {
    // Adds a little dinamyc padding to the right
    float content_width = available_width * 0.95;
    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { .width = CLAY_SIZING_GROW(0, content_width) }
        },
        .border = { .width = { .top = 2 }, .color = COLOR_BLUE }
    }) {};
}

static void render_code_block(MarkdownNode* node, float available_width) {
    const float padding_top = 16;
    const float padding_right = 16;
    const float padding_bottom = 16;
    const float padding_left = 16;

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, available_width) },
            .padding = { padding_top, padding_right, padding_bottom, padding_left }
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

static void render_quote_block(MarkdownNode* node, float available_width) {
    const float padding_top = 16;
    const float padding_right = 16;
    const float padding_bottom = 16;
    const float padding_left = 16;

    // Total horizontal padding = left + right
    const float total_horizontal_padding = padding_left + padding_right;

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, available_width) },
            .padding = { padding_top, padding_right, padding_bottom, padding_left }
        },
        .cornerRadius = 4,
        .border = { .width = { .left = 3 }, .color = COLOR_PINK },
        .clip = {
            .vertical = false,
            .horizontal = true,
            .childOffset = Clay_GetScrollOffset()
        }
    }) {
        float content_width = available_width - total_horizontal_padding;

        for (MarkdownNode* child = node->first_child; child; child = child->next_sibling) {
            render_node(child, content_width);
        }
    }
}

int list_item_index = 1;

static void render_ordered_list(MarkdownNode* current_node, float available_width) {
    if (!current_node->first_child) {
        return;
    }

    // List starting index
    int previous_index = list_item_index;
    MD_BLOCK_OL_DETAIL* detail = (MD_BLOCK_OL_DETAIL*) current_node->value.block.detail;
    list_item_index = detail->start;

    const float padding_top = 8;
    const float padding_right = 0;
    const float padding_bottom = 8;
    const float padding_left = 8;
    const float child_gap = 8;

    // Total horizontal padding = left + right
    const float total_horizontal_padding = padding_left + padding_right;

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, available_width) },
            .padding = { padding_top, padding_right, padding_bottom, padding_left },
            .childGap = child_gap,
        },
    }) {
        float content_width = available_width - total_horizontal_padding;

        for (MarkdownNode* child = current_node->first_child; child;
                child = child->next_sibling) {
            render_node(child, content_width);
        }
    }

    // Reset to the previous index
    list_item_index = previous_index;
}

static void render_unordered_list(MarkdownNode* current_node, float available_width) {
    if (!current_node->first_child) {
        return;
    }

    const float padding_top = 8;
    const float padding_right = 0;
    const float padding_bottom = 8;
    const float padding_left = 8;
    const float child_gap = 8;

    // Total horizontal padding = left + right
    const float total_horizontal_padding = padding_left + padding_right;

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { .width = CLAY_SIZING_FIT(0, available_width) },
            .padding = { padding_top, padding_right, padding_bottom, padding_left },
            .childGap = child_gap,
        },
    }) {
        // Subtract horizontal padding from available width
        float content_width = available_width - total_horizontal_padding;

        for (MarkdownNode* child = current_node->first_child; child;
                child = child->next_sibling) {
            render_node(child, content_width);
        }
    }
}

static void render_list_item(MarkdownNode* current_node, float available_width) {
    if (!current_node->first_child) return;

    const float padding_left = 8;
    const float child_gap = 8;
    const float bullet_and_padding = g_base_font_size * 4 + 16 + padding_left + child_gap;
    // Calculate available width for text content subtracting bullet space and padding
    float text_available_width = available_width - bullet_and_padding;

    textline_init();

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { .width = CLAY_SIZING_FIT(0, available_width) },
            .padding = { 0, padding_left, 0, 0 },
            .childGap = child_gap
        },
    }) {
        if (g_current_list_mode == LIST_MODE_ORDERED) {
            CLAY_AUTO_ID({
                .layout = {
                    .padding = { 8, 8, 4, 4 },
                },
                .backgroundColor = COLOR_BLUE
            }) {
                char index[4];
                int len = sprintf(index, "%d", list_item_index);
                CLAY_TEXT(make_clay_string_copy(index, len), &g_font_body_bold);
                list_item_index++;
            }
        } else {
            CLAY_TEXT(CLAY_STRING("‣"), &g_font_body_bold);
        }

        CLAY_AUTO_ID({
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { .width = CLAY_SIZING_FIT(0, available_width) },
            },
        }) {
            for (MarkdownNode* child = current_node->first_child; child;
                    child = child->next_sibling) {
                render_node(child, text_available_width);
            }
            textline_flush();
        }
    }
}

static void render_image(MarkdownNode *node, float available_width) {
    MD_SPAN_IMG_DETAIL *detail = (MD_SPAN_IMG_DETAIL*) node->value.span.detail;
    MD_ATTRIBUTE src = detail->src;
    MD_ATTRIBUTE title = detail->src;

    ImageInfo *info = find_or_load_image(src.text, src.size);

    float content_width = available_width * IMG_SCALING_FACTOR;

    // Display the image
    if (info->is_image_loaded) {
        CLAY_AUTO_ID({
            .layout = {
                .childAlignment = CLAY_ALIGN_X_CENTER,
                .sizing = { .width = CLAY_SIZING_FIXED(content_width) },
                .padding = {content_width / 6, 0, 28, 28},
            },
        }) {
            float max_width = content_width;
            float original_width = (float)info->image.width;
            float original_height = (float)info->image.height;

            // Scale the image if too big, if not, then keep the original size
            float width = (original_width > content_width) ? content_width : original_width;
            width = width - content_width / 6; // Apply the containers padding to the image

            // Scale height to keep the image ratio
            float height = original_height * (width / original_width);

            CLAY_AUTO_ID({
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height) }
                },
                .image = { .imageData = &info->image }
            }) { }
        }
    } else {
        CLAY_AUTO_ID({
            .layout = {
                .padding = {content_width / 6, 0, 28, 28},
            }
        }) {
            CLAY_TEXT(CLAY_STRING("🖼 Image not loaded"), &g_font_body_bold);
        }
    }
}

static void render_paragraph(MarkdownNode* current_node, float available_width) {
    const float padding = 1;
    const float child_gap = 2;
    const float total_spacing = padding * 2 + child_gap;

    CLAY_AUTO_ID({
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = {padding, padding, padding, padding},
            .childGap = child_gap,
            .sizing = { .width = CLAY_SIZING_GROW(0) }
        },
        .backgroundColor = COLOR_BACKGROUND,
    }) {
        textline_init();
        for (MarkdownNode* child = current_node->first_child; child;
                child = child->next_sibling) {
            render_node(child, available_width);
        }
        textline_flush();
    }
}

static void render_block(MarkdownNode* current_node, float available_width) {
    ListMode previous_list_mode = g_current_list_mode;

    switch (current_node->value.block.type) {
    case MD_BLOCK_P:
        render_paragraph(current_node, available_width);
        break;

    case MD_BLOCK_H:
        render_heading(current_node, available_width);
        break;

    case MD_BLOCK_HR:
        render_horizontal_rule(available_width);
        break;

    case MD_BLOCK_CODE:
        render_code_block(current_node, available_width);
        break;

    case MD_BLOCK_QUOTE:
        render_quote_block(current_node, available_width);
        break;

    case MD_BLOCK_UL:
        // Flush father elements after rendering inner lists if present.
        textline_flush();
        g_current_list_mode = LIST_MODE_UNORDERED;
        render_unordered_list(current_node, available_width);
        break;

    case MD_BLOCK_OL:
        g_current_list_mode = LIST_MODE_ORDERED;
        textline_flush();
        render_ordered_list(current_node, available_width);
        break;

    case MD_BLOCK_LI:
        render_list_item(current_node, available_width);
        break;

    default:
        // Just ignore the node
        break;
    }

    g_current_list_mode = previous_list_mode;
}

static void render_node(MarkdownNode* current_node, float available_width) {
    NodeType type = current_node->type;
    if (type == NODE_BLOCK) {
        render_block(current_node, available_width);
    } else if (type == NODE_SPAN && current_node->value.span.type == MD_SPAN_IMG) {
        render_image(current_node, available_width);
    } else if (type == NODE_SPAN || type == NODE_TEXT) {
        render_text_node(current_node, available_width);
    }
}

// ============================================================================
// MAIN LAYOUT AND RENDERING
// ============================================================================

static Clay_RenderCommandArray render_markdown_tree(void) {
    MarkdownNode* root_node = get_root_node();

    Clay_BeginLayout();

    int left_padding = (int)(GetScreenWidth() / 6.5); // Why 6.5 ? I don't know.
    int right_padding = (int)(GetScreenWidth() / 7); // Same here, but looks nice.
    float available_width = GetScreenWidth() - left_padding - right_padding;

    // Main app container
    CLAY(CLAY_ID(MAIN_LAYOUT_ID), {
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = { left_padding, 0, 46, right_padding },
            .childGap = 16,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT },
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
            render_node(child, available_width);
        }
    }

    return Clay_EndLayout();
}

// ============================================================================
// SCROLL INPUT HANDLING (Adaptive + Smooth)
// ============================================================================

#define SCROLL_KEY_REPEAT_START_DELAY 0.28f
#define SCROLL_KEY_REPEAT_DELAY       0.10f
#define SCROLL_MULTIPLIER             4.5f

typedef struct {
    int key;
    Vector2 direction;
    float timer;
    bool repeating;
    float screen_portion; // fraction of screen height to scroll per trigger
} ScrollKey;

static ScrollKey g_scroll_keys[] = {
    {.key = KEY_J, .direction = {0, -1}, .timer = 0, .repeating = false, .screen_portion = 0.03f},   // small step down
    {.key = KEY_K, .direction = {0,  1}, .timer = 0, .repeating = false, .screen_portion = 0.03f},   // small step up
    {.key = KEY_H, .direction = {1,  0}, .timer = 0, .repeating = false, .screen_portion = 0.03f},   // left
    {.key = KEY_L, .direction = {-1, 0}, .timer = 0, .repeating = false, .screen_portion = 0.03f},   // right
    {.key = KEY_D, .direction = {0, -1}, .timer = 0, .repeating = false, .screen_portion = 0.095f},  // half page down
    {.key = KEY_U, .direction = {0,  1}, .timer = 0, .repeating = false, .screen_portion = 0.095f},  // half page up
};

static void handle_vim_scroll_motions(void) {
    float screen_height = GetScreenHeight();
    float delta_time = GetFrameTime();
    Vector2 scroll_delta = {0};

    for (int i = 0; i < (int)(sizeof(g_scroll_keys) / sizeof(g_scroll_keys[0])); i++) {
        ScrollKey *k = &g_scroll_keys[i];

        // First press acts immediately
        if (IsKeyPressed(k->key)) {
            float scroll_pixels = screen_height * k->screen_portion;
            scroll_delta.x += k->direction.x * scroll_pixels;
            scroll_delta.y += k->direction.y * scroll_pixels;
            k->timer = 0;
            k->repeating = false;
        }
        // Handle hold + repeat
        else if (IsKeyDown(k->key)) {
            k->timer += delta_time;

            if (!k->repeating && k->timer >= SCROLL_KEY_REPEAT_START_DELAY) {
                k->repeating = true;
                k->timer = 0;
            }

            if (k->repeating && k->timer >= SCROLL_KEY_REPEAT_DELAY) {
                float scroll_pixels = screen_height * k->screen_portion;
                scroll_delta.x += k->direction.x * scroll_pixels;
                scroll_delta.y += k->direction.y * scroll_pixels;
                k->timer = 0;
            }
        } else {
            k->timer = 0;
            k->repeating = false;
        }
    }

    // Single-key "g" (top) and "G" (bottom) scroll mapping
    if (IsKeyPressed(KEY_G)) {
        bool shift_held = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (shift_held) {
            scroll_delta.y -= screen_height * 100.0f; // scroll to bottom
        } else {
            scroll_delta.y = screen_height * 100.0f;  // scroll to top
        }
    }

    // Higher smoothing_factor => faster interpolation response
    const float smoothing_factor = 15.0f;
    static Vector2 smoothed_scroll = {0};

    smoothed_scroll.x += (scroll_delta.x - smoothed_scroll.x) * smoothing_factor * delta_time;
    smoothed_scroll.y += (scroll_delta.y - smoothed_scroll.y) * smoothing_factor * delta_time;

    if (fabsf(smoothed_scroll.x) > 0.01f || fabsf(smoothed_scroll.y) > 0.01f) {
        Clay_UpdateScrollContainers(
            true,
        (Clay_Vector2) {
            smoothed_scroll.x, smoothed_scroll.y
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
    g_available_characters = (int)(GetScreenWidth() / (g_base_font_size / 2) - 1);

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
    clean_images_array();
}

void initialize_application(char *app_root) {
    init_resource_path(app_root);
    initialize_clay();
    start_main_loop();
    cleanup_application();
    CloseWindow();
}
