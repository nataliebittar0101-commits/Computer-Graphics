#include "MiniFB.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include "microui.h"
}

#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200

constexpr int UI_WIDTH = 445;
constexpr float EPSILON = 0.00001f;

static uint32_t g_buffer[WIDTH * HEIGHT];
static float g_z_buffer[WIDTH * HEIGHT];
// ============================================================
// Data structures
// ============================================================

struct Face {
    int a = 0;
    int b = 0;
    int c = 0;
};

struct TransformState {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

struct Mesh {
    std::string name;

    std::vector<glm::vec3> vertices;
    std::vector<Face> faces;

    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    glm::vec3 center{0.0f};

    float normalization_scale = 1.0f;

    TransformState transform;
};

struct Camera {
    glm::vec3 position{0.0f, 0.0f, 7.0f};

    float field_of_view = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    bool perspective = true;
};

struct RasterVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    glm::vec3 view_position{0.0f};

    bool valid = false;
};

// ============================================================
// Framebuffer
// ============================================================

void put_pixel(
    int x,
    int y,
    uint32_t color) {

    if (x < UI_WIDTH + 2 ||
        x >= WIDTH ||
        y < 0 ||
        y >= HEIGHT) {

        return;
    }

    g_buffer[y * WIDTH + x] = color;
}

void clear_background() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float horizontal =
                static_cast<float>(x) /
                static_cast<float>(WIDTH);

            float vertical =
                static_cast<float>(y) /
                static_cast<float>(HEIGHT);

            uint8_t red =
                static_cast<uint8_t>(
                    12.0f +
                    horizontal * 18.0f);

            uint8_t green =
                static_cast<uint8_t>(
                    18.0f +
                    vertical * 22.0f);

            uint8_t blue =
                static_cast<uint8_t>(
                    38.0f +
                    horizontal * 25.0f +
                    vertical * 18.0f);

            g_buffer[y * WIDTH + x] =
                MFB_RGB(red, green, blue);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        g_buffer[y * WIDTH + UI_WIDTH] =
            MFB_RGB(120, 125, 145);

        g_buffer[y * WIDTH + UI_WIDTH + 1] =
            MFB_RGB(120, 125, 145);
    }
}

void clear_z_buffer() {
    float infinity =
        std::numeric_limits<float>::infinity();

    for (float &value : g_z_buffer) {
        value = infinity;
    }
}

