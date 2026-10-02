#include "MiniFB.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include "microui.h"
}

#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200
  constexpr int UI_WIDTH = 445;
constexpr float PI_VALUE = 3.14159265358979323846f;

static uint32_t g_buffer[WIDTH * HEIGHT];

// ============================================================
// Data structures
// ============================================================

struct Face {
    int a = 0;
    int b = 0;
    int c = 0;
};

struct TransformState {
    glm::vec3 local_translation{0.0f};
    glm::vec3 local_rotation{0.0f};
    glm::vec3 local_scale{1.0f};

    glm::vec3 world_translation{0.0f};
    glm::vec3 world_rotation{0.0f};
    glm::vec3 world_scale{1.0f};
};

struct Mesh {
    std::string name;
    std::string filename;

    std::vector<glm::vec3> vertices;
    std::vector<Face> faces;

    std::vector<glm::vec3> face_normals;
    std::vector<glm::vec3> vertex_normals;

    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    glm::vec3 center{0.0f};

    float normalization_scale = 1.0f;

    TransformState transform;
    uint32_t color = MFB_RGB(255, 255, 255);
};

struct Camera {
    glm::vec3 position{0.0f, 0.0f, 7.0f};
    glm::vec3 rotation{0.0f};

    glm::vec3 target{0.0f, 0.0f, 0.0f};

    float field_of_view = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    bool use_look_at = false;
    bool use_perspective = true;

    bool dolly_zoom_enabled = false;
    float dolly_zoom_value = 0.0f;
};

// ============================================================
// Framebuffer helpers
// ============================================================

void put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        g_buffer[y * WIDTH + x] = color;
    }
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

        int doubled_error = 2 * error;

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
                    10.0f + horizontal * 17.0f);

            uint8_t green =
                static_cast<uint8_t>(
                    17.0f + vertical * 24.0f);

            uint8_t blue =
                static_cast<uint8_t>(
                    31.0f +
                    horizontal * 34.0f +
                    vertical * 15.0f);

            g_buffer[y * WIDTH + x] =
                MFB_RGB(red, green, blue);
        }
    }

    for (int y = 0; y < HEIGHT; y++) {
        put_pixel(
            UI_WIDTH,
            y,
            MFB_RGB(100, 110, 135));

        put_pixel(
            UI_WIDTH + 1,
            y,
            MFB_RGB(100, 110, 135));
    }
}

// ============================================================
// Default OBJ files
// ============================================================

