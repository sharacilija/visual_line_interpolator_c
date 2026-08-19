#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include <raylib.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 760
#define MIN_WINDOW_WIDTH 900
#define MIN_WINDOW_HEIGHT 600

#define MAX_SEGMENTS 21
#define MIN_BOUNCES 1
#define MAX_BOUNCES 20
#define DEFAULT_BOUNCES 8

#define POINT_RADIUS 13.0f
#define POINT_HIT_RADIUS 24.0f
#define PI_F 3.14159265358979323846f

typedef struct {
    Vector2 start;
    Vector2 end;
    float length;
} RaySegment;

typedef struct {
    float hue;
    const char *name;
} BeamPalette;

static float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static Vector2 AddVectors(Vector2 a, Vector2 b)
{
    return (Vector2){a.x + b.x, a.y + b.y};
}

static Vector2 SubtractVectors(Vector2 a, Vector2 b)
{
    return (Vector2){a.x - b.x, a.y - b.y};
}

static Vector2 ScaleVector(Vector2 vector, float scale)
{
    return (Vector2){vector.x * scale, vector.y * scale};
}

static float VectorLength(Vector2 vector)
{
    return sqrtf(vector.x * vector.x + vector.y * vector.y);
}

static Vector2 NormalizeVector(Vector2 vector)
{
    const float length = VectorLength(vector);

    if (length < 0.0001f) {
        return (Vector2){1.0f, 0.0f};
    }

    return ScaleVector(vector, 1.0f / length);
}