void draw_line(
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

        if (x0 == x1 &&
            y0 == y1) {

            break;
        }

        int doubled_error =
            2 * error;

        if (doubled_error >= dy) {
            error += dy;
            x0 += sx;
        }

        if (doubled_error <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

// ============================================================
// Model colors
// ============================================================

uint32_t face_color(
    int mesh_index,
    int face_index) {

    uint32_t seed =
        static_cast<uint32_t>(
            mesh_index * 92821 +
            face_index * 68917 +
            12345);

    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    uint8_t red =
        static_cast<uint8_t>(
            65 + seed % 180);

    uint8_t green =
        static_cast<uint8_t>(
            65 + (seed >> 8) % 180);

    uint8_t blue =
        static_cast<uint8_t>(
            65 + (seed >> 16) % 180);

    return MFB_RGB(
        red,
        green,
        blue);
}

// ============================================================
// Default OBJ models
// ============================================================

void create_default_models() {
    std::filesystem::create_directories(
        "models");

    if (!std::filesystem::exists(
            "models/cube.obj")) {

        std::ofstream file(
            "models/cube.obj");

        file <<
            "v -1 -1 -1\n"
            "v  1 -1 -1\n"
            "v  1  1 -1\n"
            "v -1  1 -1\n"
            "v -1 -1  1\n"
            "v  1 -1  1\n"
            "v  1  1  1\n"
            "v -1  1  1\n"

            "f 1 2 3\n"
            "f 1 3 4\n"

            "f 5 7 6\n"
            "f 5 8 7\n"

            "f 1 5 6\n"
            "f 1 6 2\n"

            "f 4 3 7\n"
            "f 4 7 8\n"

            "f 1 4 8\n"
            "f 1 8 5\n"

            "f 2 6 7\n"
            "f 2 7 3\n";
    }

    if (!std::filesystem::exists(
            "models/pyramid.obj")) {

        std::ofstream file(
            "models/pyramid.obj");

        file <<
            "v -1 -1 -1\n"
            "v  1 -1 -1\n"
            "v  1 -1  1\n"
            "v -1 -1  1\n"
            "v  0  1  0\n"

            "f 1 2 3\n"
            "f 1 3 4\n"

            "f 1 5 2\n"
            "f 2 5 3\n"
            "f 3 5 4\n"
            "f 4 5 1\n";
    }

    if (!std::filesystem::exists(
            "models/octahedron.obj")) {

        std::ofstream file(
            "models/octahedron.obj");

        file <<
            "v  1  0  0\n"
            "v -1  0  0\n"
            "v  0  1  0\n"
            "v  0 -1  0\n"
            "v  0  0  1\n"
            "v  0  0 -1\n"

            "f 1 3 5\n"
            "f 3 2 5\n"
            "f 2 4 5\n"
            "f 4 1 5\n"

            "f 3 1 6\n"
            "f 2 3 6\n"
            "f 4 2 6\n"
            "f 1 4 6\n";
    }
}
// ============================================================
// OBJ loading
// ============================================================

int parse_obj_index(
    const std::string &token,
    int vertex_count) {

    std::string text = token;

    std::size_t slash =
        text.find('/');

    if (slash != std::string::npos) {
        text =
            text.substr(0, slash);
    }

    if (text.empty()) {
        return -1;
    }

    int index =
        std::stoi(text);

    if (index > 0) {
        return index - 1;
    }

    if (index < 0) {
        return vertex_count + index;
    }

    return -1;
}

void calculate_mesh_bounds(
    Mesh &mesh) {

    if (mesh.vertices.empty()) {
        return;
    }

    mesh.minimum =
        mesh.vertices.front();

    mesh.maximum =
        mesh.vertices.front();

    for (const glm::vec3 &vertex :
         mesh.vertices) {

        mesh.minimum =
            glm::min(
                mesh.minimum,
                vertex);

        mesh.maximum =
            glm::max(
                mesh.maximum,
                vertex);
    }

    mesh.center =
        (mesh.minimum +
         mesh.maximum) *
        0.5f;

    glm::vec3 dimensions =
        mesh.maximum -
        mesh.minimum;

    float largest =
        std::max({
            dimensions.x,
            dimensions.y,
            dimensions.z
        });

    if (largest < EPSILON) {
        largest = 1.0f;
    }

    mesh.normalization_scale =
        2.0f / largest;
}

bool load_obj(
    const std::string &filename,
    Mesh &mesh) {

    std::ifstream file(filename);

    if (!file.is_open()) {
        std::printf(
            "Could not open %s\n",
            filename.c_str());

        return false;
    }

    mesh.vertices.clear();
    mesh.faces.clear();

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() ||
            line[0] == '#') {

            continue;
        }

        std::istringstream stream(line);

        std::string type;
        stream >> type;

        if (type == "v") {
            glm::vec3 vertex;

            stream >>
                vertex.x >>
                vertex.y >>
                vertex.z;

            if (!stream.fail()) {
                mesh.vertices.push_back(
                    vertex);
            }
        }

        if (type == "f") {
            std::vector<int> polygon;
            std::string token;

            while (stream >> token) {
                int index =
                    parse_obj_index(
                        token,
                        static_cast<int>(
                            mesh.vertices.size()));

                if (index >= 0 &&
                    index <
                    static_cast<int>(
                        mesh.vertices.size())) {

                    polygon.push_back(index);
                }
            }

            for (std::size_t i = 1;
                 i + 1 < polygon.size();
                 i++) {

                mesh.faces.push_back({
                    polygon[0],
                    polygon[i],
                    polygon[i + 1]
                });
            }
        }
    }

    calculate_mesh_bounds(mesh);

    std::printf(
        "Loaded %s: %zu vertices, %zu triangles\n",
        filename.c_str(),
        mesh.vertices.size(),
        mesh.faces.size());

    return
        !mesh.vertices.empty() &&
        !mesh.faces.empty();
}

// ============================================================
// Matrices
// ============================================================

