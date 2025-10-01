// NOTE: mantener siempre en ese orden y arriba del todo
#define CLAY_IMPLEMENTATION
#include "clay/clay.h"
#include "clay/clay_renderer_raylib.c"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

#define COLOR_ORANGE (Clay_Color) {225, 138, 50, 255}
#define COLOR_BLUE (Clay_Color) {111, 173, 162, 255}

bool debugEnabled = false;

Texture2D profilePicture;
#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }


const uint32_t FONT_ID_BODY_16 = 1;
const uint32_t FONT_ID_BODY_24 = 0;
Font fonts[2];

void LoadFonts() {
    // prepare used fonts
    fonts[FONT_ID_BODY_24] = LoadFontEx("resources/Roboto-Regular.ttf", 48, 0, 400);
    SetTextureFilter(fonts[FONT_ID_BODY_24].texture, TEXTURE_FILTER_BILINEAR);

    fonts[FONT_ID_BODY_16] = LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400);
    SetTextureFilter(fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);

    Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);
}

// =============================
// COMPONENTES REUTILIZABLES
// =============================

// Texto genérico
void RenderParagraph(Clay_String text, int fontId, int fontSize, Clay_Color color) {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({
                .fontId = fontId,
                .fontSize = fontSize,
                .textColor = color
                }));
}

// Texto centrado con lineHeight
void RenderCenteredText(Clay_String text) {
    CLAY_TEXT(text, CLAY_TEXT_CONFIG({
                .fontSize = 24,
                .lineHeight = 60,
                .textColor = (Clay_Color){0,0,0,255},
                .textAlignment = CLAY_TEXT_ALIGN_CENTER
                }));
}

// Imagen cuadrada fija
void RenderSquarePicture(int size, Texture2D *tex) {
    CLAY_AUTO_ID({
            .layout = { 
            .sizing = { 
            .width = CLAY_SIZING_FIXED(size), 
            .height = CLAY_SIZING_FIXED(size) 
            } 
            },
            .image = { .imageData = tex }
            }) {}
}

// Imagen con aspecto fijo
void RenderAspectPicture(int size, Texture2D *tex) {
    CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(size) }},
            .aspectRatio = 1,
            .image = { .imageData = tex }
            }) {}
}

// Galería con varias imágenes
void RenderGallery(Texture2D *tex) {
    CLAY_AUTO_ID({
            .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0) },
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            .childGap = 16,
            .padding = {16, 16, 16, 16}
            },
            .backgroundColor = {180, 180, 220, 255}
            }) {
        RenderAspectPicture(120, tex);

        CLAY_AUTO_ID({
                .layout = {
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .padding = {8, 8, 8, 8}
                },
                .backgroundColor = {170, 170, 220, 255}
                }) {
            RenderSquarePicture(60, tex);
            RenderParagraph(
                    CLAY_STRING("Image caption below"), 
                    FONT_ID_BODY_24, 24, (Clay_Color){0,0,0,255}
                    );
        }

        RenderAspectPicture(120, tex);
    }
}

// Bloque “Photos2” (ejemplo con hover)
void RenderPhotoBlock(Texture2D *tex) {
    CLAY_AUTO_ID({
            .layout = { .childGap = 16, .padding = { 16, 16, 16, 16 }},
            .backgroundColor = {180, 180, 220, Clay_Hovered() ? 120 : 255}
            }) {
        RenderSquarePicture(120, tex);
    }
}

// =============================
// LAYOUT PRINCIPAL
// =============================
Clay_RenderCommandArray CreateLayout() {
    Clay_BeginLayout();

    CLAY_AUTO_ID({
            .layout = { 
            .layoutDirection = CLAY_TOP_TO_BOTTOM, 
            .padding = {16, 16, 16, 16}, 
            .childGap = 16, 
            .sizing = { .width = CLAY_SIZING_GROW(0) }
            },
            .backgroundColor = {200, 200, 255, 255},
            .clip = { .vertical = true, .childOffset = (Clay_Vector2){0,0} },
            }) {
        RenderParagraph(
                CLAY_STRING("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt."),
                FONT_ID_BODY_24, 24, (Clay_Color){0,0,0,255}
                );

        RenderPhotoBlock(&profilePicture);
        RenderCenteredText(CLAY_STRING("un texto mas largo aun"));
        RenderParagraph(CLAY_STRING("mas texto enrome"), FONT_ID_BODY_24, 24, (Clay_Color){0,0,0,255});
        RenderGallery(&profilePicture);
        RenderParagraph(CLAY_STRING("un texto enorme"), FONT_ID_BODY_24, 24, (Clay_Color){0,0,0,255});
    }

    return Clay_EndLayout();
}

void UpdateDrawFrame(Font* fonts) {
    if (IsKeyPressed(KEY_D)) {
        debugEnabled = !debugEnabled;
        Clay_SetDebugModeEnabled(debugEnabled);
    }

    Clay_Vector2 mousePosition = RAYLIB_VECTOR2_TO_CLAY_VECTOR2(GetMousePosition());
    Clay_SetPointerState(mousePosition, IsMouseButtonDown(0));

    Clay_SetLayoutDimensions((Clay_Dimensions) {
        (float)GetScreenWidth(), (float)GetScreenHeight()
    });

    // Generate the auto layout for rendering
    Clay_RenderCommandArray renderCommands = CreateLayout();

    // RENDERING ---------------------------------
    BeginDrawing();
    ClearBackground(BLACK);
    Clay_Raylib_Render(renderCommands, fonts);
    EndDrawing();
    //----------------------------------------------------------------------------------
}

void HandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);
    if (errorData.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) {
        Clay_SetMaxElementCount(Clay_GetMaxElementCount() * 2);
        exit(1);
    } else if (errorData.errorType == CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {
        Clay_SetMaxMeasureTextCacheWordCount(Clay_GetMaxMeasureTextCacheWordCount() * 2);
        exit(1);
    }
}

int main(void) {
    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize,
                            malloc(totalMemorySize));
    Clay_Initialize(
        clayMemory,
    (Clay_Dimensions) {
        (float)GetScreenWidth(),
        (float)GetScreenHeight()
    },
    (Clay_ErrorHandler) {
        HandleClayErrors, 0
    });

    Clay_Raylib_Initialize(1024, 768, "Markdown Viewer", FLAG_WINDOW_RESIZABLE);
    LoadFonts();

    //--------------------------------------------------------------------------------------
    // Main game loop
    while (!WindowShouldClose()) {
        UpdateDrawFrame(fonts);
    }

    Clay_Raylib_Close();
    return 0;
}
