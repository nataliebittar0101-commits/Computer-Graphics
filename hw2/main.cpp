 #include "MiniFB.h"

#include <algorithm>
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
#include <glm/gtc/matrix_transform.hpp>

extern "C" {
#include "microui.h"
}

#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200

constexpr int UI_WIDTH = 440;
constexpr float SCREEN_MODEL_SCALE = 140.0f;

// ============================================================
// Data structures
// ============================================================

static uint32_t g_buffer[WIDTH * HEIGHT];
struct Face {
    int a = 0;
    int b = 0;
    int c = 0;
};

struct TransformState {
    glm::vec3 local_translation{0.0f, 0.0f, 0.0f};
    glm::vec3 local_rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 local_scale{1.0f, 1.0f, 1.0f};

    glm::vec3 world_translation{0.0f, 0.0f, 0.0f};
    glm::vec3 world_rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 world_scale{1.0f, 1.0f, 1.0f};
};

struct Mesh {
    std::string name;
    std::string filename;

    std::vector<glm::vec3> vertices;
    std::vector<Face> faces;

    glm::vec3 minimum{0.0f};
    glm::vec3 maximum{0.0f};
    glm::vec3 center{0.0f};

    float normalization_scale = 1.0f;

    TransformState transform;
    uint32_t color = MFB_RGB(255, 255, 255);
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
                    12.0f + horizontal * 18.0f);

            uint8_t green =
                static_cast<uint8_t>(
                    18.0f + vertical * 25.0f);

            uint8_t blue =
                static_cast<uint8_t>(
                    35.0f +
                    horizontal * 35.0f +
                    vertical * 15.0f);

            g_buffer[y * WIDTH + x] =
                MFB_RGB(red, green, blue);
        }
    }

    // Vertical separator between GUI and scene.
    for (int y = 0; y < HEIGHT; y++) {
        put_pixel(UI_WIDTH, y, MFB_RGB(90, 100, 125));
        put_pixel(UI_WIDTH + 1, y, MFB_RGB(90, 100, 125));
    }
}

// ============================================================
// OBJ files used by the assignment
// ============================================================

void create_default_obj_files() {
    std::filesystem::create_directories("models");

    const std::string cube_path = "models/cube.obj";
    const std::string pyramid_path = "models/pyramid.obj";
    const std::string octahedron_path = "models/octahedron.obj";

    if (!std::filesystem::exists(cube_path)) {
        std::ofstream file(cube_path);

        file <<
            "# Cube with 8 vertices and 12 triangular faces\n"
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

    if (!std::filesystem::exists(pyramid_path)) {
        std::ofstream file(pyramid_path);

        file <<
            "# Pyramid with 5 vertices and 6 triangular faces\n"
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

    if (!std::filesystem::exists(octahedron_path)) {
        std::ofstream file(octahedron_path);

        file <<
            "# Octahedron with 6 vertices and 8 triangular faces\n"
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

    std::string index_text = token;

    std::size_t slash_position =
        index_text.find('/');

    if (slash_position != std::string::npos) {
        index_text =
            index_text.substr(0, slash_position);
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

    glm::vec3 size =
        mesh.maximum - mesh.minimum;

    float largest_dimension =
        std::max({
            size.x,
            size.y,
            size.z
        });

    if (largest_dimension <= 0.0001f) {
        largest_dimension = 1.0f;
    }

    // Maps the largest dimension approximately to [-1, 1].
    mesh.normalization_scale =
        2.0f / largest_dimension;
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
            stream >> vertex.x >> vertex.y >> vertex.z;

            if (!stream.fail()) {
                mesh.vertices.push_back(vertex);
            }
        }

        if (type == "f") {
            std::vector<int> polygon_indices;
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

                    polygon_indices.push_back(index);
                }
            }

            // Triangle fan: also supports polygonal OBJ faces.
            for (std::size_t i = 1;
                 i + 1 < polygon_indices.size();
                 i++) {

                mesh.faces.push_back({
                    polygon_indices[0],
                    polygon_indices[i],
                    polygon_indices[i + 1]
                });
            }
        }
    }

    calculate_bounding_box(mesh);

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

    matrix =
        glm::rotate(
            matrix,
            glm::radians(rotation_degrees.x),
            glm::vec3(1.0f, 0.0f, 0.0f));

    matrix =
        glm::rotate(
            matrix,
            glm::radians(rotation_degrees.y),
            glm::vec3(0.0f, 1.0f, 0.0f));

    matrix =
        glm::rotate(
            matrix,
            glm::radians(rotation_degrees.z),
            glm::vec3(0.0f, 0.0f, 1.0f));

    return matrix;
}