glm::mat4 rotation_matrix(
    const glm::vec3 &rotation) {

    glm::mat4 matrix(1.0f);

    matrix =
        glm::rotate(
            matrix,
            glm::radians(rotation.x),
            glm::vec3(
                1.0f,
                0.0f,
                0.0f));

    matrix =
        glm::rotate(
            matrix,
            glm::radians(rotation.y),
            glm::vec3(
                0.0f,
                1.0f,
                0.0f));

    matrix =
        glm::rotate(
            matrix,
            glm::radians(rotation.z),
            glm::vec3(
                0.0f,
                0.0f,
                1.0f));

    return matrix;
}

glm::mat4 model_matrix(
    const Mesh &mesh) {

    glm::mat4 normalization(1.0f);

    normalization =
        glm::scale(
            normalization,
            glm::vec3(
                mesh.normalization_scale));

    normalization =
        glm::translate(
            normalization,
            -mesh.center);

    glm::mat4 translation =
        glm::translate(
            glm::mat4(1.0f),
            mesh.transform.translation);

    glm::mat4 rotation =
        rotation_matrix(
            mesh.transform.rotation);

    glm::mat4 scale =
        glm::scale(
            glm::mat4(1.0f),
            mesh.transform.scale);

    return
        translation *
        rotation *
        scale *
        normalization;
}

glm::mat4 view_matrix(
    const Camera &camera) {

    return glm::lookAt(
        camera.position,
        glm::vec3(0.0f),
        glm::vec3(
            0.0f,
            1.0f,
            0.0f));
}

glm::mat4 projection_matrix(
    const Camera &camera) {

    float viewport_width =
        static_cast<float>(
            WIDTH - UI_WIDTH);

    float aspect =
        viewport_width /
        static_cast<float>(HEIGHT);

    if (camera.perspective) {
        return glm::perspective(
            glm::radians(
                camera.field_of_view),
            aspect,
            camera.near_plane,
            camera.far_plane);
    }

    constexpr float height = 3.5f;

    float width =
        height * aspect;

    return glm::ortho(
        -width,
        width,
        -height,
        height,
        camera.near_plane,
        camera.far_plane);
}

// ============================================================
// Projection
// ============================================================

RasterVertex project_vertex(
    const glm::vec3 &world_vertex,
    const glm::mat4 &view,
    const glm::mat4 &projection) {

    RasterVertex result;

    glm::vec4 view_position =
        view *
        glm::vec4(
            world_vertex,
            1.0f);

    glm::vec4 clip =
        projection *
        view_position;

    if (std::abs(clip.w) <
        EPSILON) {

        return result;
    }

    if (clip.w <= 0.0f) {
        return result;
    }

    glm::vec3 ndc =
        glm::vec3(clip) /
        clip.w;

    if (ndc.z < -1.0f ||
        ndc.z > 1.0f) {

        return result;
    }

    int viewport_width =
        WIDTH - UI_WIDTH;

    result.x =
        static_cast<float>(
            UI_WIDTH) +
        (ndc.x * 0.5f + 0.5f) *
        static_cast<float>(
            viewport_width);

    result.y =
        (1.0f -
         (ndc.y * 0.5f + 0.5f)) *
        static_cast<float>(HEIGHT);

    result.z =
        ndc.z * 0.5f +
        0.5f;

    result.view_position =
        glm::vec3(view_position);

    result.valid = true;

    return result;
}

// ============================================================
// Barycentric rasterization
// ============================================================

float edge_function(
    const RasterVertex &a,
    const RasterVertex &b,
    float x,
    float y) {

    return
        (x - a.x) *
        (b.y - a.y) -
        (y - a.y) *
        (b.x - a.x);
}

bool is_top_left(
    const RasterVertex &a,
    const RasterVertex &b) {

    float dx =
        b.x - a.x;

    float dy =
        b.y - a.y;

    return
        dy < 0.0f ||
        (std::abs(dy) <= EPSILON &&
         dx > 0.0f);
}

bool edge_inside(
    float value,
    bool top_left,
    bool use_top_left_rule) {

    if (!use_top_left_rule) {
        return value >= -EPSILON;
    }

    if (value > EPSILON) {
        return true;
    }

    if (std::abs(value) <= EPSILON) {
        return top_left;
    }

    return false;
}

