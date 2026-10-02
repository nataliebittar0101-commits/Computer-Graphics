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
#include <limits>
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
constexpr float EPSILON = 0.00001f;
constexpr float PI_VALUE = 3.14159265358979323846f;

static uint32_t g_buffer[WIDTH * HEIGHT];
static float g_z_buffer[WIDTH * HEIGHT];
// ============================================================
// Data structures
// ============================================================

struct Face {
    int a = 0;
    int b = 0;
    int c = 0;

    int ta = -1;
    int tb = -1;
    int tc = -1;
};

struct TransformState {
    glm::vec3 translation{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

struct Material {
    glm::vec3 ambient{0.25f, 0.10f, 0.10f};
    glm::vec3 diffuse{0.85f, 0.30f, 0.25f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};

    float shininess = 32.0f;
};

struct PointLight {
    glm::vec3 position{2.5f, 3.5f, 4.0f};

    glm::vec3 ambient{0.30f, 0.30f, 0.30f};
    glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};
};

struct Mesh {
    std::string name;
    std::string filename;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texture_coordinates;
    std::vector<Face> faces;

    std::vector<glm::vec3> face_normals;
    std::vector<glm::vec3> vertex_normals;

    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    glm::vec3 center{0.0f};

    float normalization_scale = 1.0f;

    TransformState transform;
    Material material;
};

struct Camera {
    glm::vec3 position{0.0f, 0.0f, 7.0f};

    float field_of_view = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    bool perspective = true;
};

enum class ShadingMode {
    Flat = 0,
    Gouraud = 1,
    Phong = 2
};

struct RasterVertex {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;

    glm::vec3 world_position{0.0f};
    glm::vec3 world_normal{0.0f};
    glm::vec2 uv{0.0f};

    glm::vec3 gouraud_color{1.0f};

    bool valid = false;
};

struct Texture {
    int width = 0;
    int height = 0;

    std::vector<uint32_t> pixels;

    bool valid() const {
        return width > 0 &&
               height > 0 &&
               !pixels.empty();
    }
};

// ============================================================
// Utility functions
// ============================================================

glm::vec3 clamp_color(
    const glm::vec3 &color) {

    return glm::clamp(
        color,
        glm::vec3(0.0f),
        glm::vec3(1.0f));
}

uint32_t color_to_pixel(
    const glm::vec3 &color) {

    glm::vec3 result =
        clamp_color(color);

    uint8_t red =
        static_cast<uint8_t>(
            result.r * 255.0f);

    uint8_t green =
        static_cast<uint8_t>(
            result.g * 255.0f);

    uint8_t blue =
        static_cast<uint8_t>(
            result.b * 255.0f);

    return MFB_RGB(
        red,
        green,
        blue);
}

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

    g_buffer[y * WIDTH + x] =
        color;
}

void clear_background() {
    for (int y = 0;
         y < HEIGHT;
         y++) {

        for (int x = 0;
             x < WIDTH;
             x++) {

            float horizontal =
                static_cast<float>(x) /
                static_cast<float>(WIDTH);

            float vertical =
                static_cast<float>(y) /
                static_cast<float>(HEIGHT);

            uint8_t red =
                static_cast<uint8_t>(
                    8.0f +
                    horizontal * 15.0f);

            uint8_t green =
                static_cast<uint8_t>(
                    13.0f +
                    vertical * 20.0f);

            uint8_t blue =
                static_cast<uint8_t>(
                    32.0f +
                    horizontal * 25.0f +
                    vertical * 12.0f);

            g_buffer[y * WIDTH + x] =
                MFB_RGB(
                    red,
                    green,
                    blue);
        }
    }

    for (int y = 0;
         y < HEIGHT;
         y++) {

        g_buffer[
            y * WIDTH + UI_WIDTH] =
            MFB_RGB(
                110,
                115,
                135);

        g_buffer[
            y * WIDTH +
            UI_WIDTH + 1] =
            MFB_RGB(
                110,
                115,
                135);
    }
}

void clear_z_buffer() {
    float infinity =
        std::numeric_limits<float>
            ::infinity();

    for (float &value :
         g_z_buffer) {

        value = infinity;
    }
}