glm::mat4 create_model_matrix(const Mesh &mesh) {
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

    glm::mat4 local_matrix =
        local_translation *
        local_rotation *
        local_scale;

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

    glm::mat4 world_matrix =
        world_translation *
        world_rotation *
        world_scale;

    /*
     * Matrix multiplication is applied from right to left:
     *
     * 1. Normalize and center original OBJ coordinates.
     * 2. Apply local transformations.
     * 3. Apply world transformations.
     *
     * This ordering lets a world rotation also rotate the
     * object's local translation, producing an orbit effect.
     */
    return world_matrix *
           local_matrix *
           normalization;
}

// ============================================================
// Projection and rendering
// ============================================================

glm::ivec2 orthographic_project(
    const glm::vec4 &point) {

    int viewport_width =
        WIDTH - UI_WIDTH;

    int center_x =
        UI_WIDTH + viewport_width / 2;

    int center_y =
        HEIGHT / 2;

    int screen_x =
        center_x +
        static_cast<int>(
            point.x * SCREEN_MODEL_SCALE);

    int screen_y =
        center_y -
        static_cast<int>(
            point.y * SCREEN_MODEL_SCALE);

    return {screen_x, screen_y};
}

void render_mesh(const Mesh &mesh) {
    if (mesh.vertices.empty() ||
        mesh.faces.empty()) {

        return;
    }

    glm::mat4 model_matrix =
        create_model_matrix(mesh);

    std::vector<glm::ivec2> projected_vertices;
    projected_vertices.reserve(
        mesh.vertices.size());

    for (const glm::vec3 &vertex :
         mesh.vertices) {

        glm::vec4 transformed =
            model_matrix *
            glm::vec4(vertex, 1.0f);

        projected_vertices.push_back(
            orthographic_project(transformed));
    }

    for (const Face &face : mesh.faces) {
        if (face.a < 0 ||
            face.b < 0 ||
            face.c < 0 ||
            face.a >=
                static_cast<int>(
                    projected_vertices.size()) ||
            face.b >=
                static_cast<int>(
                    projected_vertices.size()) ||
            face.c >=
                static_cast<int>(
                    projected_vertices.size())) {

            continue;
        }

        const glm::ivec2 &a =
            projected_vertices[face.a];

        const glm::ivec2 &b =
            projected_vertices[face.b];

        const glm::ivec2 &c =
            projected_vertices[face.c];

        draw_line_bresenham(
            a.x,
            a.y,
            b.x,
            b.y,
            mesh.color);

        draw_line_bresenham(
            b.x,
            b.y,
            c.x,
            c.y,
            mesh.color);

        draw_line_bresenham(
            c.x,
            c.y,
            a.x,
            a.y,
            mesh.color);
    }
}

