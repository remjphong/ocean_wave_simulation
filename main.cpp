// BeachWorldOpenGL - diem vao va vong lap ung dung.
// main.cpp chi quan ly GLFW, camera, thoi gian, trang thai render va goi module.
// Khong dat logic tao dia hinh/song/bau troi trong file nay.

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "render_types.h"
#include "terrain.h"
#include "ocean.h"
#include "sky.h"

namespace {
    constexpr float PI_F = 3.14159265358979323846f;

    struct AppState {
        glm::vec3 camPos{ 40.0f, 5.0f, 0.0f };
        float yaw = 180.0f;   // 180 do: nhin ra bien theo huong -X
        float pitch = -3.0f;
        double lastX = 0.0;
        double lastY = 0.0;
        bool firstMouse = true;
        float dayPhase = 2.0f;
        bool timeRunning = true;
        bool wireframe = false;
    };

    AppState gApp;

    glm::vec3 cameraFront() {
        const float yawRad = glm::radians(gApp.yaw);
        const float pitchRad = glm::radians(gApp.pitch);
        return glm::normalize(glm::vec3(
            std::cos(yawRad) * std::cos(pitchRad),
            std::sin(pitchRad),
            std::sin(yawRad) * std::cos(pitchRad)));
    }

    void mouseCallback(GLFWwindow*, double x, double y) {
        if (gApp.firstMouse) {
            gApp.lastX = x;
            gApp.lastY = y;
            gApp.firstMouse = false;
        }

        const float dx = static_cast<float>(x - gApp.lastX) * 0.1f;
        const float dy = static_cast<float>(gApp.lastY - y) * 0.1f;
        gApp.lastX = x;
        gApp.lastY = y;

        gApp.yaw += dx;
        gApp.pitch = std::clamp(gApp.pitch + dy, -89.0f, 89.0f);
    }

    void keyCallback(GLFWwindow* window, int key, int, int action, int) {
        if (action != GLFW_PRESS) return;

        if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        } else if (key == GLFW_KEY_P) {
            gApp.timeRunning = !gApp.timeRunning;
        } else if (key == GLFW_KEY_F) {
            gApp.wireframe = !gApp.wireframe;
        }
    }

    void processInput(GLFWwindow* window, float dt) {
        const bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
        const float speed = 14.0f * (sprint ? 4.0f : 1.0f);

        const glm::vec3 front = cameraFront();
        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::cross(front, worldUp);
        if (glm::length(right) > 1e-5f) {
            right = glm::normalize(right);
        } else {
            right = glm::vec3(0.0f, 0.0f, -1.0f);
        }

        glm::vec3 movement(0.0f);
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement += front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= front;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) movement.y += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) movement.y -= 1.0f;

        if (glm::length(movement) > 1e-5f) {
            gApp.camPos += glm::normalize(movement) * speed * dt;
        }

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) gApp.dayPhase -= 0.8f * dt;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) gApp.dayPhase += 0.8f * dt;

        gApp.camPos.x = std::clamp(gApp.camPos.x, -280.0f, 280.0f);
        gApp.camPos.z = std::clamp(gApp.camPos.z, -280.0f, 280.0f);

        const float terrainY = Terrain::heightAt(gApp.camPos.x, gApp.camPos.z);
        const float minY = std::max(
            terrainY + BeachConfig::CAMERA_TERRAIN_CLEARANCE,
            BeachConfig::CAMERA_MIN_HEIGHT);
        gApp.camPos.y = std::max(gApp.camPos.y, minY);
    }

    FrameContext makeFrameContext(int framebufferWidth, int framebufferHeight,
                                  float timeSeconds, const Lighting& lighting) {
        const float aspect = static_cast<float>(framebufferWidth) /
                             static_cast<float>(framebufferHeight);
        const glm::mat4 projection = glm::perspective(
            glm::radians(65.0f), aspect, 0.1f, 2000.0f);
        const glm::mat4 view = glm::lookAt(
            gApp.camPos, gApp.camPos + cameraFront(), glm::vec3(0.0f, 1.0f, 0.0f));

        FrameContext frame;
        frame.vp = projection * view;
        // Bo tinh tien cua camera khi dung inverse VP de dung tia nhin cho skybox.
        frame.invSkyVP = glm::inverse(projection * glm::mat4(glm::mat3(view)));
        frame.camPos = gApp.camPos;
        frame.lighting = lighting;
        frame.time = timeSeconds;
        frame.dayPhase = gApp.dayPhase;
        frame.heightTexture = Terrain::getHeightTexture();
        frame.terrainMin = glm::vec2(BeachConfig::TERRAIN_MIN,
                                     BeachConfig::TERRAIN_MIN);
        frame.terrainSize = BeachConfig::TERRAIN_SIZE;
        frame.heightScaleBias = glm::vec2(
            (BeachConfig::TERRAIN_RESOLUTION - 1.0f) /
                static_cast<float>(BeachConfig::TERRAIN_RESOLUTION),
            0.5f / static_cast<float>(BeachConfig::TERRAIN_RESOLUTION));
        return frame;
    }

    void setPolygonMode(bool wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }

    void renderFrame(const FrameContext& frame) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // 1. Terrain opaque: ve truoc, co ghi depth.
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        setPolygonMode(gApp.wireframe);
        Terrain::render(frame);

        // 2. Sky fullscreen: chi ve vao pixel nen con trong (depth = 1).
        setPolygonMode(false);
        glDisable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        Sky::render(frame);

        // 3. Ocean trong suot: ve cuoi cung, dung alpha blending.
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        setPolygonMode(gApp.wireframe);
        Ocean::render(frame);

        // Khoi phuc state mac dinh cho frame tiep theo va tranh ro ri state.
        setPolygonMode(false);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
    }

    void updateWindowTitle(GLFWwindow* window, float elapsed, int& frames,
                           double& titleTimer) {
        titleTimer += elapsed;
        ++frames;
        if (titleTimer < 0.5) return;

        const float hour = std::fmod(
            12.0f + (gApp.dayPhase - PI_F * 0.5f) * 24.0f / (2.0f * PI_F) + 24.0f,
            24.0f);
        char title[160];
        std::snprintf(title, sizeof(title),
            "Beach 3D | FPS: %d | Time: %02d:%02d %s",
            static_cast<int>(frames / titleTimer),
            static_cast<int>(hour),
            static_cast<int>((hour - std::floor(hour)) * 60.0f),
            gApp.timeRunning ? "" : "(PAUSED)");
        glfwSetWindowTitle(window, title);
        frames = 0;
        titleTimer = 0.0;
    }
}

