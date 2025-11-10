#include "raylib.h"
#include "raymath.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "wchar.h"

#define SUPPORT_FILEFORMAT_TTF
#define SUPPORT_FONT_SDF

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle) (Rectangle) { .x = rectangle.x, .y = rectangle.y, .width = rectangle.width, .height = rectangle.height }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color) (Color) { .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g), .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a) }

Camera Raylib_camera;

typedef enum
{
    CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL
} CustomLayoutElementType;

typedef struct
{
    Model model;
    float scale;
    Vector3 position;
    Matrix rotation;
} CustomLayoutElement_3DModel;

typedef struct
{
    CustomLayoutElementType type;
    union {
        CustomLayoutElement_3DModel model;
    } customData;
} CustomLayoutElement;

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera camera, int screenWidth,
                                       int screenHeight, float zDistance)
{
    Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f*position.x)/(float)screenWidth - 1.0f;
    float y = 1.0f - (2.0f*position.y)/(float)screenHeight;
    float z = 1.0f;

    // Store values in a vector
    Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD,
                                    ((double)screenWidth/(double)screenHeight), 0.01f, zDistance);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        double aspect = (double)screenWidth/(double)screenHeight;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    Vector3 nearPoint = Vector3Unproject((Vector3) {
        deviceCoords.x, deviceCoords.y, 0.0f
    }, matProj, matView);
    Vector3 farPoint = Vector3Unproject((Vector3) {
        deviceCoords.x, deviceCoords.y, 1.0f
    }, matProj, matView);

    // Calculate normalized direction vector
    Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    ray.position = farPoint;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}

// Función para obtener el siguiente carácter UTF-8
static int GetNextUTF8Char(const char* text, int* index, int length) {
    if (*index >= length) return -1;

    unsigned char c = (unsigned char)text[*index];
    int codepoint = 0;

    if (c < 0x80) {
        codepoint = c;
        (*index)++;
    } else if (c < 0xE0) {
        if (*index + 1 >= length) return -1;
        codepoint = ((c & 0x1F) << 6) | (text[*index + 1] & 0x3F);
        *index += 2;
    } else if (c < 0xF0) {
        if (*index + 2 >= length) return -1;
        codepoint = ((c & 0x0F) << 12) | ((text[*index + 1] & 0x3F) << 6) |
                    (text[*index + 2] & 0x3F);
        *index += 3;
    } else {
        if (*index + 3 >= length) return -1;
        codepoint = ((c & 0x07) << 18) | ((text[*index + 1] & 0x3F) << 12) | ((
                        text[*index + 2] & 0x3F) << 6) | (text[*index + 3] & 0x3F);
        *index += 4;
    }

    return codepoint;
}

static inline Clay_Dimensions Raylib_MeasureText(Clay_StringSlice text,
        Clay_TextElementConfig *config, void *userData) 
{
    Font *fonts = (Font *)userData;
    Font font = fonts[config->fontId];
    if (!font.glyphs) font = GetFontDefault();

    const float scale = config->fontSize / (float)font.baseSize;
    const float spacing = config->letterSpacing * scale;

    float maxWidth = 0.0f;
    float lineWidth = 0.0f;
    float height = config->fontSize;

    int index = 0;
    const int len = text.length;

    while (index < len) {
        int codepoint = GetNextUTF8Char(text.chars, &index, len);
        if (codepoint == -1) break;

        if (codepoint == '\n') {
            if (lineWidth > maxWidth) maxWidth = lineWidth;
            lineWidth = 0.0f;
            height += config->fontSize;
            continue;
        }

        int glyphIndex = GetGlyphIndex(font, codepoint);
        float adv = 0.0f;

        if (glyphIndex >= 0 && glyphIndex < font.glyphCount) {
            adv = font.glyphs[glyphIndex].advanceX;
            if (adv == 0.0f)
                adv = font.recs[glyphIndex].width + font.glyphs[glyphIndex].offsetX;
        } else {
            adv = font.baseSize * 0.8f; // ancho estimado fallback
        }

        lineWidth += adv * scale + spacing;
    }

    if (lineWidth > maxWidth) maxWidth = lineWidth;

    return (Clay_Dimensions){ maxWidth, height };
}