void draw_line(
    int x0,
    int y0,
    int x1,
    int y1,
    uint32_t color) {

    int dx =
        std::abs(x1 - x0);

    int sx =
        x0 < x1 ? 1 : -1;

    int dy =
        -std::abs(y1 - y0);

    int sy =
        y0 < y1 ? 1 : -1;

    int error =
        dx + dy;

    while (true) {
        put_pixel(
            x0,
            y0,
            color);

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
// Default OBJ files
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

            "vt 0 0\n"
            "vt 1 0\n"
            "vt 1 1\n"
            "vt 0 1\n"

            "f 1/1 2/2 3/3\n"
            "f 1/1 3/3 4/4\n"

            "f 5/1 7/3 6/2\n"
            "f 5/1 8/4 7/3\n"

            "f 1/1 5/2 6/3\n"
            "f 1/1 6/3 2/4\n"

            "f 4/1 3/2 7/3\n"
            "f 4/1 7/3 8/4\n"

            "f 1/1 4/2 8/3\n"
            "f 1/1 8/3 5/4\n"

            "f 2/1 6/2 7/3\n"
            "f 2/1 7/3 3/4\n";
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

            "vt 0 0\n"
            "vt 1 0\n"
            "vt 1 1\n"
            "vt 0 1\n"
            "vt 0.5 1\n"

            "f 1/1 2/2 3/3\n"
            "f 1/1 3/3 4/4\n"

            "f 1/1 5/5 2/2\n"
            "f 2/1 5/5 3/2\n"
            "f 3/1 5/5 4/2\n"
            "f 4/1 5/5 1/2\n";
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

            "vt 0 0\n"
            "vt 1 0\n"
            "vt 0.5 1\n"

            "f 1/1 3/3 5/2\n"
            "f 3/1 2/3 5/2\n"
            "f 2/1 4/3 5/2\n"
            "f 4/1 1/3 5/2\n"

            "f 3/1 1/3 6/2\n"
            "f 2/1 3/3 6/2\n"
            "f 4/1 2/3 6/2\n"
            "f 1/1 4/3 6/2\n";
    }
}

// ============================================================
// OBJ loading
// ============================================================

int parse_obj_index(
    const std::string &text,
    int count) {

    if (text.empty()) {
        return -1;
    }

    int index =
        std::stoi(text);

    if (index > 0) {
        return index - 1;
    }

    if (index < 0) {
        return count + index;
    }

    return -1;
}

void parse_face_token(
    const std::string &token,
    int vertex_count,
    int texture_count,
    int &vertex_index,
    int &texture_index) {

    vertex_index = -1;
    texture_index = -1;

    std::size_t first_slash =
        token.find('/');

    if (first_slash ==
        std::string::npos) {

        vertex_index =
            parse_obj_index(
                token,
                vertex_count);

        return;
    }

    std::string vertex_text =
        token.substr(
            0,
            first_slash);

    vertex_index =
        parse_obj_index(
            vertex_text,
            vertex_count);

    std::size_t second_slash =
        token.find(
            '/',
            first_slash + 1);

    std::string texture_text;

    if (second_slash ==
        std::string::npos) {

        texture_text =
            token.substr(
                first_slash + 1);
    } else {
        texture_text =
            token.substr(
                first_slash + 1,
                second_slash -
                first_slash - 1);
    }

    if (!texture_text.empty()) {
        texture_index =
            parse_obj_index(
                texture_text,
                texture_count);
    }
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
        (
            mesh.minimum +
            mesh.maximum
        ) * 0.5f;

    glm::vec3 dimensions =
        mesh.maximum -
        mesh.minimum;

    float largest_dimension =
        std::max({
            dimensions.x,
            dimensions.y,
            dimensions.z
        });

    if (largest_dimension <
        EPSILON) {

        largest_dimension =
            1.0f;
    }

    mesh.normalization_scale =
        2.0f /
        largest_dimension;
}

