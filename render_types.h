#pragma once

// Cac kieu du lieu va hang so duoc ca ba module su dung.
// Day la hop dong chung: khong tu y doi ten truong neu chua thong nhat ca nhom.

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace BeachConfig {
    inline constexpr int TERRAIN_RESOLUTION = 601;
    inline constexpr float TERRAIN_MIN = -300.0f;
    inline constexpr float TERRAIN_SIZE = 600.0f;

    inline constexpr float OCEAN_X_MIN = -330.0f;
    inline constexpr float OCEAN_X_MAX = 60.0f;
    inline constexpr float OCEAN_Z_MIN = -300.0f;
    inline constexpr float OCEAN_Z_MAX = 300.0f;
    inline constexpr float OCEAN_STEP = 0.7f;

    inline constexpr float DAY_CYCLE_SECONDS = 240.0f;
    inline constexpr float CAMERA_MIN_HEIGHT = 2.2f;
    inline constexpr float CAMERA_TERRAIN_CLEARANCE = 1.2f;
}

// Ket qua tinh toan anh sang cua Sky, duoc Terrain va Ocean dung chung.
struct Lighting {
    glm::vec3 sunDir{ 1.0f, 0.0f, 0.0f };       // Huong tu tam canh ve phia mat troi
    glm::vec3 lightDir{ 1.0f, 0.0f, 0.0f };     // Huong tia sang chieu toi be mat
    glm::vec3 lightColor{ 1.0f, 1.0f, 1.0f };
    glm::vec3 ambient{ 0.1f, 0.1f, 0.1f };
    glm::vec3 sunTransmit{ 1.0f, 1.0f, 1.0f };
    float exposure = 1.0f;
};

// Du lieu bat bien trong mot frame. Main tao mot ban moi moi frame va truyen
// const reference vao tung module; module khong tu y thay doi du lieu nay.
struct FrameContext {
    glm::mat4 vp{ 1.0f };
    glm::mat4 invSkyVP{ 1.0f };
    glm::vec3 camPos{ 0.0f, 0.0f, 0.0f };
    Lighting lighting{};

    float time = 0.0f;       // Thoi gian lien tuc, giay
    float dayPhase = 0.0f;   // Goc chu ky ngay dem, radian

    // Terrain so huu texture nay; Ocean chi doc, khong duoc xoa texture.
    GLuint heightTexture = 0;
    glm::vec2 terrainMin{ BeachConfig::TERRAIN_MIN, BeachConfig::TERRAIN_MIN };
    float terrainSize = BeachConfig::TERRAIN_SIZE;

    // Khop voi cach can chinh texel khi lay mau height texture:
    // scale = (N - 1) / N, bias = 0.5 / N.
    glm::vec2 heightScaleBias{
        (BeachConfig::TERRAIN_RESOLUTION - 1.0f) / BeachConfig::TERRAIN_RESOLUTION,
        0.5f / BeachConfig::TERRAIN_RESOLUTION
    };
};
