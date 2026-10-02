#include "MiniFB.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

extern "C" {
#include "microui.h"
}

#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200

static uint32_t g_buffer[WIDTH * HEIGHT];

static bool g_invert_effect = false;

static float pattern_size = 70.0f;
static float red_shift = 0.0f;
static float green_shift = 0.0f;
static float blue_level = 180.0f;

enum class DrawMode {
  Line,
  Circle
};

struct Line {
  int x0;
  int y0;
  int x1;
  int y1;
  uint32_t color;
};

struct Circle {
  int center_x;
  int center_y;
  int radius;
  uint32_t color;
};

static std::vector<Line> saved_lines;
static std::vector<Circle> saved_circles;

void put_pixel(int x, int y, uint32_t color) {
  if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
    g_buffer[y * WIDTH + x] = color;
  }
}

void blend_pixel(
    int x,
    int y,
    uint32_t source_color,
    float intensity) {

  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
    return;
  }

  intensity = std::clamp(intensity, 0.0f, 1.0f);

  uint32_t destination_color =
      g_buffer[y * WIDTH + x];

  uint8_t source_r =
      static_cast<uint8_t>((source_color >> 16) & 0xFF);

  uint8_t source_g =
      static_cast<uint8_t>((source_color >> 8) & 0xFF);

  uint8_t source_b =
      static_cast<uint8_t>(source_color & 0xFF);

  uint8_t destination_r =
      static_cast<uint8_t>((destination_color >> 16) & 0xFF);

  uint8_t destination_g =
      static_cast<uint8_t>((destination_color >> 8) & 0xFF);

  uint8_t destination_b =
      static_cast<uint8_t>(destination_color & 0xFF);

  uint8_t result_r =
      static_cast<uint8_t>(
          source_r * intensity +
          destination_r * (1.0f - intensity));

  uint8_t result_g =
      static_cast<uint8_t>(
          source_g * intensity +
          destination_g * (1.0f - intensity));

  uint8_t result_b =
      static_cast<uint8_t>(
          source_b * intensity +
          destination_b * (1.0f - intensity));

  g_buffer[y * WIDTH + x] =
      MFB_RGB(result_r, result_g, result_b);
}