void reset_transform(TransformState &transform) {
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

// ============================================================
// GUI helpers
// ============================================================

void gui_label(mu_Context *ctx, const char *text) {
    int width[] = {-1};
    mu_layout_row(ctx, 1, width, 0);
    mu_label(ctx, text);
}

void gui_slider(
    mu_Context *ctx,
    const char *label,
    float &value,
    float minimum,
    float maximum) {

    int row_widths[] = {145, -1};
    mu_layout_row(ctx, 2, row_widths, 0);

    mu_label(ctx, label);
    mu_slider(ctx, &value, minimum, maximum);
}

void draw_transform_controls(
    mu_Context *ctx,
    TransformState &transform) {

    gui_label(ctx, "LOCAL TRANSFORMATIONS");

    gui_slider(
        ctx,
        "Local Translate X",
        transform.local_translation.x,
        -2.0f,
        2.0f);

    gui_slider(
        ctx,
        "Local Translate Y",
        transform.local_translation.y,
        -2.0f,
        2.0f);

    gui_slider(
        ctx,
        "Local Translate Z",
        transform.local_translation.z,
        -2.0f,
        2.0f);

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

    gui_slider(
        ctx,
        "Local Scale X",
        transform.local_scale.x,
        0.1f,
        2.5f);

    gui_slider(
        ctx,
        "Local Scale Y",
        transform.local_scale.y,
        0.1f,
        2.5f);

    gui_slider(
        ctx,
        "Local Scale Z",
        transform.local_scale.z,
        0.1f,
        2.5f);

    gui_label(ctx, "WORLD TRANSFORMATIONS");

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

    gui_slider(
        ctx,
        "World Scale X",
        transform.world_scale.x,
        0.1f,
        2.5f);

    gui_slider(
        ctx,
        "World Scale Y",
        transform.world_scale.y,
        0.1f,
        2.5f);

    gui_slider(
        ctx,
        "World Scale Z",
        transform.world_scale.z,
        0.1f,
        2.5f);
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

    if (load_obj("models/cube.obj", cube)) {
        cube.transform.world_translation.x = -1.25f;
        cube.transform.local_rotation =
            glm::vec3(20.0f, 30.0f, 0.0f);

        meshes.push_back(cube);
    }

    Mesh pyramid;
    pyramid.name = "Pyramid";
    pyramid.color = MFB_RGB(120, 255, 160);

    if (load_obj("models/pyramid.obj", pyramid)) {
        pyramid.transform.world_translation.x = 1.25f;
        pyramid.transform.local_rotation =
            glm::vec3(15.0f, -25.0f, 0.0f);

        meshes.push_back(pyramid);
    }

    Mesh octahedron;
    octahedron.name = "Octahedron";
    octahedron.color = MFB_RGB(120, 180, 255);

    if (load_obj(
            "models/octahedron.obj",
            octahedron)) {

        octahedron.transform.world_translation.y =
            1.25f;

        octahedron.transform.local_scale =
            glm::vec3(0.65f);

        meshes.push_back(octahedron);
    }

    if (meshes.empty()) {
        std::printf(
            "No OBJ models were loaded.\n");

        return 1;
    }

    int active_model = 0;

    struct mfb_window *window =
        mfb_open_ex(
            "Assignment 2 - Wireframe Viewer",
            WIDTH,
            HEIGHT,
            MFB_WF_RESIZABLE);

    if (!window) {
        return 1;
    }

    mu_Context *ctx =
        static_cast<mu_Context *>(
            std::malloc(sizeof(mu_Context)));

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

    UIRenderer renderer(WIDTH, HEIGHT);

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

    bool rotating_with_mouse = false;
    bool translating_with_mouse = false;

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

        // ----------------------------------------------------
        // Keyboard control: world translation
        // ----------------------------------------------------

        constexpr float keyboard_speed = 0.025f;

        if (keys[MFB_KB_KEY_LEFT]) {
            active_mesh.transform
                .world_translation.x -=
                keyboard_speed;
        }

        if (keys[MFB_KB_KEY_RIGHT]) {
            active_mesh.transform
                .world_translation.x +=
                keyboard_speed;
        }

        if (keys[MFB_KB_KEY_UP]) {
            active_mesh.transform
                .world_translation.y +=
                keyboard_speed;
        }

        if (keys[MFB_KB_KEY_DOWN]) {
            active_mesh.transform
                .world_translation.y -=
                keyboard_speed;
        }

        // ----------------------------------------------------
        // Mouse approach 1: left drag rotates active model
        // ----------------------------------------------------

        if (left_down &&
            !previous_left_down &&
            mouse_inside_scene) {

            rotating_with_mouse = true;
            previous_mouse_x = ctx->mouse_pos.x;
            previous_mouse_y = ctx->mouse_pos.y;
        }

        if (!left_down) {
            rotating_with_mouse = false;
        }

        if (rotating_with_mouse &&
            left_down) {

            int delta_x =
                ctx->mouse_pos.x -
                previous_mouse_x;

            int delta_y =
                ctx->mouse_pos.y -
                previous_mouse_y;

            active_mesh.transform
                .local_rotation.y +=
                static_cast<float>(delta_x) *
                0.6f;

            active_mesh.transform
                .local_rotation.x +=
                static_cast<float>(delta_y) *
                0.6f;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        // ----------------------------------------------------
        // Mouse approach 2: right drag translates active model
        // ----------------------------------------------------

        if (right_down &&
            !previous_right_down &&
            mouse_inside_scene) {

            translating_with_mouse = true;
            previous_mouse_x = ctx->mouse_pos.x;
            previous_mouse_y = ctx->mouse_pos.y;
        }

        if (!right_down) {
            translating_with_mouse = false;
        }

        if (translating_with_mouse &&
            right_down) {

            int delta_x =
                ctx->mouse_pos.x -
                previous_mouse_x;

            int delta_y =
                ctx->mouse_pos.y -
                previous_mouse_y;

            active_mesh.transform
                .world_translation.x +=
                static_cast<float>(delta_x) /
                SCREEN_MODEL_SCALE;

            active_mesh.transform
                .world_translation.y -=
                static_cast<float>(delta_y) /
                SCREEN_MODEL_SCALE;

            previous_mouse_x =
                ctx->mouse_pos.x;

            previous_mouse_y =
                ctx->mouse_pos.y;
        }

        previous_left_down = left_down;
        previous_right_down = right_down;

        // ----------------------------------------------------
        // Scene rendering
        // ----------------------------------------------------

        clear_background();

        for (const Mesh &mesh : meshes) {
            render_mesh(mesh);
        }

        // ----------------------------------------------------
        // GUI
        // ----------------------------------------------------

        mu_begin(ctx);

        if (mu_begin_window(
                ctx,
                "Wireframe Viewer Controls",
                mu_rect(
                    15,
                    15,
                    410,
                    1160))) {

            int full_width[] = {-1};

            mu_layout_row(
                ctx,
                1,
                full_width,
                0);

            mu_label(
                ctx,
                "Assignment 2: 3D Wireframe Viewer");

            mu_text(
                ctx,
                "Left drag: rotate active model. "
                "Right drag: translate active model. "
                "Arrow keys: world translation.");

            gui_label(ctx, "ACTIVE MODEL");

            int model_button_widths[] =
                {115, 115, 115};

            mu_layout_row(
                ctx,
                3,
                model_button_widths,
                0);

            if (mu_button(ctx, "Cube")) {
                active_model = 0;
            }

            if (meshes.size() > 1) {
                if (mu_button(ctx, "Pyramid")) {
                    active_model = 1;
                }
            } else {
                mu_label(ctx, "Unavailable");
            }

            if (meshes.size() > 2) {
                if (mu_button(ctx, "Octahedron")) {
                    active_model = 2;
                }
            } else {
                mu_label(ctx, "Unavailable");
            }

            Mesh &selected =
                meshes[active_model];

            char active_text[128];

            std::snprintf(
                active_text,
                sizeof(active_text),
                "Selected: %s",
                selected.name.c_str());

            gui_label(ctx, active_text);

            char vertex_text[128];

            std::snprintf(
                vertex_text,
                sizeof(vertex_text),
                "Vertices: %zu",
                selected.vertices.size());

            gui_label(ctx, vertex_text);

            char face_text[128];

            std::snprintf(
                face_text,
                sizeof(face_text),
                "Faces: %zu",
                selected.faces.size());

            gui_label(ctx, face_text);

            char minimum_text[160];

            std::snprintf(
                minimum_text,
                sizeof(minimum_text),
                "BBox min: %.2f, %.2f, %.2f",
                selected.minimum.x,
                selected.minimum.y,
                selected.minimum.z);

            gui_label(ctx, minimum_text);

            char maximum_text[160];

            std::snprintf(
                maximum_text,
                sizeof(maximum_text),
                "BBox max: %.2f, %.2f, %.2f",
                selected.maximum.x,
                selected.maximum.y,
                selected.maximum.z);

            gui_label(ctx, maximum_text);

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
                    "Reset Active Model Transform")) {

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
                    "Arrange Models")) {

                reset_transform(
                    meshes[0].transform);

                meshes[0].transform
                    .world_translation.x =
                    -1.25f;

                if (meshes.size() > 1) {
                    reset_transform(
                        meshes[1].transform);

                    meshes[1].transform
                        .world_translation.x =
                        1.25f;
                }

                if (meshes.size() > 2) {
                    reset_transform(
                        meshes[2].transform);

                    meshes[2].transform
                        .world_translation.y =
                        1.25f;

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
