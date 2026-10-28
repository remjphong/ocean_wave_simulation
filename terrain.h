#pragma once

// Module Terrain quan ly toan bo dia hinh bai cat va height texture.
// Terrain so huu VAO/VBO/EBO, shader va height texture; module khac chi duoc doc texture.

#include "render_types.h"

namespace Terrain {
    // Goi sau khi da tao OpenGL context va khoi tao GLEW.
    // Tra ve false neu shader hoac tai nguyen OpenGL khong the khoi tao.
    bool init();

    // Ve bai cat trong frame hien tai. Main quan ly depth/blend/polygon mode.
    void render(const FrameContext& frame);

    // Giai phong tat ca tai nguyen do Terrain so huu.
    // Phai goi truoc khi huy OpenGL context.
    void destroy();

    // Lay do cao dia hinh tai toa do world (x, z); dung cho camera/gameplay.
    // Ham nay dung cung ham tao heightmap nen nhat quan voi mesh.
    float heightAt(float x, float z);

    // Tra ve height texture do Terrain so huu. Module khac chi duoc bind/doc,
    // khong duoc sua noi dung hoac goi glDeleteTextures tren texture nay.
    GLuint getHeightTexture();
}