int main() {
    glfwSetErrorCallback([](int, const char* description) {
        std::fprintf(stderr, "[GLFW] %s\n", description ? description : "unknown error");
    });

    if (!glfwInit()) {
        std::fprintf(stderr, "Khong khoi tao duoc GLFW.\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Beach 3D", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Khong tao duoc cua so OpenGL.\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "Khong khoi tao duoc GLEW.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    // GLEW co the tao GL_INVALID_ENUM khi khoi tao context core; xoa loi nay.
    while (glGetError() != GL_NO_ERROR) {}

    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);

    // Khoi tao theo thu tu phu thuoc: Terrain tao height texture truoc.
    if (!Terrain::init()) {
        std::fprintf(stderr, "Terrain::init() that bai.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    if (!Ocean::init()) {
        std::fprintf(stderr, "Ocean::init() that bai.\n");
        Terrain::destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    if (!Sky::init()) {
        std::fprintf(stderr, "Sky::init() that bai.\n");
        Ocean::destroy();
        Terrain::destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    gApp.camPos.y = std::max(
        Terrain::heightAt(gApp.camPos.x, gApp.camPos.z) + 2.0f,
        BeachConfig::CAMERA_MIN_HEIGHT);

    const float daySpeed = 2.0f * PI_F / BeachConfig::DAY_CYCLE_SECONDS;
    double previousTime = glfwGetTime();
    double titleTimer = 0.0;
    int frames = 0;

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::min(now - previousTime, 0.1));
        previousTime = now;
        const float timeSeconds = static_cast<float>(now);

        glfwPollEvents();
        processInput(window, dt);

        if (gApp.timeRunning) gApp.dayPhase += daySpeed * dt;
        gApp.dayPhase = std::fmod(gApp.dayPhase + 2.0f * PI_F, 2.0f * PI_F);

        const Lighting lighting = Sky::computeLighting(gApp.dayPhase);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            glfwPollEvents();
            continue;
        }
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        const FrameContext frame = makeFrameContext(
            framebufferWidth, framebufferHeight, timeSeconds, lighting);
        renderFrame(frame);
        glfwSwapBuffers(window);
        updateWindowTitle(window, dt, frames, titleTimer);
    }

    // Giai phong tai nguyen module khi OpenGL context van con hien hanh.
    Sky::destroy();
    Ocean::destroy();
    Terrain::destroy();

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