void triangle_bounds(
    const RasterVertex &v0,
    const RasterVertex &v1,
    const RasterVertex &v2,
    int &minimum_x,
    int &maximum_x,
    int &minimum_y,
    int &maximum_y) {

    minimum_x =
        static_cast<int>(
            std::floor(
                std::min({
                    v0.x,
                    v1.x,
                    v2.x
                })));

    maximum_x =
        static_cast<int>(
            std::ceil(
                std::max({
                    v0.x,
                    v1.x,
                    v2.x
                })));

    minimum_y =
        static_cast<int>(
            std::floor(
                std::min({
                    v0.y,
                    v1.y,
                    v2.y
                })));

    maximum_y =
        static_cast<int>(
            std::ceil(
                std::max({
                    v0.y,
                    v1.y,
                    v2.y
                })));

    minimum_x =
        std::clamp(
            minimum_x,
            UI_WIDTH + 2,
            WIDTH - 1);

    maximum_x =
        std::clamp(
            maximum_x,
            UI_WIDTH + 2,
            WIDTH - 1);

    minimum_y =
        std::clamp(
            minimum_y,
            0,
            HEIGHT - 1);

    maximum_y =
        std::clamp(
            maximum_y,
            0,
            HEIGHT - 1);
}

void draw_triangle_bounding_box(
    const RasterVertex &v0,
    const RasterVertex &v1,
    const RasterVertex &v2,
    uint32_t color) {

    int minimum_x;
    int maximum_x;
    int minimum_y;
    int maximum_y;

    triangle_bounds(
        v0,
        v1,
        v2,
        minimum_x,
        maximum_x,
        minimum_y,
        maximum_y);

    for (int y = minimum_y;
         y <= maximum_y;
         y++) {

        for (int x = minimum_x;
             x <= maximum_x;
             x++) {

            put_pixel(
                x,
                y,
                color);
        }
    }
}

void rasterize_triangle(
    RasterVertex v0,
    RasterVertex v1,
    RasterVertex v2,
    uint32_t color,
    bool use_z_buffer,
    bool use_top_left_rule) {

    float area =
        edge_function(
            v0,
            v1,
            v2.x,
            v2.y);

    if (std::abs(area) <
        EPSILON) {

        return;
    }

    if (area < 0.0f) {
        std::swap(v1, v2);
        area = -area;
    }

    int minimum_x;
    int maximum_x;
    int minimum_y;
    int maximum_y;

    triangle_bounds(
        v0,
        v1,
        v2,
        minimum_x,
        maximum_x,
        minimum_y,
        maximum_y);

    bool edge0_top_left =
        is_top_left(v1, v2);

    bool edge1_top_left =
        is_top_left(v2, v0);

    bool edge2_top_left =
        is_top_left(v0, v1);

    for (int y = minimum_y;
         y <= maximum_y;
         y++) {

        for (int x = minimum_x;
             x <= maximum_x;
             x++) {

            float pixel_x =
                static_cast<float>(x) +
                0.5f;

            float pixel_y =
                static_cast<float>(y) +
                0.5f;

            float weight0 =
                edge_function(
                    v1,
                    v2,
                    pixel_x,
                    pixel_y);

            float weight1 =
                edge_function(
                    v2,
                    v0,
                    pixel_x,
                    pixel_y);

            float weight2 =
                edge_function(
                    v0,
                    v1,
                    pixel_x,
                    pixel_y);

            bool inside0 =
                edge_inside(
                    weight0,
                    edge0_top_left,
                    use_top_left_rule);

            bool inside1 =
                edge_inside(
                    weight1,
                    edge1_top_left,
                    use_top_left_rule);

            bool inside2 =
                edge_inside(
                    weight2,
                    edge2_top_left,
                    use_top_left_rule);

            if (!inside0 ||
                !inside1 ||
                !inside2) {

                continue;
            }

            float alpha =
                weight0 / area;

            float beta =
                weight1 / area;

            float gamma =
                weight2 / area;

            float depth =
                alpha * v0.z +
                beta * v1.z +
                gamma * v2.z;

            int index =
                y * WIDTH + x;

            if (use_z_buffer) {
                if (depth >=
                    g_z_buffer[index]) {

                    continue;
                }

                g_z_buffer[index] =
                    depth;
            }

            g_buffer[index] =
                color;
        }
    }
}

// ============================================================
// Backface culling
// ============================================================