static float DistanceSquared(Vector2 a, Vector2 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

static Vector2 ClampPointToRectangle(Vector2 point, Rectangle rectangle, float padding)
{
    point.x = ClampFloat(point.x, rectangle.x + padding,
                         rectangle.x + rectangle.width - padding);
    point.y = ClampFloat(point.y, rectangle.y + padding,
                         rectangle.y + rectangle.height - padding);
    return point;
}

static Color WithAlpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

static Color PaletteColor(float baseHue, int segmentIndex)
{
    float hue = fmodf(baseHue + (float)segmentIndex * 10.0f, 360.0f);

    if (hue < 0.0f) {
        hue += 360.0f;
    }

    return ColorFromHSV(hue, 0.78f, 1.0f);
}

static int TraceRicochets(Vector2 origin, Vector2 aim, Rectangle bounds,
                          int bounceCount, RaySegment segments[MAX_SEGMENTS])
{
    const float epsilon = 0.001f;
    Vector2 direction = NormalizeVector(SubtractVectors(aim, origin));
    Vector2 current = ClampPointToRectangle(origin, bounds, 1.0f);
    int segmentCount = 0;

    if (DistanceSquared(origin, aim) < 4.0f) {
        return 0;
    }

    for (int index = 0; index <= bounceCount && index < MAX_SEGMENTS; ++index) {
        float timeToVertical = 1000000.0f;
        float timeToHorizontal = 1000000.0f;

        if (direction.x > epsilon) {
            timeToVertical = (bounds.x + bounds.width - current.x) / direction.x;
        } else if (direction.x < -epsilon) {
            timeToVertical = (bounds.x - current.x) / direction.x;
        }

        if (direction.y > epsilon) {
            timeToHorizontal = (bounds.y + bounds.height - current.y) / direction.y;
        } else if (direction.y < -epsilon) {
            timeToHorizontal = (bounds.y - current.y) / direction.y;
        }

        const float travelTime = fminf(timeToVertical, timeToHorizontal);
        const bool hitVertical = fabsf(timeToVertical - travelTime) < 0.05f;
        const bool hitHorizontal = fabsf(timeToHorizontal - travelTime) < 0.05f;
        const Vector2 hit = AddVectors(current, ScaleVector(direction, travelTime));

        segments[segmentCount] = (RaySegment){
            .start = current,
            .end = hit,
            .length = VectorLength(SubtractVectors(hit, current))
        };
        ++segmentCount;

        if (hitVertical) {
            direction.x *= -1.0f;
        }
        if (hitHorizontal) {
            direction.y *= -1.0f;
        }

        current = AddVectors(hit, ScaleVector(direction, 0.05f));
    }

    return segmentCount;
}

static float TotalPathLength(const RaySegment segments[MAX_SEGMENTS], int segmentCount)
{
    float total = 0.0f;

    for (int index = 0; index < segmentCount; ++index) {
        total += segments[index].length;
    }

    return total;
}

static Vector2 PointAlongPath(const RaySegment segments[MAX_SEGMENTS], int segmentCount,
                              float distance, int *segmentIndex)
{
    float remaining = distance;

    for (int index = 0; index < segmentCount; ++index) {
        if (remaining <= segments[index].length || index == segmentCount - 1) {
            const float amount = segments[index].length > 0.0f
                                     ? ClampFloat(remaining / segments[index].length, 0.0f, 1.0f)
                                     : 0.0f;
            *segmentIndex = index;
            return (Vector2){
                segments[index].start.x +
                    (segments[index].end.x - segments[index].start.x) * amount,
                segments[index].start.y +
                    (segments[index].end.y - segments[index].start.y) * amount
            };
        }

        remaining -= segments[index].length;
    }

    *segmentIndex = 0;
    return segmentCount > 0 ? segments[0].start : (Vector2){0.0f, 0.0f};
}

static void DrawPill(Rectangle rectangle, const char *label, const char *value,
                     Color accent)
{
    DrawRectangleRounded(rectangle, 0.45f, 12, (Color){16, 22, 40, 235});
    DrawRectangleRoundedLinesEx(rectangle, 0.45f, 12, 1.0f, WithAlpha(accent, 90));
    DrawText(label, (int)rectangle.x + 12, (int)rectangle.y + 7, 10,
             (Color){130, 146, 177, 255});
    DrawText(value, (int)rectangle.x + 12, (int)rectangle.y + 21, 15,
             (Color){235, 242, 255, 255});
}

static void DrawBackgroundGrid(Rectangle stage, float time, Color accent)
{
    const int spacing = 48;
    const int left = (int)stage.x;
    const int top = (int)stage.y;
    const int right = (int)(stage.x + stage.width);
    const int bottom = (int)(stage.y + stage.height);
    const int offset = (int)fmodf(time * 8.0f, (float)spacing);
    const unsigned char gridAlpha = (unsigned char)(20.0f + 6.0f * sinf(time * 0.8f));

    for (int x = left - spacing + offset; x <= right; x += spacing) {
        DrawLine(x, top, x, bottom, WithAlpha(accent, gridAlpha));
    }

    for (int y = top - spacing + offset; y <= bottom; y += spacing) {
        DrawLine(left, y, right, y, WithAlpha(accent, gridAlpha));
    }

    for (int index = 0; index < 72; ++index) {
        const float starX = stage.x + fmodf((float)(index * 137 + 29), stage.width);
        const float starY = stage.y + fmodf((float)(index * 71 + 47), stage.height);
        const float twinkle = 0.5f + 0.5f * sinf(time * 1.4f + (float)index * 1.17f);
        const float radius = 0.8f + twinkle * 1.2f;
        const unsigned char alpha = (unsigned char)(25.0f + twinkle * 65.0f);
        DrawCircleV((Vector2){starX, starY}, radius, WithAlpha(accent, alpha));
    }
}

static void DrawBeam(const RaySegment segments[MAX_SEGMENTS], int segmentCount,
                     float baseHue, float time, bool paused)
{
    const float pulse = 0.5f + 0.5f * sinf(time * 3.2f);

    for (int index = 0; index < segmentCount; ++index) {
        const Color color = PaletteColor(baseHue, index);
        DrawLineEx(segments[index].start, segments[index].end, 22.0f + pulse * 3.0f,
                   WithAlpha(color, 16));
        DrawLineEx(segments[index].start, segments[index].end, 12.0f + pulse * 2.0f,
                   WithAlpha(color, 35));
        DrawLineEx(segments[index].start, segments[index].end, 6.0f,
                   WithAlpha(color, 120));
        DrawLineEx(segments[index].start, segments[index].end, 2.2f,
                   (Color){245, 252, 255, 245});
    }

    for (int index = 0; index < segmentCount; ++index) {
        const float phase = paused ? 0.32f : fmodf(time * 1.65f + (float)index * 0.19f, 1.0f);
        const Color impactColor = PaletteColor(baseHue, index);
        const float radius = 7.0f + phase * 20.0f;
        const unsigned char alpha = (unsigned char)(110.0f * (1.0f - phase));

        DrawCircleV(segments[index].end, 9.0f + pulse * 2.0f,
                    WithAlpha(impactColor, 28));
        DrawRing(segments[index].end, radius - 1.2f, radius, 0.0f, 360.0f, 40,
                 WithAlpha(impactColor, alpha));
    }

    const float pathLength = TotalPathLength(segments, segmentCount);

    if (pathLength <= 0.0f) {
        return;
    }

    for (int particle = 0; particle < 28; ++particle) {
        float distance = (float)particle / 28.0f * pathLength;

        if (!paused) {
            distance = fmodf(distance + time * 250.0f, pathLength);
        }

        int segmentIndex = 0;
        const Vector2 position = PointAlongPath(segments, segmentCount, distance,
                                                &segmentIndex);
        const Color color = PaletteColor(baseHue, segmentIndex);
        const float particlePulse = 0.5f +
                                    0.5f * sinf(time * 5.0f + (float)particle * 0.9f);

        DrawCircleV(position, 7.0f + particlePulse * 2.0f, WithAlpha(color, 22));
        DrawCircleV(position, 2.0f + particlePulse * 1.4f,
                    WithAlpha((Color){255, 255, 255, 255}, 220));
    }
}

static void DrawAnchor(Vector2 position, Color color, const char *letter,
                       const char *caption, bool selected, float time)
{
    const float pulse = 0.5f + 0.5f * sinf(time * 3.0f);
    const float outerRadius = selected ? 28.0f + pulse * 3.0f : 23.0f + pulse * 2.0f;

    DrawCircleV(position, outerRadius, WithAlpha(color, selected ? 38 : 24));
    DrawRing(position, outerRadius - 1.5f, outerRadius, 0.0f, 360.0f, 48,
             WithAlpha(color, selected ? 180 : 95));
    DrawCircleV((Vector2){position.x + 2.0f, position.y + 3.0f}, POINT_RADIUS + 2.0f,
                (Color){2, 6, 16, 190});
    DrawCircleV(position, POINT_RADIUS, color);
    DrawCircleV((Vector2){position.x - 3.5f, position.y - 4.0f}, 3.2f,
                (Color){255, 255, 255, 220});

    const int letterWidth = MeasureText(letter, 14);
    DrawText(letter, (int)(position.x - (float)letterWidth / 2.0f),
             (int)(position.y - 7.0f), 14, (Color){5, 9, 19, 245});

    const int captionWidth = MeasureText(caption, 11);
    DrawText(caption, (int)(position.x - (float)captionWidth / 2.0f),
             (int)(position.y + 30.0f), 11, WithAlpha(color, 220));
}

static void DrawDashedGuide(Vector2 start, Vector2 end, Color color)
{
    const Vector2 difference = SubtractVectors(end, start);
    const float length = VectorLength(difference);
    const Vector2 direction = NormalizeVector(difference);
    const float dashLength = 8.0f;
    const float gapLength = 7.0f;

    for (float distance = 0.0f; distance < length; distance += dashLength + gapLength) {
        const float endDistance = fminf(distance + dashLength, length);
        DrawLineEx(AddVectors(start, ScaleVector(direction, distance)),
                   AddVectors(start, ScaleVector(direction, endDistance)),
                   1.0f, WithAlpha(color, 90));
    }
}

static void ResetAnchors(Vector2 *emitter, Vector2 *aim, Rectangle stage)
{
    *emitter = (Vector2){stage.x + stage.width * 0.23f,
                         stage.y + stage.height * 0.58f};
    *aim = (Vector2){stage.x + stage.width * 0.53f,
                     stage.y + stage.height * 0.31f};
}

static void RandomizeAnchors(Vector2 *emitter, Vector2 *aim, Rectangle stage)
{
    const float emitterX = (float)GetRandomValue(14, 43) / 100.0f;
    const float emitterY = (float)GetRandomValue(24, 78) / 100.0f;
    const float aimX = (float)GetRandomValue(55, 88) / 100.0f;
    const float aimY = (float)GetRandomValue(18, 82) / 100.0f;

    *emitter = (Vector2){stage.x + stage.width * emitterX,
                         stage.y + stage.height * emitterY};
    *aim = (Vector2){stage.x + stage.width * aimX,
                     stage.y + stage.height * aimY};
}

int main(void)
{
    const BeamPalette palettes[] = {
        {190.0f, "CYBER ICE"},
        {286.0f, "ULTRAVIOLET"},
        {22.0f, "SOLAR FLARE"},
        {128.0f, "TOXIC LIME"}
    };
    const int paletteCount = (int)(sizeof palettes / sizeof palettes[0]);

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Neon Ricochet Lab");
    SetWindowMinSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    SetTargetFPS(120);

    int bounceCount = DEFAULT_BOUNCES;
    int paletteIndex = 0;
    int draggedAnchor = 0;
    bool paused = false;
    bool showHelp = true;
    bool anchorsInitialized = false;
    float animationTime = 0.0f;
    Vector2 emitter = {0.0f, 0.0f};
    Vector2 aim = {0.0f, 0.0f};

    while (!WindowShouldClose()) {
        const int screenWidth = GetScreenWidth();
        const int screenHeight = GetScreenHeight();
        const Rectangle stage = {
            24.0f,
            105.0f,
            (float)screenWidth - 48.0f,
            (float)screenHeight - 153.0f
        };
        const Vector2 mouse = GetMousePosition();

        if (!anchorsInitialized) {
            ResetAnchors(&emitter, &aim, stage);
            anchorsInitialized = true;
        }

        emitter = ClampPointToRectangle(emitter, stage, POINT_RADIUS + 4.0f);
        aim = ClampPointToRectangle(aim, stage, POINT_RADIUS + 4.0f);

        if (!paused) {
            animationTime += GetFrameTime();
        }

        const float wheel = GetMouseWheelMove();
        if (wheel > 0.0f || IsKeyPressed(KEY_UP)) {
            ++bounceCount;
        }
        if (wheel < 0.0f || IsKeyPressed(KEY_DOWN)) {
            --bounceCount;
        }
        bounceCount = ClampInt(bounceCount, MIN_BOUNCES, MAX_BOUNCES);

        if (IsKeyPressed(KEY_SPACE)) {
            paused = !paused;
        }
        if (IsKeyPressed(KEY_H)) {
            showHelp = !showHelp;
        }
        if (IsKeyPressed(KEY_P)) {
            paletteIndex = (paletteIndex + 1) % paletteCount;
        }
        if (IsKeyPressed(KEY_R)) {
            RandomizeAnchors(&emitter, &aim, stage);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            ResetAnchors(&emitter, &aim, stage);
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (DistanceSquared(mouse, emitter) <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
                draggedAnchor = 1;
            } else if (DistanceSquared(mouse, aim) <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
                draggedAnchor = 2;
            } else if (CheckCollisionPointRec(mouse, stage)) {
                aim = ClampPointToRectangle(mouse, stage, POINT_RADIUS + 4.0f);
                draggedAnchor = 2;
            }
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (draggedAnchor == 1) {
                emitter = ClampPointToRectangle(mouse, stage, POINT_RADIUS + 4.0f);
            } else if (draggedAnchor == 2) {
                aim = ClampPointToRectangle(mouse, stage, POINT_RADIUS + 4.0f);
            }
        } else {
            draggedAnchor = 0;
        }

        RaySegment segments[MAX_SEGMENTS] = {0};
        const int segmentCount = TraceRicochets(emitter, aim, stage, bounceCount,
                                                segments);
        const float pathLength = TotalPathLength(segments, segmentCount);
        const Color accent = PaletteColor(palettes[paletteIndex].hue, 0);
        const bool emitterHovered = DistanceSquared(mouse, emitter) <=
                                    POINT_HIT_RADIUS * POINT_HIT_RADIUS;
        const bool aimHovered = DistanceSquared(mouse, aim) <=
                                POINT_HIT_RADIUS * POINT_HIT_RADIUS;

        if (emitterHovered || aimHovered || draggedAnchor != 0) {
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        } else {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }

        BeginDrawing();
        ClearBackground((Color){4, 6, 14, 255});
        DrawRectangleGradientV(0, 0, screenWidth, screenHeight,
                               (Color){11, 16, 32, 255},
                               (Color){3, 5, 12, 255});

        const bool compactHeader = screenWidth < 1050;
        DrawText("NEON RICOCHET LAB", 25, compactHeader ? 24 : 19,
                 compactHeader ? 24 : 30, (Color){239, 246, 255, 255});
        DrawText(compactHeader ? "Drag points. Bend light."
                               : "Two points. One beam. Infinite-looking possibilities.",
                 27, 57, 15, (Color){121, 139, 172, 255});

        char bounceText[16];
        char lengthText[24];
        (void)snprintf(bounceText, sizeof bounceText, "%d", bounceCount);
        (void)snprintf(lengthText, sizeof lengthText, "%.0f px", pathLength);

        const float metricsX = (float)screenWidth - 548.0f;
        DrawPill((Rectangle){metricsX, 18.0f, 106.0f, 51.0f},
                 "BOUNCES", bounceText, accent);
        DrawPill((Rectangle){metricsX + 116.0f, 18.0f, 126.0f, 51.0f},
                 "BEAM LENGTH", lengthText, accent);
        DrawPill((Rectangle){metricsX + 252.0f, 18.0f, 145.0f, 51.0f},
                 "PALETTE", palettes[paletteIndex].name, accent);
        DrawPill((Rectangle){metricsX + 407.0f, 18.0f, 116.0f, 51.0f},
                 "STATUS", paused ? "PAUSED" : "LIVE", accent);

        DrawRectangleRounded(stage, 0.025f, 12, (Color){5, 9, 22, 235});
        DrawRectangleRoundedLinesEx(stage, 0.025f, 12, 1.0f, WithAlpha(accent, 75));

        BeginScissorMode((int)stage.x, (int)stage.y,
                         (int)stage.width, (int)stage.height);
        DrawBackgroundGrid(stage, animationTime, accent);
        DrawDashedGuide(emitter, aim, accent);
        DrawBeam(segments, segmentCount, palettes[paletteIndex].hue,
                 animationTime, paused);
        DrawAnchor(emitter, (Color){61, 225, 255, 255}, "E", "EMITTER",
                   draggedAnchor == 1 || emitterHovered, animationTime);
        DrawAnchor(aim, (Color){255, 75, 150, 255}, "A", "AIM",
                   draggedAnchor == 2 || aimHovered, animationTime);
        EndScissorMode();

        if (showHelp) {
            const Rectangle helpPanel = {
                stage.x + stage.width - 260.0f,
                stage.y + 18.0f,
                240.0f,
                136.0f
            };
            DrawRectangleRounded(helpPanel, 0.12f, 10, (Color){8, 13, 29, 225});
            DrawRectangleRoundedLinesEx(helpPanel, 0.12f, 10, 1.0f,
                                        WithAlpha(accent, 70));
            DrawText("CONTROLS", (int)helpPanel.x + 16, (int)helpPanel.y + 14,
                     13, accent);
            DrawText("Drag nodes / click to aim", (int)helpPanel.x + 16,
                     (int)helpPanel.y + 38, 12, (Color){211, 220, 238, 255});
            DrawText("Wheel / arrows   bounces", (int)helpPanel.x + 16,
                     (int)helpPanel.y + 57, 12, (Color){157, 172, 202, 255});
            DrawText("P   palette     R   randomize", (int)helpPanel.x + 16,
                     (int)helpPanel.y + 76, 12, (Color){157, 172, 202, 255});
            DrawText("Space   pause   Right click   reset", (int)helpPanel.x + 16,
                     (int)helpPanel.y + 95, 12, (Color){157, 172, 202, 255});
            DrawText("H   hide this panel", (int)helpPanel.x + 16,
                     (int)helpPanel.y + 114, 12, (Color){157, 172, 202, 255});
        }

        const char *footer = "Drag either point to bend the entire ricochet path in real time";
        const int footerWidth = MeasureText(footer, 13);
        DrawText(footer, (screenWidth - footerWidth) / 2, screenHeight - 31,
                 13, (Color){105, 122, 153, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