void Clay_Raylib_Initialize(int width, int height, const char *title,
                            unsigned int flags) {
    SetConfigFlags(flags);
    InitWindow(width, height, title);

    // Configurar para soportar UTF-8
    SetExitKey(0); // Deshabilitar tecla de salida por defecto
}

static char *temp_render_buffer = NULL;
static int temp_render_buffer_len = 0;

void Clay_Raylib_Close() {
    if(temp_render_buffer) free(temp_render_buffer);
    temp_render_buffer_len = 0;
    CloseWindow();
}

static bool is_emoji_codepoint(int cp) {
    return (cp >= 0x1F300 && cp <= 0x1F9FF) ||  // símbolos, pictogramas
           (cp >= 0x2600 && cp <= 0x26FF) ||    // misc symbols
           (cp >= 0x2700 && cp <= 0x27BF) ||    // dingbats
           (cp == 0x00A9) || (cp == 0x00AE) ||  // copyright
           (cp >= 0x203C && cp <= 0x3299);      // otros emojis comunes
}

void Clay_Raylib_Render(Clay_RenderCommandArray renderCommands, Font* fonts,
                        int emoji_font_index) {
    for (int j = 0; j < renderCommands.length; j++) {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, j);
        Clay_BoundingBox boundingBox = {roundf(renderCommand->boundingBox.x), roundf(renderCommand->boundingBox.y), roundf(renderCommand->boundingBox.width), roundf(renderCommand->boundingBox.height)};

        switch (renderCommand->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *textData = &renderCommand->renderData.text;
            Font baseFont = fonts[textData->fontId];
            Font emojiFont = fonts[emoji_font_index];

            float x = boundingBox.x;
            float y = boundingBox.y;
            float fontSize = (float)textData->fontSize;
            float spacing = (float)textData->letterSpacing;
            Color color = CLAY_COLOR_TO_RAYLIB_COLOR(textData->textColor);

            const char *p = textData->stringContents.chars;
            int remaining = textData->stringContents.length;

            while (remaining > 0) {
                int bytes = 0;
                int cp = GetCodepointNext(p, &bytes);
                if (bytes <= 0) break;

                bool isEmoji = is_emoji_codepoint(cp);
                Font *font = isEmoji ? &emojiFont : &baseFont;

                int glyphIndex = GetGlyphIndex(*font, cp);
                if (glyphIndex < 0 || glyphIndex >= font->glyphCount
                        || font->glyphs[glyphIndex].advanceX == 0) {
                    font = isEmoji ? &baseFont : &emojiFont;  // fallback cruzado
                    glyphIndex = GetGlyphIndex(*font, cp);
                    if (glyphIndex < 0 || glyphIndex >= font->glyphCount
                            || font->glyphs[glyphIndex].advanceX == 0) {
                        DrawTextEx(GetFontDefault(), "�", (Vector2) {
                            x, y
                        }, fontSize, spacing, color);
                        x += fontSize * 0.6f + spacing;
                        p += bytes;
                        remaining -= bytes;
                        continue;
                    }
                }

                DrawTextCodepoint(*font, cp, (Vector2) {
                    x, y
                }, fontSize, color);

                float advance = font->glyphs[glyphIndex].advanceX;
                if (advance == 0) {
                    Rectangle rec = font->recs[glyphIndex];
                    advance = rec.width + font->glyphs[glyphIndex].offsetX;
                }
                x += advance * (fontSize / font->baseSize) + spacing;

                p += bytes;
                remaining -= bytes;
            }
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Texture2D imageTexture = *(Texture2D *)renderCommand->renderData.image.imageData;
            Clay_Color tintColor = renderCommand->renderData.image.backgroundColor;
            if (tintColor.r == 0 && tintColor.g == 0 && tintColor.b == 0 && tintColor.a == 0) {
                tintColor = (Clay_Color) {
                    255, 255, 255, 255
                };
            }
            DrawTexturePro(
                imageTexture,
            (Rectangle) {
                0, 0, imageTexture.width, imageTexture.height
            },
            (Rectangle) {
                boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height
            },
            (Vector2) {},
            0,
            CLAY_COLOR_TO_RAYLIB_COLOR(tintColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y),
                             (int)roundf(boundingBox.width), (int)roundf(boundingBox.height));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            EndScissorMode();
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
            if (config->cornerRadius.topLeft > 0) {
                float radius = (config->cornerRadius.topLeft * 2) / (float)((boundingBox.width >
                               boundingBox.height) ? boundingBox.height : boundingBox.width);
                DrawRectangleRounded((Rectangle) {
                    boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height
                }, radius, 8, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
            } else {
                DrawRectangle(boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height,
                              CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *config = &renderCommand->renderData.border;
            // Left border
            if (config->width.left > 0) {
                DrawRectangle((int)roundf(boundingBox.x),
                              (int)roundf(boundingBox.y + config->cornerRadius.topLeft), (int)config->width.left,
                              (int)roundf(boundingBox.height - config->cornerRadius.topLeft -
                                          config->cornerRadius.bottomLeft), CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            // Right border
            if (config->width.right > 0) {
                DrawRectangle((int)roundf(boundingBox.x + boundingBox.width - config->width.right),
                              (int)roundf(boundingBox.y + config->cornerRadius.topRight), (int)config->width.right,
                              (int)roundf(boundingBox.height - config->cornerRadius.topRight -
                                          config->cornerRadius.bottomRight), CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            // Top border
            if (config->width.top > 0) {
                DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.topLeft),
                              (int)roundf(boundingBox.y),
                              (int)roundf(boundingBox.width - config->cornerRadius.topLeft -
                                          config->cornerRadius.topRight), (int)config->width.top,
                              CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            // Bottom border
            if (config->width.bottom > 0) {
                DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.bottomLeft),
                              (int)roundf(boundingBox.y + boundingBox.height - config->width.bottom),
                              (int)roundf(boundingBox.width - config->cornerRadius.bottomLeft -
                                          config->cornerRadius.bottomRight), (int)config->width.bottom,
                              CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.topLeft > 0) {
                DrawRing((Vector2) {
                    roundf(boundingBox.x + config->cornerRadius.topLeft),
                           roundf(boundingBox.y + config->cornerRadius.topLeft)
                }, roundf(config->cornerRadius.topLeft - config->width.top), config->cornerRadius.topLeft,
                180, 270, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.topRight > 0) {
                DrawRing((Vector2) {
                    roundf(boundingBox.x + boundingBox.width - config->cornerRadius.topRight),
                           roundf(boundingBox.y + config->cornerRadius.topRight)
                }, roundf(config->cornerRadius.topRight - config->width.top),
                config->cornerRadius.topRight, 270, 360, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.bottomLeft > 0) {
                DrawRing((Vector2) {
                    roundf(boundingBox.x + config->cornerRadius.bottomLeft),
                           roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomLeft)
                }, roundf(config->cornerRadius.bottomLeft - config->width.bottom),
                config->cornerRadius.bottomLeft, 90, 180, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            if (config->cornerRadius.bottomRight > 0) {
                DrawRing((Vector2) {
                    roundf(boundingBox.x + boundingBox.width - config->cornerRadius.bottomRight),
                           roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomRight)
                }, roundf(config->cornerRadius.bottomRight - config->width.bottom),
                config->cornerRadius.bottomRight, 0.1, 90, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            Clay_CustomRenderData *config = &renderCommand->renderData.custom;
            CustomLayoutElement *customElement = (CustomLayoutElement *)config->customData;
            if (!customElement) continue;
            switch (customElement->type) {
            case CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL: {
                Clay_BoundingBox rootBox = renderCommands.internalArray[0].boundingBox;
                float scaleValue = CLAY__MIN(CLAY__MIN(1, 768 / rootBox.height) * CLAY__MAX(1,
                                             rootBox.width / 1024), 1.5f);
                Ray positionRay = GetScreenToWorldPointWithZDistance((Vector2) {
                    renderCommand->boundingBox.x + renderCommand->boundingBox.width / 2,
                                  renderCommand->boundingBox.y + (renderCommand->boundingBox.height / 2) + 20
                }, Raylib_camera, (int)roundf(rootBox.width), (int)roundf(rootBox.height), 140);
                BeginMode3D(Raylib_camera);
                DrawModel(customElement->customData.model.model, positionRay.position,
                          customElement->customData.model.scale * scaleValue, WHITE);
                EndMode3D();
                break;
            }
            default:
                break;
            }
            break;
        }
        default: {
            printf("Error: unhandled render command.");
            exit(1);
        }
        }
    }
}