bool is_back_facing(
    const RasterVertex &v0,
    const RasterVertex &v1,
    const RasterVertex &v2) {

    glm::vec3 edge1 =
        v1.view_position -
        v0.view_position;

    glm::vec3 edge2 =
        v2.view_position -
        v0.view_position;

    glm::vec3 normal =
        glm::cross(
            edge1,
            edge2);

    if (glm::length(normal) <
        EPSILON) {

        return true;
    }

    glm::vec3 center =
        (
            v0.view_position +
            v1.view_position +
            v2.view_position
        ) / 3.0f;

    glm::vec3 direction_to_camera =
        -center;

    float result =
        glm::dot(
            normal,
            direction_to_camera);

    return result <= 0.0f;
}

// ============================================================
// Mesh rendering
// ============================================================

void render_mesh(
    const Mesh &mesh,
    int mesh_index,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    bool show_boxes,
    bool use_z_buffer,
    bool use_backface_culling,
    bool use_top_left_rule,
    bool show_wireframe) {

    glm::mat4 model =
        model_matrix(mesh);

    std::vector<glm::vec3>
        world_vertices;

    world_vertices.reserve(
        mesh.vertices.size());

    for (const glm::vec3 &vertex :
         mesh.vertices) {

        world_vertices.push_back(
            glm::vec3(
                model *
                glm::vec4(
                    vertex,
                    1.0f)));
    }

    for (std::size_t i = 0;
         i < mesh.faces.size();
         i++) {

        const Face &face =
            mesh.faces[i];

        RasterVertex v0 =
            project_vertex(
                world_vertices[face.a],
                view,
                projection);

        RasterVertex v1 =
            project_vertex(
                world_vertices[face.b],
                view,
                projection);

        RasterVertex v2 =
            project_vertex(
                world_vertices[face.c],
                view,
                projection);

        if (!v0.valid ||
            !v1.valid ||
            !v2.valid) {

            continue;
        }

        if (use_backface_culling &&
            is_back_facing(
                v0,
                v1,
                v2)) {

            continue;
        }

        uint32_t color =
            face_color(
                mesh_index,
                static_cast<int>(i));

        if (show_boxes) {
            draw_triangle_bounding_box(
                v0,
                v1,
                v2,
                color);
        } else {
            rasterize_triangle(
                v0,
                v1,
                v2,
                color,
                use_z_buffer,
                use_top_left_rule);
        }

        if (show_wireframe) {
            uint32_t white =
                MFB_RGB(
                    255,
                    255,
                    255);

            draw_line(
                static_cast<int>(v0.x),
                static_cast<int>(v0.y),
                static_cast<int>(v1.x),
                static_cast<int>(v1.y),
                white);

            draw_line(
                static_cast<int>(v1.x),
                static_cast<int>(v1.y),
                static_cast<int>(v2.x),
                static_cast<int>(v2.y),
                white);

            draw_line(
                static_cast<int>(v2.x),
                static_cast<int>(v2.y),
                static_cast<int>(v0.x),
                static_cast<int>(v0.y),
                white);
        }
    }
}

// ============================================================
// Depth map
// ============================================================

void draw_depth_map() {
    float minimum_depth =
        std::numeric_limits<float>
            ::infinity();

    float maximum_depth =
        -std::numeric_limits<float>
            ::infinity();

    for (int y = 0;
         y < HEIGHT;
         y++) {

        for (int x = UI_WIDTH + 2;
             x < WIDTH;
             x++) {

            float depth =
                g_z_buffer[
                    y * WIDTH + x];

            if (!std::isfinite(depth)) {
                continue;
            }

            minimum_depth =
                std::min(
                    minimum_depth,
                    depth);

            maximum_depth =
                std::max(
                    maximum_depth,
                    depth);
        }
    }

    if (!std::isfinite(minimum_depth) ||
        !std::isfinite(maximum_depth)) {

        return;
    }

    float range =
        maximum_depth -
        minimum_depth;

    if (range < EPSILON) {
        range = 1.0f;
    }

    for (int y = 0;
         y < HEIGHT;
         y++) {

        for (int x = UI_WIDTH + 2;
             x < WIDTH;
             x++) {

            int index =
                y * WIDTH + x;

            float depth =
                g_z_buffer[index];

            if (!std::isfinite(depth)) {
                g_buffer[index] =
                    MFB_RGB(
                        5,
                        5,
                        5);

                continue;
            }

            float normalized =
                (depth -
                 minimum_depth) /
                range;

            normalized =
                std::clamp(
                    normalized,
                    0.0f,
                    1.0f);

            uint8_t gray =
                static_cast<uint8_t>(
                    (1.0f -
                     normalized) *
                    255.0f);

            g_buffer[index] =
                MFB_RGB(
                    gray,
                    gray,
                    gray);
        }
    }
}