void draw_line_bresenham(
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color) {

  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;

  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;

  int error = dx + dy;

  while (true) {
    put_pixel(x0, y0, color);

    if (x0 == x1 && y0 == y1) {
      break;
    }

    int error2 = 2 * error;

    if (error2 >= dy) {
      error += dy;
      x0 += sx;
    }

    if (error2 <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void draw_line_naive(
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color) {

  int dx = x1 - x0;
  int dy = y1 - y0;

  int steps =
      std::max(std::abs(dx), std::abs(dy));

  if (steps == 0) {
    put_pixel(x0, y0, color);
    return;
  }

  float x_increment =
      static_cast<float>(dx) /
      static_cast<float>(steps);

  float y_increment =
      static_cast<float>(dy) /
      static_cast<float>(steps);

  float x = static_cast<float>(x0);
  float y = static_cast<float>(y0);

  for (int i = 0; i <= steps; i++) {
    put_pixel(
        static_cast<int>(std::round(x)),
        static_cast<int>(std::round(y)),
        color);

    x += x_increment;
    y += y_increment;
  }
}

float fractional_part(float value) {
  return value - std::floor(value);
}

float reverse_fractional_part(float value) {
  return 1.0f - fractional_part(value);
}

void plot_wu_pixel(
    bool steep,
    int x,
    int y,
    uint32_t color,
    float intensity) {

  if (steep) {
    blend_pixel(y, x, color, intensity);
  } else {
    blend_pixel(x, y, color, intensity);
  }
}

void draw_line_wu(
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color) {

  bool steep =
      std::abs(y1 - y0) >
      std::abs(x1 - x0);

  if (steep) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }

  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }

  float dx =
      static_cast<float>(x1 - x0);

  float dy =
      static_cast<float>(y1 - y0);

  if (dx == 0.0f) {
    draw_line_bresenham(
        steep ? y0 : x0,
        steep ? x0 : y0,
        steep ? y1 : x1,
        steep ? x1 : y1,
        color);

    return;
  }

  float gradient = dy / dx;

  float first_x =
      std::round(static_cast<float>(x0));

  float first_y =
      static_cast<float>(y0) +
      gradient *
          (first_x - static_cast<float>(x0));

  float first_gap =
      reverse_fractional_part(
          static_cast<float>(x0) + 0.5f);

  int first_pixel_x =
      static_cast<int>(first_x);

  int first_pixel_y =
      static_cast<int>(std::floor(first_y));

  plot_wu_pixel(
      steep,
      first_pixel_x,
      first_pixel_y,
      color,
      reverse_fractional_part(first_y) *
          first_gap);

  plot_wu_pixel(
      steep,
      first_pixel_x,
      first_pixel_y + 1,
      color,
      fractional_part(first_y) *
          first_gap);

  float intersection_y =
      first_y + gradient;

  float second_x =
      std::round(static_cast<float>(x1));

  float second_y =
      static_cast<float>(y1) +
      gradient *
          (second_x - static_cast<float>(x1));

  float second_gap =
      fractional_part(
          static_cast<float>(x1) + 0.5f);

  int second_pixel_x =
      static_cast<int>(second_x);

  int second_pixel_y =
      static_cast<int>(std::floor(second_y));

  plot_wu_pixel(
      steep,
      second_pixel_x,
      second_pixel_y,
      color,
      reverse_fractional_part(second_y) *
          second_gap);

  plot_wu_pixel(
      steep,
      second_pixel_x,
      second_pixel_y + 1,
      color,
      fractional_part(second_y) *
          second_gap);

  for (int x = first_pixel_x + 1;
       x < second_pixel_x;
       x++) {

    int integer_y =
        static_cast<int>(
            std::floor(intersection_y));

    plot_wu_pixel(
        steep,
        x,
        integer_y,
        color,
        reverse_fractional_part(
            intersection_y));

    plot_wu_pixel(
        steep,
        x,
        integer_y + 1,
        color,
        fractional_part(
            intersection_y));

    intersection_y += gradient;
  }
}

void draw_circle(
    int center_x,
    int center_y,
    int radius,
    uint32_t color) {

  if (radius < 0) {
    return;
  }

  int x = 0;
  int y = radius;
  int decision = 3 - 2 * radius;

  while (y >= x) {
    put_pixel(center_x + x, center_y + y, color);
    put_pixel(center_x - x, center_y + y, color);
    put_pixel(center_x + x, center_y - y, color);
    put_pixel(center_x - x, center_y - y, color);

    put_pixel(center_x + y, center_y + x, color);
    put_pixel(center_x - y, center_y + x, color);
    put_pixel(center_x + y, center_y - x, color);
    put_pixel(center_x - y, center_y - x, color);

    x++;

    if (decision > 0) {
      y--;
      decision += 4 * (x - y) + 10;
    } else {
      decision += 4 * x + 6;
    }
  }
}

void draw_selected_line(
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color,
    bool use_wu) {

  if (use_wu) {
    draw_line_wu(
        x0,
        y0,
        x1,
        y1,
        color);
  } else {
    draw_line_bresenham(
        x0,
        y0,
        x1,
        y1,
        color);
  }
}

void run_line_benchmark(
    double &bresenham_time_ms,
    double &naive_time_ms) {

  constexpr int line_count = 100000;

  std::mt19937 random_generator(123456);

  std::uniform_int_distribution<int> x_distribution(
      820,
      WIDTH - 1);

  std::uniform_int_distribution<int> y_distribution(
      0,
      HEIGHT - 1);

  std::vector<Line> random_lines;
  random_lines.reserve(line_count);

  uint32_t benchmark_color =
      MFB_RGB(255, 255, 255);

  for (int i = 0; i < line_count; i++) {
    random_lines.push_back({
        x_distribution(random_generator),
        y_distribution(random_generator),
        x_distribution(random_generator),
        y_distribution(random_generator),
        benchmark_color
    });
  }

  auto bresenham_start =
      std::chrono::high_resolution_clock::now();

  for (const Line &line : random_lines) {
    draw_line_bresenham(
        line.x0,
        line.y0,
        line.x1,
        line.y1,
        line.color);
  }

  auto bresenham_end =
      std::chrono::high_resolution_clock::now();

  auto naive_start =
      std::chrono::high_resolution_clock::now();

  for (const Line &line : random_lines) {
    draw_line_naive(
        line.x0,
        line.y0,
        line.x1,
        line.y1,
        line.color);
  }

  auto naive_end =
      std::chrono::high_resolution_clock::now();

  bresenham_time_ms =
      std::chrono::duration<double, std::milli>(
          bresenham_end - bresenham_start)
          .count();

  naive_time_ms =
      std::chrono::duration<double, std::milli>(
          naive_end - naive_start)
          .count();

  printf("\n========== LINE BENCHMARK ==========\n");
  printf("Lines tested: %d\n", line_count);
  printf("Bresenham time: %.3f ms\n", bresenham_time_ms);
  printf("Naive float time: %.3f ms\n", naive_time_ms);

  if (bresenham_time_ms < naive_time_ms) {
    printf("Result: Bresenham was faster.\n");
  } else if (naive_time_ms < bresenham_time_ms) {
    printf("Result: Naive was faster.\n");
  } else {
    printf("Result: Equal times.\n");
  }

  printf("====================================\n\n");
}

int main() {
  struct mfb_window *window =
      mfb_open_ex(
          "MiniGUI Platform",
          WIDTH,
          HEIGHT,
          MFB_WF_RESIZABLE);

  if (!window) {
    return 1;
  }

  mu_Context *ctx =
      static_cast<mu_Context *>(
          malloc(sizeof(mu_Context)));

  if (!ctx) {
    mfb_close(window);
    return 1;
  }

  mu_init(ctx);

  ctx->text_width =
      [](mu_Font font,
         const char *text,
         int length) {

        (void)font;

        return (
                   length < 0
                       ? static_cast<int>(
                             strlen(text))
                       : length) *
               8;
      };

  ctx->text_height =
      [](mu_Font font) {
        (void)font;
        return 8;
      };

  UIRenderer renderer(WIDTH, HEIGHT);

  mfb_set_char_input_callback(
      [](struct mfb_window *window_pointer,
         unsigned int character) {

        if (character == 'e' ||
            character == 'E') {

          g_invert_effect =
              !g_invert_effect;

          printf("Background effect toggled\n");
          return;
        }

        extern void ui_bridge_char_input(
            struct mfb_window *,
            unsigned int);

        ui_bridge_char_input(
            window_pointer,
            character);
      },
      window);

  bool is_drawing = false;
  bool previous_left_down = false;

  DrawMode draw_mode =
      DrawMode::Line;

  int shape_start_x = 0;
  int shape_start_y = 0;

  int preview_x = 0;
  int preview_y = 0;

  float shape_red = 255.0f;
  float shape_green = 255.0f;
  float shape_blue = 255.0f;

  int use_wu_algorithm = 0;

  double bresenham_time_ms = 0.0;
  double naive_time_ms = 0.0;

  bool benchmark_has_run = false;

  float slider_value = 50.0f;
  float number_value = 3.14f;

  int checkbox_a = 0;
  int checkbox_b = 1;

  char textbox_buffer[128] =
      "edit me";

  bool quit_requested = false;
  bool show_message = false;

  while (
      mfb_update_events(window) !=
      MFB_STATE_EXIT) {

    ui_bridge_input(ctx, window);

    bool left_down =
        (ctx->mouse_down &
         MU_MOUSE_LEFT) != 0;

    bool mouse_inside_canvas =
        ctx->mouse_pos.x >= 820 &&
        ctx->mouse_pos.x < WIDTH &&
        ctx->mouse_pos.y >= 20 &&
        ctx->mouse_pos.y < HEIGHT;

    if (left_down &&
        !previous_left_down &&
        mouse_inside_canvas) {

      is_drawing = true;

      shape_start_x =
          ctx->mouse_pos.x;

      shape_start_y =
          ctx->mouse_pos.y;

      preview_x =
          shape_start_x;

      preview_y =
          shape_start_y;
    }

    if (is_drawing && left_down) {
      preview_x =
          ctx->mouse_pos.x;

      preview_y =
          ctx->mouse_pos.y;
    }

    if (!left_down &&
        previous_left_down &&
        is_drawing) {

      uint32_t selected_color =
          MFB_RGB(
              static_cast<uint8_t>(
                  shape_red),
              static_cast<uint8_t>(
                  shape_green),
              static_cast<uint8_t>(
                  shape_blue));

      if (draw_mode ==
          DrawMode::Line) {

        saved_lines.push_back({
            shape_start_x,
            shape_start_y,
            preview_x,
            preview_y,
            selected_color
        });

      } else {
        int dx =
            preview_x - shape_start_x;

        int dy =
            preview_y - shape_start_y;

        int radius =
            static_cast<int>(
                std::sqrt(
                    static_cast<double>(
                        dx * dx +
                        dy * dy)));

        saved_circles.push_back({
            shape_start_x,
            shape_start_y,
            radius,
            selected_color
        });
      }

      is_drawing = false;
    }

    previous_left_down =
        left_down;

    for (int i = 0;
         i < WIDTH * HEIGHT;
         i++) {

      int x = i % WIDTH;
      int y = i / WIDTH;

      int center_x = WIDTH / 2;
      int center_y = HEIGHT / 2;

      int dx = x - center_x;
      int dy = y - center_y;

      int distance =
          static_cast<int>(
              std::sqrt(
                  static_cast<double>(
                      dx * dx +
                      dy * dy)));

      int cell_size =
          static_cast<int>(
              pattern_size);

      if (cell_size < 1) {
        cell_size = 1;
      }

      int checker =
          ((x / cell_size) +
           (y / cell_size)) %
          2;

      uint8_t red;
      uint8_t green;
      uint8_t blue;

      if (!g_invert_effect) {
        red =
            static_cast<uint8_t>(
                (distance +
                 x +
                 static_cast<int>(
                     red_shift)) %
                256);

        green =
            static_cast<uint8_t>(
                (distance +
                 y +
                 static_cast<int>(
                     green_shift)) %
                256);

        blue =
            checker
                ? static_cast<uint8_t>(
                      blue_level)
                : static_cast<uint8_t>(
                      blue_level /
                      3.0f);

      } else {
        red =
            checker ? 30 : 220;

        green =
            static_cast<uint8_t>(
                255 -
                ((distance +
                  y +
                  static_cast<int>(
                      green_shift)) %
                 256));

        blue =
            static_cast<uint8_t>(
                255 -
                ((distance +
                  x +
                  static_cast<int>(
                      red_shift)) %
                 256));
      }

      g_buffer[i] =
          MFB_RGB(
              red,
              green,
              blue);
    }

    for (const Line &line :
         saved_lines) {

      draw_selected_line(
          line.x0,
          line.y0,
          line.x1,
          line.y1,
          line.color,
          use_wu_algorithm != 0);
    }

    for (const Circle &circle :
         saved_circles) {

      draw_circle(
          circle.center_x,
          circle.center_y,
          circle.radius,
          circle.color);
    }

    if (is_drawing) {
      uint32_t preview_color =
          MFB_RGB(
              static_cast<uint8_t>(
                  shape_red),
              static_cast<uint8_t>(
                  shape_green),
              static_cast<uint8_t>(
                  shape_blue));

      if (draw_mode ==
          DrawMode::Line) {

        draw_selected_line(
            shape_start_x,
            shape_start_y,
            preview_x,
            preview_y,
            preview_color,
            use_wu_algorithm != 0);

      } else {
        int dx =
            preview_x -
            shape_start_x;

        int dy =
            preview_y -
            shape_start_y;

        int radius =
            static_cast<int>(
                std::sqrt(
                    static_cast<double>(
                        dx * dx +
                        dy * dy)));

        draw_circle(
            shape_start_x,
            shape_start_y,
            radius,
            preview_color);
      }
    }

    mu_begin(ctx);

    if (mu_begin_window(
            ctx,
            "Widgets",
            mu_rect(
                20,
                20,
                390,
                1140))) {

      int full_width[] = {-1};

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Immediate Mode GUI");

      mu_text(
          ctx,
          "Interactive graphics application.");

      mu_layout_row(ctx, 1, full_width, 0);

      if (mu_button(
              ctx,
              "Show / Hide Message")) {

        show_message =
            !show_message;

        printf("Custom button clicked\n");
      }

      if (show_message) {
        mu_layout_row(ctx, 1, full_width, 0);

        mu_label(
            ctx,
            "The custom message is visible.");
      }

      mu_layout_row(ctx, 1, full_width, 0);

      mu_checkbox(
          ctx,
          "Checkbox A",
          &checkbox_a);

      mu_checkbox(
          ctx,
          "Checkbox B",
          &checkbox_b);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Textbox:");

      mu_textbox(
          ctx,
          textbox_buffer,
          sizeof(textbox_buffer));

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Original Slider:");

      mu_slider(
          ctx,
          &slider_value,
          0,
          100);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "--- Background Controls ---");

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Pattern Size:");

      mu_slider(
          ctx,
          &pattern_size,
          20,
          150);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Red Shift:");

      mu_slider(
          ctx,
          &red_shift,
          0,
          255);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Green Shift:");

      mu_slider(
          ctx,
          &green_shift,
          0,
          255);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Blue Level:");

      mu_slider(
          ctx,
          &blue_level,
          0,
          255);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "--- Drawing Tool ---");

      int mode_widths[] =
          {170, 170};

      mu_layout_row(
          ctx,
          2,
          mode_widths,
          0);

      if (mu_button(
              ctx,
              "Line Mode")) {

        draw_mode =
            DrawMode::Line;
      }

      if (mu_button(
              ctx,
              "Circle Mode")) {

        draw_mode =
            DrawMode::Circle;
      }

      mu_layout_row(ctx, 1, full_width, 0);

      if (draw_mode ==
          DrawMode::Line) {

        mu_label(
            ctx,
            "Current mode: Line");

      } else {
        mu_label(
            ctx,
            "Current mode: Circle");
      }

      mu_layout_row(ctx, 1, full_width, 0);

      if (use_wu_algorithm == 0) {
        if (mu_button(
                ctx,
                "Switch to Xiaolin Wu")) {

          use_wu_algorithm = 1;
        }

      } else {
        if (mu_button(
                ctx,
                "Switch to Bresenham")) {

          use_wu_algorithm = 0;
        }
      }

      mu_layout_row(ctx, 1, full_width, 0);

      if (use_wu_algorithm == 1) {
        mu_label(
            ctx,
            "Line algorithm: Xiaolin Wu");
      } else {
        mu_label(
            ctx,
            "Line algorithm: Bresenham");
      }

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Shape Red:");

      mu_slider(
          ctx,
          &shape_red,
          0,
          255);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Shape Green:");

      mu_slider(
          ctx,
          &shape_green,
          0,
          255);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Shape Blue:");

      mu_slider(
          ctx,
          &shape_blue,
          0,
          255);

      mu_layout_row(ctx, 1, full_width, 0);

      if (mu_button(
              ctx,
              "Clear Canvas")) {

        saved_lines.clear();
        saved_circles.clear();
        is_drawing = false;
      }

      char line_count_text[64];

      snprintf(
          line_count_text,
          sizeof(line_count_text),
          "Saved lines: %zu",
          saved_lines.size());

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, line_count_text);

      char circle_count_text[64];

      snprintf(
          circle_count_text,
          sizeof(circle_count_text),
          "Saved circles: %zu",
          saved_circles.size());

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, circle_count_text);

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "--- Performance Benchmark ---");

      mu_layout_row(ctx, 1, full_width, 0);

      if (mu_button(
              ctx,
              "Run 100,000 Line Benchmark")) {

        run_line_benchmark(
            bresenham_time_ms,
            naive_time_ms);

        benchmark_has_run = true;
      }

      if (benchmark_has_run) {
        char bresenham_text[80];

        snprintf(
            bresenham_text,
            sizeof(bresenham_text),
            "Bresenham: %.3f ms",
            bresenham_time_ms);

        mu_layout_row(ctx, 1, full_width, 0);
        mu_label(ctx, bresenham_text);

        char naive_text[80];

        snprintf(
            naive_text,
            sizeof(naive_text),
            "Naive float: %.3f ms",
            naive_time_ms);

        mu_layout_row(ctx, 1, full_width, 0);
        mu_label(ctx, naive_text);

        mu_layout_row(ctx, 1, full_width, 0);

        if (bresenham_time_ms <
            naive_time_ms) {

          mu_label(
              ctx,
              "Winner: Bresenham");

        } else {
          mu_label(
              ctx,
              "Winner: Naive float");
        }
      }

      mu_layout_row(ctx, 1, full_width, 0);

      mu_text(
          ctx,
          "Draw on the right side by clicking, "
          "dragging and releasing.");

      mu_layout_row(ctx, 1, full_width, 0);
      mu_label(ctx, "Number widget:");

      mu_number(
          ctx,
          &number_value,
          0.1f);

      mu_layout_row(ctx, 1, full_width, 0);

      if (mu_button(
              ctx,
              "Quit")) {

        quit_requested = true;
      }

      mu_end_window(ctx);
    }

    if (mu_begin_window(
            ctx,
            "Panel Demo",
            mu_rect(
                430,
                20,
                360,
                200))) {

      int panel_width[] = {-1};

      mu_layout_row(
          ctx,
          1,
          panel_width,
          120);

      mu_begin_panel(
          ctx,
          "scrollable panel");

      for (int i = 1;
           i <= 12;
           i++) {

        mu_layout_row(
            ctx,
            1,
            panel_width,
            0);

        char row_text[32];

        snprintf(
            row_text,
            sizeof(row_text),
            "Panel row %d",
            i);

        mu_label(
            ctx,
            row_text);
      }

      mu_end_panel(ctx);
      mu_end_window(ctx);
    }

    if (mu_begin_window(
            ctx,
            "Popup Demo",
            mu_rect(
                430,
                235,
                360,
                80))) {

      int popup_width[] = {-1};

      mu_layout_row(
          ctx,
          1,
          popup_width,
          0);

      if (mu_button(
              ctx,
              "Open popup")) {

        mu_Container *popup =
            mu_get_container(
                ctx,
                "my popup");

        popup->rect =
            mu_rect(
                ctx->mouse_pos.x,
                ctx->mouse_pos.y,
                260,
                84);

        popup->open = 1;

        ctx->hover_root =
            ctx->next_hover_root =
                popup;

        mu_bring_to_front(
            ctx,
            popup);
      }

      int popup_options =
          MU_OPT_POPUP |
          MU_OPT_NORESIZE |
          MU_OPT_NOSCROLL |
          MU_OPT_NOTITLE |
          MU_OPT_CLOSED;

      if (mu_begin_window_ex(
              ctx,
              "my popup",
              mu_rect(
                  0,
                  0,
                  260,
                  84),
              popup_options)) {

        mu_layout_row(
            ctx,
            1,
            popup_width,
            0);

        mu_label(
            ctx,
            "Click outside to close.");

        if (mu_button(
                ctx,
                "Close")) {

          mu_get_current_container(ctx)
              ->open = 0;
        }

        mu_end_window(ctx);
      }

      mu_end_window(ctx);
    }

    mu_end(ctx);

    if (quit_requested) {
      mfb_close(window);
      break;
    }

    renderer.render(
        ctx,
        g_buffer);

    mfb_update_state state =
        mfb_update_ex(
            window,
            g_buffer,
            WIDTH,
            HEIGHT);

    if (state < 0) {
      break;
    }

    mfb_wait_sync(window);
  }

  mfb_close(window);
  free(ctx);

  return 0;
}