void calculate_normals(
    Mesh &mesh) {

    mesh.face_normals.clear();

    mesh.vertex_normals.assign(
        mesh.vertices.size(),
        glm::vec3(0.0f));

    for (const Face &face :
         mesh.faces) {

        const glm::vec3 &a =
            mesh.vertices[face.a];

        const glm::vec3 &b =
            mesh.vertices[face.b];

        const glm::vec3 &c =
            mesh.vertices[face.c];

        glm::vec3 normal =
            glm::cross(
                b - a,
                c - a);

        if (glm::length(normal) >
            EPSILON) {

            normal =
                glm::normalize(normal);
        }

        mesh.face_normals.push_back(
            normal);

        mesh.vertex_normals[face.a] +=
            normal;

        mesh.vertex_normals[face.b] +=
            normal;

        mesh.vertex_normals[face.c] +=
            normal;
    }

    for (glm::vec3 &normal :
         mesh.vertex_normals) {

        if (glm::length(normal) >
            EPSILON) {

            normal =
                glm::normalize(normal);
        }
    }
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

    mesh.filename =
        filename;

    mesh.vertices.clear();
    mesh.texture_coordinates.clear();
    mesh.faces.clear();

    std::string line;

    while (std::getline(
        file,
        line)) {

        if (line.empty() ||
            line[0] == '#') {

            continue;
        }

        std::istringstream stream(
            line);

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
        } else if (type == "vt") {
            glm::vec2 texture_coordinate;

            stream >>
                texture_coordinate.x >>
                texture_coordinate.y;

            if (!stream.fail()) {
                mesh.texture_coordinates
                    .push_back(
                        texture_coordinate);
            }
        } else if (type == "f") {
            std::vector<int>
                polygon_vertices;

            std::vector<int>
                polygon_textures;

            std::string token;

            while (stream >> token) {
                int vertex_index;
                int texture_index;

                parse_face_token(
                    token,
                    static_cast<int>(
                        mesh.vertices.size()),
                    static_cast<int>(
                        mesh.texture_coordinates
                            .size()),
                    vertex_index,
                    texture_index);

                if (vertex_index >= 0 &&
                    vertex_index <
                    static_cast<int>(
                        mesh.vertices.size())) {

                    polygon_vertices
                        .push_back(
                            vertex_index);

                    polygon_textures
                        .push_back(
                            texture_index);
                }
            }

            for (std::size_t i = 1;
                 i + 1 <
                 polygon_vertices.size();
                 i++) {

                Face face;

                face.a =
                    polygon_vertices[0];

                face.b =
                    polygon_vertices[i];

                face.c =
                    polygon_vertices[i + 1];

                face.ta =
                    polygon_textures[0];

                face.tb =
                    polygon_textures[i];

                face.tc =
                    polygon_textures[i + 1];

                mesh.faces.push_back(
                    face);
            }
        }
    }

    calculate_mesh_bounds(mesh);
    calculate_normals(mesh);

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
// Texture generation and sampling
// ============================================================

Texture create_checker_texture() {
    Texture texture;

    texture.width = 128;
    texture.height = 128;

    texture.pixels.resize(
        texture.width *
        texture.height);

    constexpr int square_size =
        16;

    for (int y = 0;
         y < texture.height;
         y++) {

        for (int x = 0;
             x < texture.width;
             x++) {

            bool even_square =
                (
                    x / square_size +
                    y / square_size
                ) % 2 == 0;

            uint32_t color =
                even_square
                    ? MFB_RGB(
                          235,
                          220,
                          165)
                    : MFB_RGB(
                          65,
                          110,
                          170);

            texture.pixels[
                y * texture.width + x] =
                color;
        }
    }

    return texture;
}

glm::vec3 sample_texture(
    const Texture &texture,
    glm::vec2 uv) {

    if (!texture.valid()) {
        return glm::vec3(1.0f);
    }

    uv.x =
        uv.x -
        std::floor(uv.x);

    uv.y =
        uv.y -
        std::floor(uv.y);

    int x =
        static_cast<int>(
            uv.x *
            static_cast<float>(
                texture.width - 1));

    int y =
        static_cast<int>(
            (1.0f - uv.y) *
            static_cast<float>(
                texture.height - 1));

    x =
        std::clamp(
            x,
            0,
            texture.width - 1);

    y =
        std::clamp(
            y,
            0,
            texture.height - 1);

    uint32_t pixel =
        texture.pixels[
            y * texture.width + x];

    float red =
        static_cast<float>(
            (pixel >> 16) & 0xFF) /
        255.0f;

    float green =
        static_cast<float>(
            (pixel >> 8) & 0xFF) /
        255.0f;

    float blue =
        static_cast<float>(
            pixel & 0xFF) /
        255.0f;

    return glm::vec3(
        red,
        green,
        blue);
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
            glm::radians(
                rotation.x),
            glm::vec3(
                1.0f,
                0.0f,
                0.0f));

    matrix =
        glm::rotate(
            matrix,
            glm::radians(
                rotation.y),
            glm::vec3(
                0.0f,
                1.0f,
                0.0f));

    matrix =
        glm::rotate(
            matrix,
            glm::radians(
                rotation.z),
            glm::vec3(
                0.0f,
                0.0f,
                1.0f));

    return matrix;
}