// ============================================================
// GUI helpers
// ============================================================

void gui_label(
    mu_Context *ctx,
    const char *text) {

    int widths[] = {-1};

    mu_layout_row(
        ctx,
        1,
        widths,
        0);

    mu_label(ctx, text);
}

void gui_slider(
    mu_Context *ctx,
    const char *label,
    float &value,
    float minimum,
    float maximum) {

    int widths[] =
        {145, -1};

    mu_layout_row(
        ctx,
        2,
        widths,
        0);

    mu_label(ctx, label);

    mu_slider(
        ctx,
        &value,
        minimum,
        maximum);
}

void reset_transform(
    TransformState &transform) {

    transform.translation =
        glm::vec3(0.0f);

    transform.rotation =
        glm::vec3(0.0f);

    transform.scale =
        glm::vec3(1.0f);
}

void reset_camera(
    Camera &camera) {

    camera.position =
        glm::vec3(
            0.0f,
            0.0f,
            7.0f);

    camera.field_of_view =
        60.0f;

    camera.perspective =
        true;
}

// ============================================================
// Main
// ============================================================

int main() {
    create_default_models();

    std::vector<Mesh> meshes;

    Mesh cube;
    cube.name = "Cube";

    if (load_obj(
            "models/cube.obj",
            cube)) {

        cube.transform.translation =
            glm::vec3(
                -1.35f,
                -0.35f,
                0.0f);

        cube.transform.rotation =
            glm::vec3(
                20.0f,
                30.0f,
                0.0f);

        meshes.push_back(cube);
    }

    Mesh pyramid;
    pyramid.name = "Pyramid";

    if (load_obj(
            "models/pyramid.obj",
            pyramid)) {

        pyramid.transform.translation =
            glm::vec3(
                1.35f,
                -0.35f,
                0.0f);

        pyramid.transform.rotation =
            glm::vec3(
                15.0f,
                -25.0f,
                0.0f);

        meshes.push_back(pyramid);
    }

    Mesh octahedron;
    octahedron.name =
        "Octahedron";

    if (load_obj(
            "models/octahedron.obj",
            octahedron)) {

        octahedron.transform.translation =
            glm::vec3(
                0.0f,
                1.35f,
                0.0f);

        octahedron.transform.scale =
            glm::vec3(0.65f);

        octahedron.transform.rotation =
            glm::vec3(
                20.0f,
                25.0f,
                0.0f);

        meshes.push_back(octahedron);
    }

    if (meshes.empty()) {
        std::printf(
            "No models loaded\n");

        return 1;
    }

    Camera camera;

    int active_model = 0;

    int show_triangle_boxes = 0;
    int use_z_buffer = 1;
    int show_depth_map = 0;
    int use_backface_culling = 0;
    int use_top_left_rule = 1;
    int show_wireframe = 0;

    struct mfb_window *window =
        mfb_open_ex(
            "Assignment 4 - Triangle Rasterization",
            WIDTH,
            HEIGHT,
            MFB_WF_RESIZABLE);

    if (!window) {
        return 1;
    }

    mu_Context *ctx =
        static_cast<mu_Context *>(
            std::malloc(
                sizeof(mu_Context)));

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

            return
                (
                    length < 0
                        ? static_cast<int>(
                              std::strlen(text))
                        : length
                ) * 8;
        };

    ctx->text_height =
        [](mu_Font font) {

            (void)font;
            return 8;
        };

    UIRenderer renderer(
        WIDTH,
        HEIGHT);

    mfb_set_char_input_callback(
        [](struct mfb_window *window_pointer,
           unsigned int character) {

            ui_bridge_char_input(
                window_pointer,
                character);
        },
        window);

    bool quit_requested = false;

    bool rotating_model = false;
    bool previous_left_down = false;

    int previous_mouse_x = 0;
    int previous_mouse_y = 0;

    while (
        mfb_update_events(window) !=
        MFB_STATE_EXIT) {

        ui_bridge_input(
            ctx,
            window);

        Mesh &active_mesh =
            meshes[active_model];

        const uint8_t *keys =
            mfb_get_key_buffer(window);

        constexpr float camera_speed =
            0.035f;

        if (keys[MFB_KB_KEY_LEFT]) {
            camera.position.x -=
                camera_speed;
        }

        if (keys[MFB_KB_KEY_RIGHT]) {
            camera.position.x +=
                camera_speed;
        }

        if (keys[MFB_KB_KEY_UP]) {
            camera.position.y +=
                camera_speed;
        }

        if (keys[MFB_KB_KEY_DOWN]) {
            camera.position.y -=
                camera_speed;
        }

        if (keys[MFB_KB_KEY_W]) {
            camera.position.z -=
                camera_speed;
        }

        if (keys[MFB_KB_KEY_S]) {
            camera.position.z +=
                camera_speed;
        }

        bool mouse_inside_scene =
            ctx->mouse_pos.x >
                UI_WIDTH &&
            ctx->mouse_pos.x <
                WIDTH &&
            ctx->mouse_pos.y >= 0 &&
            ctx->mouse_pos.y <
                HEIGHT;

        bool left_down =
            (ctx->mouse_down &
             MU_MOUSE_LEFT) != 0;

        if (left_down &&
            !previous_left_down &&
            mouse_inside_scene) {

            rotating_model = true;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        if (!left_down) {
            rotating_model = false;
        }

        if (rotating_model &&
            left_down) {

            int delta_x =
                ctx->mouse_pos.x -
                previous_mouse_x;

            int delta_y =
                ctx->mouse_pos.y -
                previous_mouse_y;

            active_mesh.transform
                .rotation.y +=
                static_cast<float>(
                    delta_x) *
                0.55f;

            active_mesh.transform
                .rotation.x +=
                static_cast<float>(
                    delta_y) *
                0.55f;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        previous_left_down =
            left_down;

        glm::mat4 view =
            view_matrix(camera);

        glm::mat4 projection =
            projection_matrix(camera);

        clear_background();
        clear_z_buffer();

        for (std::size_t i = 0;
             i < meshes.size();
             i++) {

            render_mesh(
                meshes[i],
                static_cast<int>(i),
                view,
                projection,
                show_triangle_boxes != 0,
                use_z_buffer != 0,
                use_backface_culling != 0,
                use_top_left_rule != 0,
                show_wireframe != 0);
        }

        if (show_depth_map != 0 &&
            use_z_buffer != 0 &&
            show_triangle_boxes == 0) {

            draw_depth_map();
        }

        mu_begin(ctx);

        if (mu_begin_window(
                ctx,
                "Rasterization Controls",
                mu_rect(
                    15,
                    15,
                    415,
                    1160))) {

            int full_width[] = {-1};

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_label(
                ctx,
                "Assignment 4: Triangle Rasterization");

            mu_text(
                ctx,
                "Left drag rotates the selected model. "
                "Arrow keys move the camera. "
                "W and S move the camera forward and backward.");

            gui_label(
                ctx,
                "ACTIVE MODEL");

            int model_widths[] =
                {115, 115, 115};

            mu_layout_row(
                ctx,
                3,
                model_widths,
                0);

            if (mu_button(
                    ctx,
                    "Cube")) {

                active_model = 0;
            }

            if (meshes.size() > 1) {
                if (mu_button(
                        ctx,
                        "Pyramid")) {

                    active_model = 1;
                }
            } else {
                mu_label(
                    ctx,
                    "Unavailable");
            }

            if (meshes.size() > 2) {
                if (mu_button(
                        ctx,
                        "Octahedron")) {

                    active_model = 2;
                }
            } else {
                mu_label(
                    ctx,
                    "Unavailable");
            }

            Mesh &selected =
                meshes[active_model];

            char selected_text[128];

            std::snprintf(
                selected_text,
                sizeof(selected_text),
                "Selected model: %s",
                selected.name.c_str());

            gui_label(
                ctx,
                selected_text);

            char vertices_text[64];

            std::snprintf(
                vertices_text,
                sizeof(vertices_text),
                "Vertices: %zu",
                selected.vertices.size());

            gui_label(
                ctx,
                vertices_text);

            char triangles_text[64];

            std::snprintf(
                triangles_text,
                sizeof(triangles_text),
                "Triangles: %zu",
                selected.faces.size());

            gui_label(
                ctx,
                triangles_text);

            gui_label(
                ctx,
                "RASTERIZATION MODES");

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Triangle Bounding Boxes",
                &show_triangle_boxes);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Enable Z-Buffer",
                &use_z_buffer);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Show Depth Map",
                &show_depth_map);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Backface Culling",
                &use_backface_culling);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Top-Left Fill Rule",
                &use_top_left_rule);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Wireframe Overlay",
                &show_wireframe);

            gui_label(
                ctx,
                "PROJECTION");

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            const char *projection_button =
                camera.perspective
                    ? "Switch to Orthographic"
                    : "Switch to Perspective";

            if (mu_button(
                    ctx,
                    projection_button)) {

                camera.perspective =
                    !camera.perspective;
            }

            gui_label(
                ctx,
                camera.perspective
                    ? "Current: Perspective"
                    : "Current: Orthographic");

            gui_label(
                ctx,
                "CAMERA");

            gui_slider(
                ctx,
                "Camera X",
                camera.position.x,
                -8.0f,
                8.0f);

            gui_slider(
                ctx,
                "Camera Y",
                camera.position.y,
                -8.0f,
                8.0f);

            gui_slider(
                ctx,
                "Camera Z",
                camera.position.z,
                2.0f,
                18.0f);

            gui_slider(
                ctx,
                "Field of View",
                camera.field_of_view,
                15.0f,
                110.0f);

            gui_label(
                ctx,
                "MODEL TRANSFORMATIONS");

            gui_slider(
                ctx,
                "Translate X",
                selected.transform.translation.x,
                -3.0f,
                3.0f);

            gui_slider(
                ctx,
                "Translate Y",
                selected.transform.translation.y,
                -3.0f,
                3.0f);

            gui_slider(
                ctx,
                "Translate Z",
                selected.transform.translation.z,
                -3.0f,
                3.0f);

            gui_slider(
                ctx,
                "Rotate X",
                selected.transform.rotation.x,
                -180.0f,
                180.0f);

            gui_slider(
                ctx,
                "Rotate Y",
                selected.transform.rotation.y,
                -180.0f,
                180.0f);

            gui_slider(
                ctx,
                "Rotate Z",
                selected.transform.rotation.z,
                -180.0f,
                180.0f);

            gui_slider(
                ctx,
                "Scale",
                selected.transform.scale.x,
                0.2f,
                2.5f);

            selected.transform.scale.y =
                selected.transform.scale.x;

            selected.transform.scale.z =
                selected.transform.scale.x;

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            if (mu_button(
                    ctx,
                    "Reset Active Model")) {

                reset_transform(
                    selected.transform);
            }

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            if (mu_button(
                    ctx,
                    "Reset Camera")) {

                reset_camera(camera);
            }

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            if (mu_button(
                    ctx,
                    "Arrange Models")) {

                reset_transform(
                    meshes[0].transform);

                meshes[0].transform.translation =
                    glm::vec3(
                        -1.35f,
                        -0.35f,
                        0.0f);

                meshes[0].transform.rotation =
                    glm::vec3(
                        20.0f,
                        30.0f,
                        0.0f);

                if (meshes.size() > 1) {
                    reset_transform(
                        meshes[1].transform);

                    meshes[1].transform.translation =
                        glm::vec3(
                            1.35f,
                            -0.35f,
                            0.0f);

                    meshes[1].transform.rotation =
                        glm::vec3(
                            15.0f,
                            -25.0f,
                            0.0f);
                }

                if (meshes.size() > 2) {
                    reset_transform(
                        meshes[2].transform);

                    meshes[2].transform.translation =
                        glm::vec3(
                            0.0f,
                            1.35f,
                            0.0f);

                    meshes[2].transform.scale =
                        glm::vec3(0.65f);

                    meshes[2].transform.rotation =
                        glm::vec3(
                            20.0f,
                            25.0f,
                            0.0f);
                }
            }

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            if (mu_button(
                    ctx,
                    "Quit")) {

                quit_requested = true;
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
    std::free(ctx);

    return 0;
}