void create_default_obj_files() {
    std::filesystem::create_directories("models");

    if (!std::filesystem::exists("models/cube.obj")) {
        std::ofstream file("models/cube.obj");

        file <<
            "# Cube\n"
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

    if (!std::filesystem::exists("models/pyramid.obj")) {
        std::ofstream file("models/pyramid.obj");

        file <<
            "# Pyramid\n"
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
            "# Octahedron\n"
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
// OBJ loading and geometry calculations
// ============================================================

int parse_obj_index(
    const std::string &token,
    int vertex_count) {

    std::string index_text = token;

    std::size_t slash =
        index_text.find('/');

    if (slash != std::string::npos) {
        index_text =
            index_text.substr(0, slash);
    }

    if (index_text.empty()) {
        return -1;
    }

    int index = std::stoi(index_text);

    if (index > 0) {
        return index - 1;
    }

    if (index < 0) {
        return vertex_count + index;
    }

    return -1;
}

void calculate_bounding_box(Mesh &mesh) {
    if (mesh.vertices.empty()) {
        mesh.minimum = glm::vec3(0.0f);
        mesh.maximum = glm::vec3(0.0f);
        mesh.center = glm::vec3(0.0f);
        mesh.normalization_scale = 1.0f;
        return;
    }

    mesh.minimum = mesh.vertices.front();
    mesh.maximum = mesh.vertices.front();

    for (const glm::vec3 &vertex : mesh.vertices) {
        mesh.minimum =
            glm::min(mesh.minimum, vertex);

        mesh.maximum =
            glm::max(mesh.maximum, vertex);
    }

    mesh.center =
        (mesh.minimum + mesh.maximum) * 0.5f;

    glm::vec3 dimensions =
        mesh.maximum - mesh.minimum;

    float largest_dimension =
        std::max({
            dimensions.x,
            dimensions.y,
            dimensions.z
        });

    if (largest_dimension < 0.0001f) {
        largest_dimension = 1.0f;
    }

    mesh.normalization_scale =
        2.0f / largest_dimension;
}

void calculate_normals(Mesh &mesh) {
    mesh.face_normals.clear();

    mesh.vertex_normals.assign(
        mesh.vertices.size(),
        glm::vec3(0.0f));

    for (const Face &face : mesh.faces) {
        const glm::vec3 &a =
            mesh.vertices[face.a];

        const glm::vec3 &b =
            mesh.vertices[face.b];

        const glm::vec3 &c =
            mesh.vertices[face.c];

        glm::vec3 edge_ab = b - a;
        glm::vec3 edge_ac = c - a;

        glm::vec3 normal =
            glm::cross(edge_ab, edge_ac);

        float length =
            glm::length(normal);

        if (length > 0.0001f) {
            normal /= length;
        } else {
            normal = glm::vec3(0.0f);
        }

        mesh.face_normals.push_back(normal);

        mesh.vertex_normals[face.a] += normal;
        mesh.vertex_normals[face.b] += normal;
        mesh.vertex_normals[face.c] += normal;
    }

    for (glm::vec3 &normal :
         mesh.vertex_normals) {

        float length =
            glm::length(normal);

        if (length > 0.0001f) {
            normal /= length;
        }
    }
}

bool load_obj(
    const std::string &filename,
    Mesh &mesh) {

    std::ifstream file(filename);

    if (!file.is_open()) {
        std::printf(
            "Could not open OBJ file: %s\n",
            filename.c_str());

        return false;
    }

    mesh.filename = filename;
    mesh.vertices.clear();
    mesh.faces.clear();

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
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
                mesh.vertices.push_back(vertex);
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

    calculate_bounding_box(mesh);
    calculate_normals(mesh);

    std::printf(
        "Loaded %s: %zu vertices, %zu faces\n",
        filename.c_str(),
        mesh.vertices.size(),
        mesh.faces.size());

    return !mesh.vertices.empty() &&
           !mesh.faces.empty();
}

// ============================================================
// Transformation matrices
// ============================================================

glm::mat4 create_rotation_matrix(
    const glm::vec3 &rotation_degrees) {

    glm::mat4 matrix(1.0f);

    matrix = glm::rotate(
        matrix,
        glm::radians(rotation_degrees.x),
        glm::vec3(1.0f, 0.0f, 0.0f));

    matrix = glm::rotate(
        matrix,
        glm::radians(rotation_degrees.y),
        glm::vec3(0.0f, 1.0f, 0.0f));

    matrix = glm::rotate(
        matrix,
        glm::radians(rotation_degrees.z),
        glm::vec3(0.0f, 0.0f, 1.0f));

    return matrix;
}

glm::mat4 create_local_world_matrix(
    const Mesh &mesh) {

    glm::mat4 local_translation =
        glm::translate(
            glm::mat4(1.0f),
            mesh.transform.local_translation);

    glm::mat4 local_rotation =
        create_rotation_matrix(
            mesh.transform.local_rotation);

    glm::mat4 local_scale =
        glm::scale(
            glm::mat4(1.0f),
            mesh.transform.local_scale);

    glm::mat4 world_translation =
        glm::translate(
            glm::mat4(1.0f),
            mesh.transform.world_translation);

    glm::mat4 world_rotation =
        create_rotation_matrix(
            mesh.transform.world_rotation);

    glm::mat4 world_scale =
        glm::scale(
            glm::mat4(1.0f),
            mesh.transform.world_scale);

    glm::mat4 local_matrix =
        local_translation *
        local_rotation *
        local_scale;

    glm::mat4 world_matrix =
        world_translation *
        world_rotation *
        world_scale;

    return world_matrix * local_matrix;
}

glm::mat4 create_model_matrix(
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

    return create_local_world_matrix(mesh) *
           normalization;
}
   // ============================================================
// Camera and projection
// ============================================================

glm::mat4 create_view_matrix(
    const Camera &camera) {

    if (camera.use_look_at) {
        glm::vec3 forward =
            camera.target -
            camera.position;

        if (glm::length(forward) < 0.0001f) {
            forward =
                glm::vec3(
                    0.0f,
                    0.0f,
                    -1.0f);
        }

        return glm::lookAt(
            camera.position,
            camera.target,
            glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 camera_transform(1.0f);

    camera_transform =
        glm::translate(
            camera_transform,
            camera.position);

    camera_transform *=
        create_rotation_matrix(
            camera.rotation);

    return glm::inverse(camera_transform);
}

glm::mat4 create_projection_matrix(
    const Camera &camera) {

    float viewport_width =
        static_cast<float>(
            WIDTH - UI_WIDTH);

    float viewport_height =
        static_cast<float>(HEIGHT);

    float aspect_ratio =
        viewport_width / viewport_height;

    if (camera.use_perspective) {
        return glm::perspective(
            glm::radians(
                camera.field_of_view),
            aspect_ratio,
            camera.near_plane,
            camera.far_plane);
    }

    constexpr float ortho_height = 3.4f;
    float ortho_width =
        ortho_height * aspect_ratio;

    return glm::ortho(
        -ortho_width,
        ortho_width,
        -ortho_height,
        ortho_height,
        camera.near_plane,
        camera.far_plane);
}

void apply_dolly_zoom(Camera &camera) {
    if (!camera.dolly_zoom_enabled) {
        return;
    }

    constexpr float base_distance = 7.0f;
    constexpr float base_fov = 60.0f;

    float distance =
        base_distance +
        camera.dolly_zoom_value;

    distance =
        std::clamp(
            distance,
            2.0f,
            18.0f);

    camera.position.z = distance;

    float constant =
        base_distance *
        std::tan(
            glm::radians(base_fov) *
            0.5f);

    float new_fov_radians =
        2.0f *
        std::atan(
            constant / distance);

    camera.field_of_view =
        glm::degrees(new_fov_radians);

    camera.field_of_view =
        std::clamp(
            camera.field_of_view,
            15.0f,
            110.0f);
}

// ============================================================
// Projection helpers
// ============================================================

bool project_point(
    const glm::vec3 &world_point,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix,
    glm::ivec2 &screen_point) {

    glm::vec4 clip =
        projection_matrix *
        view_matrix *
        glm::vec4(world_point, 1.0f);

    if (std::abs(clip.w) < 0.00001f) {
        return false;
    }

    if (clip.w <= 0.0f) {
        return false;
    }

    glm::vec3 ndc =
        glm::vec3(clip) / clip.w;

    if (ndc.z < -1.2f ||
        ndc.z > 1.2f) {

        return false;
    }

    int viewport_width =
        WIDTH - UI_WIDTH;

    int screen_x =
        UI_WIDTH +
        static_cast<int>(
            (ndc.x * 0.5f + 0.5f) *
            static_cast<float>(
                viewport_width));

    int screen_y =
        static_cast<int>(
            (1.0f -
             (ndc.y * 0.5f + 0.5f)) *
            static_cast<float>(HEIGHT));

    screen_point =
        glm::ivec2(screen_x, screen_y);

    return true;
}

void draw_projected_line(
    const glm::vec3 &start,
    const glm::vec3 &end,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix,
    uint32_t color) {

    glm::ivec2 screen_start;
    glm::ivec2 screen_end;

    bool start_visible =
        project_point(
            start,
            view_matrix,
            projection_matrix,
            screen_start);

    bool end_visible =
        project_point(
            end,
            view_matrix,
            projection_matrix,
            screen_end);

    if (!start_visible || !end_visible) {
        return;
    }

    draw_line_bresenham(
        screen_start.x,
        screen_start.y,
        screen_end.x,
        screen_end.y,
        color);
}

void draw_world_axes(
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix) {

    glm::vec3 origin(0.0f);

    draw_projected_line(
        origin,
        glm::vec3(1.5f, 0.0f, 0.0f),
        view_matrix,
        projection_matrix,
        MFB_RGB(255, 60, 60));

    draw_projected_line(
        origin,
        glm::vec3(0.0f, 1.5f, 0.0f),
        view_matrix,
        projection_matrix,
        MFB_RGB(60, 255, 80));

    draw_projected_line(
        origin,
        glm::vec3(0.0f, 0.0f, 1.5f),
        view_matrix,
        projection_matrix,
        MFB_RGB(80, 130, 255));
}

void draw_local_axes(
    const Mesh &mesh,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix) {

    glm::mat4 frame_matrix =
        create_local_world_matrix(mesh);

    glm::vec3 origin =
        glm::vec3(
            frame_matrix *
            glm::vec4(
                0.0f,
                0.0f,
                0.0f,
                1.0f));

    glm::vec3 x_end =
        glm::vec3(
            frame_matrix *
            glm::vec4(
                0.85f,
                0.0f,
                0.0f,
                1.0f));

    glm::vec3 y_end =
        glm::vec3(
            frame_matrix *
            glm::vec4(
                0.0f,
                0.85f,
                0.0f,
                1.0f));

    glm::vec3 z_end =
        glm::vec3(
            frame_matrix *
            glm::vec4(
                0.0f,
                0.0f,
                0.85f,
                1.0f));

    draw_projected_line(
        origin,
        x_end,
        view_matrix,
        projection_matrix,
        MFB_RGB(255, 60, 60));

    draw_projected_line(
        origin,
        y_end,
        view_matrix,
        projection_matrix,
        MFB_RGB(60, 255, 80));

    draw_projected_line(
        origin,
        z_end,
        view_matrix,
        projection_matrix,
        MFB_RGB(80, 130, 255));
}

void draw_bounding_box(
    const Mesh &mesh,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix) {

    glm::mat4 model_matrix =
        create_model_matrix(mesh);

    const glm::vec3 &minimum =
        mesh.minimum;

    const glm::vec3 &maximum =
        mesh.maximum;

    std::array<glm::vec3, 8> local_corners = {
        glm::vec3(minimum.x, minimum.y, minimum.z),
        glm::vec3(maximum.x, minimum.y, minimum.z),
        glm::vec3(maximum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, minimum.y, maximum.z),
        glm::vec3(maximum.x, minimum.y, maximum.z),
        glm::vec3(maximum.x, maximum.y, maximum.z),
        glm::vec3(minimum.x, maximum.y, maximum.z)
    };

    std::array<glm::vec3, 8> world_corners;

    for (int i = 0; i < 8; i++) {
        world_corners[i] =
            glm::vec3(
                model_matrix *
                glm::vec4(
                    local_corners[i],
                    1.0f));
    }

    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    uint32_t color =
        MFB_RGB(255, 230, 80);

    for (const auto &edge : edges) {
        draw_projected_line(
            world_corners[edge[0]],
            world_corners[edge[1]],
            view_matrix,
            projection_matrix,
            color);
    }
}

void draw_face_normals(
    const Mesh &mesh,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix) {

    glm::mat4 model_matrix =
        create_model_matrix(mesh);

    glm::mat3 normal_matrix =
        glm::transpose(
            glm::inverse(
                glm::mat3(model_matrix)));

    constexpr float normal_length = 0.35f;

    for (std::size_t i = 0;
         i < mesh.faces.size();
         i++) {

        const Face &face =
            mesh.faces[i];

        glm::vec3 local_center =
            (
                mesh.vertices[face.a] +
                mesh.vertices[face.b] +
                mesh.vertices[face.c]
            ) / 3.0f;

        glm::vec3 world_center =
            glm::vec3(
                model_matrix *
                glm::vec4(
                    local_center,
                    1.0f));

        glm::vec3 world_normal =
            normal_matrix *
            mesh.face_normals[i];

        if (glm::length(world_normal) >
            0.0001f) {

            world_normal =
                glm::normalize(world_normal);
        }

        glm::vec3 end =
            world_center +
            world_normal *
            normal_length;

        draw_projected_line(
            world_center,
            end,
            view_matrix,
            projection_matrix,
            MFB_RGB(255, 120, 255));
    }
}

void draw_vertex_normals(
    const Mesh &mesh,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix) {

    glm::mat4 model_matrix =
        create_model_matrix(mesh);

    glm::mat3 normal_matrix =
        glm::transpose(
            glm::inverse(
                glm::mat3(model_matrix)));

    constexpr float normal_length = 0.28f;

    for (std::size_t i = 0;
         i < mesh.vertices.size();
         i++) {

        glm::vec3 world_vertex =
            glm::vec3(
                model_matrix *
                glm::vec4(
                    mesh.vertices[i],
                    1.0f));

        glm::vec3 world_normal =
            normal_matrix *
            mesh.vertex_normals[i];

        if (glm::length(world_normal) >
            0.0001f) {

            world_normal =
                glm::normalize(world_normal);
        }

        glm::vec3 end =
            world_vertex +
            world_normal *
            normal_length;

        draw_projected_line(
            world_vertex,
            end,
            view_matrix,
            projection_matrix,
            MFB_RGB(80, 255, 255));
    }
}

// ============================================================
// Mesh rendering
// ============================================================

void render_mesh(
    const Mesh &mesh,
    const glm::mat4 &view_matrix,
    const glm::mat4 &projection_matrix) {

    if (mesh.vertices.empty() ||
        mesh.faces.empty()) {

        return;
    }

    glm::mat4 model_matrix =
        create_model_matrix(mesh);

    std::vector<glm::vec3> world_vertices;
    world_vertices.reserve(
        mesh.vertices.size());

    for (const glm::vec3 &vertex :
         mesh.vertices) {

        world_vertices.push_back(
            glm::vec3(
                model_matrix *
                glm::vec4(vertex, 1.0f)));
    }

    for (const Face &face : mesh.faces) {
        draw_projected_line(
            world_vertices[face.a],
            world_vertices[face.b],
            view_matrix,
            projection_matrix,
            mesh.color);

        draw_projected_line(
            world_vertices[face.b],
            world_vertices[face.c],
            view_matrix,
            projection_matrix,
            mesh.color);

        draw_projected_line(
            world_vertices[face.c],
            world_vertices[face.a],
            view_matrix,
            projection_matrix,
            mesh.color);
    }
}

// ============================================================
// Reset functions
// ============================================================

void reset_transform(
    TransformState &transform) {

    transform.local_translation =
        glm::vec3(0.0f);

    transform.local_rotation =
        glm::vec3(0.0f);

    transform.local_scale =
        glm::vec3(1.0f);

    transform.world_translation =
        glm::vec3(0.0f);

    transform.world_rotation =
        glm::vec3(0.0f);

    transform.world_scale =
        glm::vec3(1.0f);
}

void reset_camera(Camera &camera) {
    camera.position =
        glm::vec3(0.0f, 0.0f, 7.0f);

    camera.rotation =
        glm::vec3(0.0f);

    camera.target =
        glm::vec3(0.0f);

    camera.field_of_view = 60.0f;

    camera.use_look_at = false;
    camera.use_perspective = true;

    camera.dolly_zoom_enabled = false;
    camera.dolly_zoom_value = 0.0f;
}

// ============================================================
// GUI helpers
// ============================================================

void gui_label(
    mu_Context *ctx,
    const char *text) {

    int width[] = {-1};

    mu_layout_row(
        ctx,
        1,
        width,
        0);

    mu_label(ctx, text);
}

void gui_slider(
    mu_Context *ctx,
    const char *label,
    float &value,
    float minimum,
    float maximum) {

    int widths[] = {150, -1};

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

void draw_transform_controls(
    mu_Context *ctx,
    TransformState &transform) {

    gui_label(
        ctx,
        "LOCAL TRANSFORMATIONS");

    gui_slider(
        ctx,
        "Local Translate X",
        transform.local_translation.x,
        -2.5f,
        2.5f);

    gui_slider(
        ctx,
        "Local Translate Y",
        transform.local_translation.y,
        -2.5f,
        2.5f);

    gui_slider(
        ctx,
        "Local Translate Z",
        transform.local_translation.z,
        -2.5f,
        2.5f);

    gui_slider(
        ctx,
        "Local Rotate X",
        transform.local_rotation.x,
        -180.0f,
        180.0f);

    gui_slider(
        ctx,
        "Local Rotate Y",
        transform.local_rotation.y,
        -180.0f,
        180.0f);

    gui_slider(
        ctx,
        "Local Rotate Z",
        transform.local_rotation.z,
        -180.0f,
        180.0f);

    gui_label(
        ctx,
        "WORLD TRANSFORMATIONS");

    gui_slider(
        ctx,
        "World Translate X",
        transform.world_translation.x,
        -3.0f,
        3.0f);

    gui_slider(
        ctx,
        "World Translate Y",
        transform.world_translation.y,
        -3.0f,
        3.0f);

    gui_slider(
        ctx,
        "World Translate Z",
        transform.world_translation.z,
        -3.0f,
        3.0f);

    gui_slider(
        ctx,
        "World Rotate X",
        transform.world_rotation.x,
        -180.0f,
        180.0f);

    gui_slider(
        ctx,
        "World Rotate Y",
        transform.world_rotation.y,
        -180.0f,
        180.0f);

    gui_slider(
        ctx,
        "World Rotate Z",
        transform.world_rotation.z,
        -180.0f,
        180.0f);
}

// ============================================================
// Main
// ============================================================

int main() {
    create_default_obj_files();

    std::vector<Mesh> meshes;

    Mesh cube;
    cube.name = "Cube";
    cube.color = MFB_RGB(255, 120, 120);

    if (load_obj(
            "models/cube.obj",
            cube)) {

        cube.transform.world_translation.x =
            -1.35f;

        cube.transform.local_rotation =
            glm::vec3(
                20.0f,
                30.0f,
                0.0f);

        meshes.push_back(cube);
    }

    Mesh pyramid;
    pyramid.name = "Pyramid";
    pyramid.color = MFB_RGB(120, 255, 160);

    if (load_obj(
            "models/pyramid.obj",
            pyramid)) {

        pyramid.transform.world_translation.x =
            1.35f;

        pyramid.transform.local_rotation =
            glm::vec3(
                15.0f,
                -25.0f,
                0.0f);

        meshes.push_back(pyramid);
    }

    Mesh octahedron;
    octahedron.name = "Octahedron";
    octahedron.color =
        MFB_RGB(120, 180, 255);

    if (load_obj(
            "models/octahedron.obj",
            octahedron)) {

        octahedron.transform
            .world_translation.y =
            1.35f;

        octahedron.transform.local_scale =
            glm::vec3(0.65f);

        meshes.push_back(octahedron);
    }

    if (meshes.empty()) {
        std::printf(
            "No OBJ models were loaded.\n");

        return 1;
    }

    Camera camera;

    int active_model = 0;

    int show_world_axes = 1;
    int show_local_axes = 1;
    int show_bounding_box = 0;
    int show_face_normals = 0;
    int show_vertex_normals = 0;

    struct mfb_window *window =
        mfb_open_ex(
            "Assignment 3 - Virtual Cameras",
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

            return (
                       length < 0
                           ? static_cast<int>(
                                 std::strlen(text))
                           : length) *
                   8;
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

    bool previous_left_down = false;
    bool previous_right_down = false;

    bool rotating_model = false;
    bool moving_camera = false;

    int previous_mouse_x = 0;
    int previous_mouse_y = 0;

    while (
        mfb_update_events(window) !=
        MFB_STATE_EXIT) {

        ui_bridge_input(ctx, window);

        Mesh &active_mesh =
            meshes[active_model];

        const uint8_t *keys =
            mfb_get_key_buffer(window);

        bool mouse_inside_scene =
            ctx->mouse_pos.x > UI_WIDTH &&
            ctx->mouse_pos.x < WIDTH &&
            ctx->mouse_pos.y >= 0 &&
            ctx->mouse_pos.y < HEIGHT;

        bool left_down =
            (ctx->mouse_down &
             MU_MOUSE_LEFT) != 0;

        bool right_down =
            (ctx->mouse_down &
             MU_MOUSE_RIGHT) != 0;

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
                .local_rotation.y +=
                static_cast<float>(
                    delta_x) *
                0.55f;

            active_mesh.transform
                .local_rotation.x +=
                static_cast<float>(
                    delta_y) *
                0.55f;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        if (right_down &&
            !previous_right_down &&
            mouse_inside_scene) {

            moving_camera = true;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        if (!right_down) {
            moving_camera = false;
        }

        if (moving_camera &&
            right_down) {

            int delta_x =
                ctx->mouse_pos.x -
                previous_mouse_x;

            int delta_y =
                ctx->mouse_pos.y -
                previous_mouse_y;

            camera.position.x -=
                static_cast<float>(
                    delta_x) *
                0.01f;

            camera.position.y +=
                static_cast<float>(
                    delta_y) *
                0.01f;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        previous_left_down =
            left_down;

        previous_right_down =
            right_down;

        apply_dolly_zoom(camera);

        glm::mat4 view_matrix =
            create_view_matrix(camera);

        glm::mat4 projection_matrix =
            create_projection_matrix(camera);

        clear_background();

        if (show_world_axes != 0) {
            draw_world_axes(
                view_matrix,
                projection_matrix);
        }

        for (const Mesh &mesh : meshes) {
            render_mesh(
                mesh,
                view_matrix,
                projection_matrix);
        }

        if (show_local_axes != 0) {
            draw_local_axes(
                active_mesh,
                view_matrix,
                projection_matrix);
        }

        if (show_bounding_box != 0) {
            draw_bounding_box(
                active_mesh,
                view_matrix,
                projection_matrix);
        }

        if (show_face_normals != 0) {
            draw_face_normals(
                active_mesh,
                view_matrix,
                projection_matrix);
        }

        if (show_vertex_normals != 0) {
            draw_vertex_normals(
                active_mesh,
                view_matrix,
                projection_matrix);
        }

        mu_begin(ctx);

        if (mu_begin_window(
                ctx,
                "Virtual Camera Controls",
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
                "Assignment 3: Cameras and Projections");

            mu_text(
                ctx,
                "Left drag rotates the active model. "
                "Right drag moves the camera. "
                "Arrow keys move camera X/Y. "
                "W and S move camera Z.");

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

            if (mu_button(ctx, "Cube")) {
                active_model = 0;
            }

            if (meshes.size() > 1) {
                if (mu_button(
                        ctx,
                        "Pyramid")) {

                    active_model = 1;
                }
            } else {
                mu_label(ctx, "Unavailable");
            }

            if (meshes.size() > 2) {
                if (mu_button(
                        ctx,
                        "Octahedron")) {

                    active_model = 2;
                }
            } else {
                mu_label(ctx, "Unavailable");
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
                "Number of vertices: %zu",
                selected.vertices.size());

            gui_label(
                ctx,
                vertices_text);

            char faces_text[64];

            std::snprintf(
                faces_text,
                sizeof(faces_text),
                "Number of faces: %zu",
                selected.faces.size());

            gui_label(
                ctx,
                faces_text);

            gui_label(
                ctx,
                "DEBUG VISUALIZATION");

            int checkbox_widths[] =
                {190, -1};

            mu_layout_row(
                ctx,
                2,
                checkbox_widths,
                0);

            mu_checkbox(
                ctx,
                "World Axes",
                &show_world_axes);

            mu_checkbox(
                ctx,
                "Local Axes",
                &show_local_axes);

            mu_layout_row(
                ctx,
                2,
                checkbox_widths,
                0);

            mu_checkbox(
                ctx,
                "Bounding Box",
                &show_bounding_box);

            mu_checkbox(
                ctx,
                "Face Normals",
                &show_face_normals);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Vertex Normals",
                &show_vertex_normals);

            gui_label(
                ctx,
                "PROJECTION");

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            const char *projection_button =
                camera.use_perspective
                    ? "Switch to Orthographic"
                    : "Switch to Perspective";

            if (mu_button(
                    ctx,
                    projection_button)) {

                camera.use_perspective =
                    !camera.use_perspective;
            }

            gui_label(
                ctx,
                camera.use_perspective
                    ? "Current: Perspective"
                    : "Current: Orthographic");

            gui_label(
                ctx,
                "CAMERA POSITION");

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

            gui_label(
                ctx,
                "CAMERA ROTATION");

            gui_slider(
                ctx,
                "Camera Rotate X",
                camera.rotation.x,
                -180.0f,
                180.0f);

            gui_slider(
                ctx,
                "Camera Rotate Y",
                camera.rotation.y,
                -180.0f,
                180.0f);

            gui_slider(
                ctx,
                "Camera Rotate Z",
                camera.rotation.z,
                -180.0f,
                180.0f);

            gui_slider(
                ctx,
                "Field of View",
                camera.field_of_view,
                15.0f,
                110.0f);

            gui_label(
                ctx,
                "LOOKAT CAMERA");

            int look_at_value =
                camera.use_look_at ? 1 : 0;

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Enable LookAt",
                &look_at_value);

            camera.use_look_at =
                look_at_value != 0;

            gui_slider(
                ctx,
                "Target X",
                camera.target.x,
                -5.0f,
                5.0f);

            gui_slider(
                ctx,
                "Target Y",
                camera.target.y,
                -5.0f,
                5.0f);

            gui_slider(
                ctx,
                "Target Z",
                camera.target.z,
                -5.0f,
                5.0f);

            gui_label(
                ctx,
                "DOLLY ZOOM");

            int dolly_value =
                camera.dolly_zoom_enabled
                    ? 1
                    : 0;

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Enable Dolly Zoom",
                &dolly_value);

            camera.dolly_zoom_enabled =
                dolly_value != 0;

            gui_slider(
                ctx,
                "Dolly Amount",
                camera.dolly_zoom_value,
                -4.5f,
                10.0f);

            draw_transform_controls(
                ctx,
                selected.transform);

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

                meshes[0].transform
                    .world_translation.x =
                    -1.35f;

                if (meshes.size() > 1) {
                    reset_transform(
                        meshes[1].transform);

                    meshes[1].transform
                        .world_translation.x =
                        1.35f;
                }

                if (meshes.size() > 2) {
                    reset_transform(
                        meshes[2].transform);

                    meshes[2].transform
                        .world_translation.y =
                        1.35f;

                    meshes[2].transform
                        .local_scale =
                        glm::vec3(0.65f);
                }
            }

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            if (mu_button(ctx, "Quit")) {
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