glm::mat4 model_matrix(
    const Mesh &mesh) {

    glm::mat4 normalization(
        1.0f);

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

    float aspect_ratio =
        viewport_width /
        static_cast<float>(HEIGHT);

    if (camera.perspective) {
        return glm::perspective(
            glm::radians(
                camera.field_of_view),
            aspect_ratio,
            camera.near_plane,
            camera.far_plane);
    }

    constexpr float ortho_height =
        3.5f;

    float ortho_width =
        ortho_height *
        aspect_ratio;

    return glm::ortho(
        -ortho_width,
        ortho_width,
        -ortho_height,
        ortho_height,
        camera.near_plane,
        camera.far_plane);
}

// ============================================================
// Lighting
// ============================================================

glm::vec3 calculate_lighting(
    const glm::vec3 &world_position,
    const glm::vec3 &world_normal,
    const Camera &camera,
    const PointLight &light,
    const Material &material,
    bool ambient_enabled,
    bool diffuse_enabled,
    bool specular_enabled,
    const glm::vec3 &diffuse_base_color) {

    glm::vec3 normal =
        world_normal;

    if (glm::length(normal) >
        EPSILON) {

        normal =
            glm::normalize(normal);
    }

    glm::vec3 result(0.0f);

    if (ambient_enabled) {
        glm::vec3 ambient =
            light.ambient *
            material.ambient;

        result += ambient;
    }

    glm::vec3 light_direction =
        light.position -
        world_position;

    if (glm::length(light_direction) >
        EPSILON) {

        light_direction =
            glm::normalize(
                light_direction);
    }

    if (diffuse_enabled) {
        float diffuse_factor =
            std::max(
                glm::dot(
                    normal,
                    light_direction),
                0.0f);

        glm::vec3 diffuse =
            light.diffuse *
            material.diffuse *
            diffuse_base_color *
            diffuse_factor;

        result += diffuse;
    }

    if (specular_enabled) {
        glm::vec3 view_direction =
            camera.position -
            world_position;

        if (glm::length(view_direction) >
            EPSILON) {

            view_direction =
                glm::normalize(
                    view_direction);
        }

        glm::vec3 reflection_direction =
            glm::reflect(
                -light_direction,
                normal);

        float specular_factor =
            std::pow(
                std::max(
                    glm::dot(
                        view_direction,
                        reflection_direction),
                    0.0f),
                material.shininess);

        glm::vec3 specular =
            light.specular *
            material.specular *
            specular_factor;

        result += specular;
    }

    return clamp_color(result);
}

// ============================================================
// Projection
// ============================================================

RasterVertex project_vertex(
    const glm::vec3 &world_position,
    const glm::vec3 &world_normal,
    const glm::vec2 &uv,
    const glm::mat4 &view,
    const glm::mat4 &projection) {

    RasterVertex result;

    glm::vec4 view_position =
        view *
        glm::vec4(
            world_position,
            1.0f);

    glm::vec4 clip_position =
        projection *
        view_position;

    if (std::abs(
            clip_position.w) <
        EPSILON) {

        return result;
    }

    if (clip_position.w <= 0.0f) {
        return result;
    }

    glm::vec3 ndc =
        glm::vec3(
            clip_position) /
        clip_position.w;

    if (ndc.z < -1.0f ||
        ndc.z > 1.0f) {

        return result;
    }

    int viewport_width =
        WIDTH - UI_WIDTH;

    result.x =
        static_cast<float>(
            UI_WIDTH) +
        (
            ndc.x * 0.5f +
            0.5f
        ) *
        static_cast<float>(
            viewport_width);

    result.y =
        (
            1.0f -
            (
                ndc.y * 0.5f +
                0.5f
            )
        ) *
        static_cast<float>(
            HEIGHT);

    result.depth =
        ndc.z * 0.5f +
        0.5f;

    result.world_position =
        world_position;

    result.world_normal =
        world_normal;

    result.uv =
        uv;

    result.valid =
        true;

    return result;
}

bool project_debug_point(
    const glm::vec3 &world_position,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    glm::ivec2 &screen_position) {

    RasterVertex vertex =
        project_vertex(
            world_position,
            glm::vec3(0.0f),
            glm::vec2(0.0f),
            view,
            projection);

    if (!vertex.valid) {
        return false;
    }

    screen_position =
        glm::ivec2(
            static_cast<int>(
                vertex.x),
            static_cast<int>(
                vertex.y));

    return true;
}

