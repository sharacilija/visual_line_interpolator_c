#include <stdio.h>
#include <raylib.h>

#define WIDTH 900
#define HEIGHT 600
#define RADIUS 20
#define THICKNESS 10

struct Point {
    float x, y;
    int is_set; // non-zero means true
};

struct Point first_point = (struct Point) {0, 0, 0};
struct Point second_point = (struct Point) {0, 0, 0};

int main()
{
    puts("Place two points");

    InitWindow(WIDTH, HEIGHT, "Lines UI");

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (first_point.is_set != 0 && second_point.is_set != 0)
            {
                first_point.is_set = 0;
                second_point.is_set = 0;
            }

            int x = GetMouseX();
            int y = GetMouseY();

            if (first_point.is_set == 0)
            {
                first_point = (struct Point) {x, y, 1};
            }

            else if (second_point.is_set == 0)
            {
                second_point = (struct Point) {x, y, 1};
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawFPS(50, 50);

        if (first_point.is_set != 0 && second_point.is_set != 0)
        {
            float slope = (second_point.y - first_point.y) / (second_point.x - first_point.x);
            printf("slope = %f\n", slope);
            float offset = first_point.y - slope * first_point.x;
            printf("offset = %f\n", offset);

            // Intersection with left window boundary
            float x1 = 0;
            float y1 = offset;

            // Intersection with right window boundary
            float x2 = WIDTH;
            float y2 = slope * x2 + offset;

            DrawLineEx((Vector2) {x1, y1}, (Vector2) {x2, y2}, THICKNESS, (Color) {255, 0, 68, 255});
        }

        if (first_point.is_set != 0)
        {
            DrawCircle(first_point.x, first_point.y, RADIUS, WHITE);
        }

        if (second_point.is_set != 0)
        {
            DrawCircle(second_point.x, second_point.y, RADIUS, WHITE);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