void draw_world_line(
    const glm::vec3 &start,
    const glm::vec3 &end,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    uint32_t color) {

    glm::ivec2 start_screen;
    glm::ivec2 end_screen;

    if (!project_debug_point(
            start,
            view,
            projection,
            start_screen) ||
        !project_debug_point(
            end,
            view,
            projection,
            end_screen)) {

        return;
    }

    draw_line(
        start_screen.x,
        start_screen.y,
        end_screen.x,
        end_screen.y,
        color);
}

// ============================================================
// Rasterization
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

bool is_top_left_edge(
    const RasterVertex &a,
    const RasterVertex &b) {

    float dx =
        b.x - a.x;

    float dy =
        b.y - a.y;

    return
        dy < 0.0f ||
        (
            std::abs(dy) <=
            EPSILON &&
            dx > 0.0f
        );
}

bool edge_inside(
    float value,
    bool top_left) {

    if (value >
        EPSILON) {

        return true;
    }

    if (std::abs(value) <=
        EPSILON) {

        return top_left;
    }

    return false;
}

bool is_back_facing(
    const glm::vec3 &world_a,
    const glm::vec3 &world_b,
    const glm::vec3 &world_c,
    const Camera &camera) {

    glm::vec3 normal =
        glm::cross(
            world_b - world_a,
            world_c - world_a);

    if (glm::length(normal) <
        EPSILON) {

        return true;
    }

    normal =
        glm::normalize(normal);

    glm::vec3 center =
        (
            world_a +
            world_b +
            world_c
        ) / 3.0f;

    glm::vec3 direction_to_camera =
        camera.position -
        center;

    return
        glm::dot(
            normal,
            direction_to_camera) <=
        0.0f;
}

void rasterize_triangle(
    RasterVertex v0,
    RasterVertex v1,
    RasterVertex v2,
    const glm::vec3 &flat_color,
    ShadingMode shading_mode,
    const Camera &camera,
    const PointLight &light,
    const Material &material,
    const Texture &texture,
    bool texture_enabled,
    bool ambient_enabled,
    bool diffuse_enabled,
    bool specular_enabled) {

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
        std::swap(
            v1,
            v2);

        area =
            -area;
    }

    int minimum_x =
        static_cast<int>(
            std::floor(
                std::min({
                    v0.x,
                    v1.x,
                    v2.x
                })));

    int maximum_x =
        static_cast<int>(
            std::ceil(
                std::max({
                    v0.x,
                    v1.x,
                    v2.x
                })));

    int minimum_y =
        static_cast<int>(
            std::floor(
                std::min({
                    v0.y,
                    v1.y,
                    v2.y
                })));

    int maximum_y =
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

    bool edge0_top_left =
        is_top_left_edge(
            v1,
            v2);

    bool edge1_top_left =
        is_top_left_edge(
            v2,
            v0);

    bool edge2_top_left =
        is_top_left_edge(
            v0,
            v1);

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

            if (!edge_inside(
                    weight0,
                    edge0_top_left) ||
                !edge_inside(
                    weight1,
                    edge1_top_left) ||
                !edge_inside(
                    weight2,
                    edge2_top_left)) {

                continue;
            }

            float alpha =
                weight0 /
                area;

            float beta =
                weight1 /
                area;

            float gamma =
                weight2 /
                area;

            float depth =
                alpha *
                    v0.depth +
                beta *
                    v1.depth +
                gamma *
                    v2.depth;

            int index =
                y * WIDTH + x;

            if (depth >=
                g_z_buffer[index]) {

                continue;
            }

            glm::vec3 final_color;

            if (shading_mode ==
                ShadingMode::Flat) {

                final_color =
                    flat_color;
            } else if (
                shading_mode ==
                ShadingMode::Gouraud) {

                final_color =
                    alpha *
                        v0.gouraud_color +
                    beta *
                        v1.gouraud_color +
                    gamma *
                        v2.gouraud_color;
            } else {
                glm::vec3 pixel_position =
                    alpha *
                        v0.world_position +
                    beta *
                        v1.world_position +
                    gamma *
                        v2.world_position;

                glm::vec3 pixel_normal =
                    alpha *
                        v0.world_normal +
                    beta *
                        v1.world_normal +
                    gamma *
                        v2.world_normal;

                if (glm::length(
                        pixel_normal) >
                    EPSILON) {

                    pixel_normal =
                        glm::normalize(
                            pixel_normal);
                }

                glm::vec2 pixel_uv =
                    alpha *
                        v0.uv +
                    beta *
                        v1.uv +
                    gamma *
                        v2.uv;

                glm::vec3 texture_color =
                    texture_enabled
                        ? sample_texture(
                              texture,
                              pixel_uv)
                        : glm::vec3(
                              1.0f);

                final_color =
                    calculate_lighting(
                        pixel_position,
                        pixel_normal,
                        camera,
                        light,
                        material,
                        ambient_enabled,
                        diffuse_enabled,
                        specular_enabled,
                        texture_color);
            }

            g_z_buffer[index] =
                depth;

            g_buffer[index] =
                color_to_pixel(
                    final_color);
        }
    }
}

// ============================================================
// Mesh rendering
// ============================================================

void render_mesh(
    const Mesh &mesh,
    const Camera &camera,
    const PointLight &light,
    const Texture &texture,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    ShadingMode shading_mode,
    bool texture_enabled,
    bool ambient_enabled,
    bool diffuse_enabled,
    bool specular_enabled,
    bool backface_culling,
    bool wireframe_overlay,
    bool debug_vectors,
    bool active_mesh) {

    glm::mat4 model =
        model_matrix(mesh);

    glm::mat3 normal_matrix =
        glm::transpose(
            glm::inverse(
                glm::mat3(model)));

    std::vector<glm::vec3>
        world_vertices;

    std::vector<glm::vec3>
        world_vertex_normals;

    world_vertices.reserve(
        mesh.vertices.size());

    world_vertex_normals.reserve(
        mesh.vertex_normals.size());

    for (std::size_t i = 0;
         i < mesh.vertices.size();
         i++) {

        world_vertices.push_back(
            glm::vec3(
                model *
                glm::vec4(
                    mesh.vertices[i],
                    1.0f)));

        glm::vec3 normal =
            normal_matrix *
            mesh.vertex_normals[i];

        if (glm::length(normal) >
            EPSILON) {

            normal =
                glm::normalize(normal);
        }

        world_vertex_normals.push_back(
            normal);
    }

    for (std::size_t face_index = 0;
         face_index <
         mesh.faces.size();
         face_index++) {

        const Face &face =
            mesh.faces[face_index];

        glm::vec3 world_a =
            world_vertices[face.a];

        glm::vec3 world_b =
            world_vertices[face.b];

        glm::vec3 world_c =
            world_vertices[face.c];

        if (backface_culling &&
            is_back_facing(
                world_a,
                world_b,
                world_c,
                camera)) {

            continue;
        }

        glm::vec3 face_normal =
            glm::cross(
                world_b - world_a,
                world_c - world_a);

        if (glm::length(face_normal) >
            EPSILON) {

            face_normal =
                glm::normalize(
                    face_normal);
        }

        glm::vec3 face_center =
            (
                world_a +
                world_b +
                world_c
            ) / 3.0f;

        glm::vec2 uv_a(0.0f);
        glm::vec2 uv_b(1.0f, 0.0f);
        glm::vec2 uv_c(0.5f, 1.0f);

        if (face.ta >= 0 &&
            face.ta <
            static_cast<int>(
                mesh.texture_coordinates
                    .size())) {

            uv_a =
                mesh.texture_coordinates[
                    face.ta];
        }

        if (face.tb >= 0 &&
            face.tb <
            static_cast<int>(
                mesh.texture_coordinates
                    .size())) {

            uv_b =
                mesh.texture_coordinates[
                    face.tb];
        }

        if (face.tc >= 0 &&
            face.tc <
            static_cast<int>(
                mesh.texture_coordinates
                    .size())) {

            uv_c =
                mesh.texture_coordinates[
                    face.tc];
        }

        RasterVertex v0 =
            project_vertex(
                world_a,
                world_vertex_normals[
                    face.a],
                uv_a,
                view,
                projection);

        RasterVertex v1 =
            project_vertex(
                world_b,
                world_vertex_normals[
                    face.b],
                uv_b,
                view,
                projection);

        RasterVertex v2 =
            project_vertex(
                world_c,
                world_vertex_normals[
                    face.c],
                uv_c,
                view,
                projection);

        if (!v0.valid ||
            !v1.valid ||
            !v2.valid) {

            continue;
        }

        glm::vec3 flat_texture_color =
            texture_enabled
                ? sample_texture(
                      texture,
                      (
                          uv_a +
                          uv_b +
                          uv_c
                      ) / 3.0f)
                : glm::vec3(1.0f);

        glm::vec3 flat_color =
            calculate_lighting(
                face_center,
                face_normal,
                camera,
                light,
                mesh.material,
                ambient_enabled,
                diffuse_enabled,
                specular_enabled,
                flat_texture_color);

        v0.gouraud_color =
            calculate_lighting(
                world_a,
                world_vertex_normals[
                    face.a],
                camera,
                light,
                mesh.material,
                ambient_enabled,
                diffuse_enabled,
                specular_enabled,
                texture_enabled
                    ? sample_texture(
                          texture,
                          uv_a)
                    : glm::vec3(1.0f));

        v1.gouraud_color =
            calculate_lighting(
                world_b,
                world_vertex_normals[
                    face.b],
                camera,
                light,
                mesh.material,
                ambient_enabled,
                diffuse_enabled,
                specular_enabled,
                texture_enabled
                    ? sample_texture(
                          texture,
                          uv_b)
                    : glm::vec3(1.0f));

        v2.gouraud_color =
            calculate_lighting(
                world_c,
                world_vertex_normals[
                    face.c],
                camera,
                light,
                mesh.material,
                ambient_enabled,
                diffuse_enabled,
                specular_enabled,
                texture_enabled
                    ? sample_texture(
                          texture,
                          uv_c)
                    : glm::vec3(1.0f));

        rasterize_triangle(
            v0,
            v1,
            v2,
            flat_color,
            shading_mode,
            camera,
            light,
            mesh.material,
            texture,
            texture_enabled,
            ambient_enabled,
            diffuse_enabled,
            specular_enabled);

        if (wireframe_overlay) {
            uint32_t line_color =
                MFB_RGB(
                    255,
                    255,
                    255);

            draw_line(
                static_cast<int>(
                    v0.x),
                static_cast<int>(
                    v0.y),
                static_cast<int>(
                    v1.x),
                static_cast<int>(
                    v1.y),
                line_color);

            draw_line(
                static_cast<int>(
                    v1.x),
                static_cast<int>(
                    v1.y),
                static_cast<int>(
                    v2.x),
                static_cast<int>(
                    v2.y),
                line_color);

            draw_line(
                static_cast<int>(
                    v2.x),
                static_cast<int>(
                    v2.y),
                static_cast<int>(
                    v0.x),
                static_cast<int>(
                    v0.y),
                line_color);
        }

        if (debug_vectors &&
            active_mesh &&
            face_index < 3) {

            glm::vec3 light_direction =
                light.position -
                face_center;

            if (glm::length(
                    light_direction) >
                EPSILON) {

                light_direction =
                    glm::normalize(
                        light_direction);
            }

            glm::vec3 reflection_direction =
                glm::reflect(
                    -light_direction,
                    face_normal);

            draw_world_line(
                face_center,
                face_center +
                    light_direction *
                    0.65f,
                view,
                projection,
                MFB_RGB(
                    255,
                    220,
                    50));

            draw_world_line(
                face_center,
                face_center +
                    reflection_direction *
                    0.65f,
                view,
                projection,
                MFB_RGB(
                    255,
                    70,
                    255));
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

    mu_label(
        ctx,
        text);
}

void gui_slider(
    mu_Context *ctx,
    const char *label,
    float &value,
    float minimum,
    float maximum) {

    int widths[] =
        {150, -1};

    mu_layout_row(
        ctx,
        2,
        widths,
        0);

    mu_label(
        ctx,
        label);

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

    Texture texture =
        create_checker_texture();

    std::vector<Mesh> meshes;

    Mesh cube;
    cube.name = "Cube";

    cube.material.ambient =
        glm::vec3(
            0.25f,
            0.08f,
            0.08f);

    cube.material.diffuse =
        glm::vec3(
            0.90f,
            0.30f,
            0.25f);

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

    pyramid.material.ambient =
        glm::vec3(
            0.08f,
            0.20f,
            0.08f);

    pyramid.material.diffuse =
        glm::vec3(
            0.25f,
            0.85f,
            0.35f);

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

    octahedron.material.ambient =
        glm::vec3(
            0.08f,
            0.12f,
            0.25f);

    octahedron.material.diffuse =
        glm::vec3(
            0.25f,
            0.45f,
            0.95f);

    octahedron.material.shininess =
        64.0f;

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

        meshes.push_back(
            octahedron);
    }

    if (meshes.empty()) {
        std::printf(
            "No models loaded\n");

        return 1;
    }

    Camera camera;
    PointLight light;

    int active_model = 0;

    ShadingMode shading_mode =
        ShadingMode::Phong;

    int ambient_enabled = 1;
    int diffuse_enabled = 1;
    int specular_enabled = 1;

    int backface_culling = 1;
    int wireframe_overlay = 0;
    int debug_vectors = 0;
    int texture_enabled = 0;

    struct mfb_window *window =
        mfb_open_ex(
            "Assignment 5 - Lighting and Shading",
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

    bool quit_requested =
        false;

    bool rotating_model =
        false;

    bool previous_left_down =
        false;

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
            mfb_get_key_buffer(
                window);

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
            (
                ctx->mouse_down &
                MU_MOUSE_LEFT
            ) != 0;

        if (left_down &&
            !previous_left_down &&
            mouse_inside_scene) {

            rotating_model =
                true;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        if (!left_down) {
            rotating_model =
                false;
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
                camera,
                light,
                texture,
                view,
                projection,
                shading_mode,
                texture_enabled != 0,
                ambient_enabled != 0,
                diffuse_enabled != 0,
                specular_enabled != 0,
                backface_culling != 0,
                wireframe_overlay != 0,
                debug_vectors != 0,
                static_cast<int>(i) ==
                    active_model);
        }

        mu_begin(ctx);

        if (mu_begin_window(
                ctx,
                "Lighting Controls",
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
                "Assignment 5: Lighting and Shading");

            mu_text(
                ctx,
                "Left drag rotates the selected model. "
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

            gui_label(
                ctx,
                "SHADING MODE");

            int shading_widths[] =
                {115, 115, 115};

            mu_layout_row(
                ctx,
                3,
                shading_widths,
                0);

            if (mu_button(
                    ctx,
                    "Flat")) {

                shading_mode =
                    ShadingMode::Flat;
            }

            if (mu_button(
                    ctx,
                    "Gouraud")) {

                shading_mode =
                    ShadingMode::Gouraud;
            }

            if (mu_button(
                    ctx,
                    "Phong")) {

                shading_mode =
                    ShadingMode::Phong;
            }

            const char *mode_name =
                shading_mode ==
                        ShadingMode::Flat
                    ? "Current: Flat"
                    : shading_mode ==
                              ShadingMode::Gouraud
                          ? "Current: Gouraud"
                          : "Current: Phong";

            gui_label(
                ctx,
                mode_name);

            gui_label(
                ctx,
                "LIGHTING COMPONENTS");

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Ambient",
                &ambient_enabled);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Diffuse",
                &diffuse_enabled);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Specular",
                &specular_enabled);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Backface Culling",
                &backface_culling);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Wireframe Overlay",
                &wireframe_overlay);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Show Light/Reflection Vectors",
                &debug_vectors);

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_checkbox(
                ctx,
                "Enable Texture Mapping",
                &texture_enabled);

            gui_label(
                ctx,
                "POINT LIGHT POSITION");

            gui_slider(
                ctx,
                "Light X",
                light.position.x,
                -6.0f,
                6.0f);

            gui_slider(
                ctx,
                "Light Y",
                light.position.y,
                -6.0f,
                6.0f);

            gui_slider(
                ctx,
                "Light Z",
                light.position.z,
                -2.0f,
                10.0f);

            gui_label(
                ctx,
                "LIGHT INTENSITIES");

            gui_slider(
                ctx,
                "Ambient R",
                light.ambient.r,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Ambient G",
                light.ambient.g,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Ambient B",
                light.ambient.b,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Diffuse R",
                light.diffuse.r,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Diffuse G",
                light.diffuse.g,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Diffuse B",
                light.diffuse.b,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Specular R",
                light.specular.r,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Specular G",
                light.specular.g,
                0.0f,
                1.0f);

            gui_slider(
                ctx,
                "Specular B",
                light.specular.b,
                0.0f,
                1.0f);

            gui_label(
                ctx,
                "MATERIAL");

            gui_slider(
                ctx,
                "Ambient Strength",
                selected.material.ambient.r,
                0.0f,
                1.0f);

            selected.material.ambient.g =
                selected.material.ambient.r;

            selected.material.ambient.b =
                selected.material.ambient.r;

            gui_slider(
                ctx,
                "Diffuse Strength",
                selected.material.diffuse.r,
                0.0f,
                1.0f);

            selected.material.diffuse.g =
                selected.material.diffuse.r;

            selected.material.diffuse.b =
                selected.material.diffuse.r;

            gui_slider(
                ctx,
                "Shininess",
                selected.material.shininess,
                1.0f,
                128.0f);

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
                "MODEL TRANSFORM");

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
                        glm::vec3(
                            0.65f);

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

                quit_requested =
                    true;
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