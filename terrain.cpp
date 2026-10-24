// Terrain module: Perlin/fBm heightmap, mesh, height texture va shader cat.

#include "terrain.h"
#include "shader_common.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace Terrain {
namespace {
    constexpr int kGridResolution = BeachConfig::TERRAIN_RESOLUTION;
    constexpr float kTerrainMin = BeachConfig::TERRAIN_MIN;
    constexpr float kTerrainSize = BeachConfig::TERRAIN_SIZE;
    constexpr float kGridStep = kTerrainSize / static_cast<float>(kGridResolution - 1);

    struct Mesh {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
    };

    Mesh gMesh;
    GLuint gProgram = 0;
    GLuint gHeightTexture = 0;

    float clampf(float x, float low, float high) {
        return std::max(low, std::min(x, high));
    }

    float lerpf(float a, float b, float t) {
        return a + (b - a) * t;
    }

    float smoothStep(float edge0, float edge1, float x) {
        const float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Improved 2D Perlin noise voi bang hoan vi co seed co dinh.
    class Perlin {
    public:
        explicit Perlin(unsigned int seed) {
            std::vector<int> permutation(256);
            std::iota(permutation.begin(), permutation.end(), 0);
            std::mt19937 rng(seed);
            std::shuffle(permutation.begin(), permutation.end(), rng);
            for (int i = 0; i < 512; ++i) {
                p_[i] = permutation[static_cast<std::size_t>(i) & 255U];
            }
        }

        float noise(float x, float y) const {
            const float floorX = std::floor(x);
            const float floorY = std::floor(y);
            const int X = static_cast<int>(floorX) & 255;
            const int Y = static_cast<int>(floorY) & 255;
            x -= floorX;
            y -= floorY;

            const float u = fade(x);
            const float v = fade(y);
            const int aa = p_[p_[X] + Y];
            const int ab = p_[p_[X] + Y + 1];
            const int ba = p_[p_[X + 1] + Y];
            const int bb = p_[p_[X + 1] + Y + 1];

            const float lower = lerpf(grad(aa, x, y), grad(ba, x - 1.0f, y), u);
            const float upper = lerpf(grad(ab, x, y - 1.0f),
                                      grad(bb, x - 1.0f, y - 1.0f), u);
            return lerpf(lower, upper, v);
        }

        // Fractal Brownian Motion: cong cac octave va chuan hoa bien do.
        float fbm(float x, float y, int octaves) const {
            float sum = 0.0f;
            float amplitude = 1.0f;
            float frequency = 1.0f;
            float normalization = 0.0f;

            for (int octave = 0; octave < octaves; ++octave) {
                sum += amplitude * noise(x * frequency, y * frequency);
                normalization += amplitude;
                amplitude *= 0.5f;
                frequency *= 2.03f;
            }
            return normalization > 0.0f ? sum / normalization : 0.0f;
        }

    private:
        int p_[512]{};

        static float fade(float t) {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        static float grad(int hash, float x, float y) {
            switch (hash & 7) {
            case 0: return  x + y;
            case 1: return -x + y;
            case 2: return  x - y;
            case 3: return -x - y;
            case 4: return  x;
            case 5: return -x;
            case 6: return  y;
            default: return -y;
            }
        }
    };

    const Perlin gNoise(20240601U);

    // Duong bo uon theo truc Z; bien la phia -X, dat lien phia +X.
    float calculateHeight(float x, float z) {
        const float bend = 9.0f * gNoise.noise(z * 0.011f, 3.7f);
        const float xs = x + bend;

        float h = (xs - 6.0f) * 0.07f;
        if (xs > 60.0f) {
            h = 3.78f + (xs - 60.0f) * 0.01f;
        }

        float t = (xs + 28.0f) / 7.0f;
        h += 1.10f * std::exp(-t * t); // doi cat ngam / sandbar

        t = (xs - 24.0f) / 7.0f;
        h += 0.55f * std::exp(-t * t); // berm: go cat gan bai kho

        h += 0.35f * gNoise.fbm(x * 0.04f + 11.0f,
                                z * 0.04f + 3.0f, 3);

        const float dune = smoothStep(40.0f, 100.0f, xs);
        h += dune * (5.5f + 7.0f * gNoise.fbm(
            x * 0.013f + 50.0f, z * 0.017f + 9.0f, 4));

        h += 0.04f * gNoise.noise(x * 0.5f, z * 0.18f);
        return h;
    }

    GLuint compileShader(GLenum shaderType, const std::string& source,
                         const char* label) {
        const GLuint shader = glCreateShader(shaderType);
        if (shader == 0) {
            std::fprintf(stderr, "[Terrain] Khong tao duoc shader %s.\n", label);
            return 0;
        }

        const char* sourcePointer = source.c_str();
        glShaderSource(shader, 1, &sourcePointer, nullptr);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled != GL_TRUE) {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
            glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
            std::fprintf(stderr, "[Terrain] Loi compile shader %s:\n%s\n",
                         label, log.data());
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint createProgram(const std::string& vertexSource,
                         const std::string& fragmentSource) {
        const GLuint vertexShader = compileShader(
            GL_VERTEX_SHADER, vertexSource, "terrain vertex");
        if (vertexShader == 0) return 0;

        const GLuint fragmentShader = compileShader(
            GL_FRAGMENT_SHADER, fragmentSource, "terrain fragment");
        if (fragmentShader == 0) {
            glDeleteShader(vertexShader);
            return 0;
        }

        const GLuint program = glCreateProgram();
        if (program == 0) {
            std::fprintf(stderr, "[Terrain] Khong tao duoc shader program.\n");
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return 0;
        }

        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
            glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
            std::fprintf(stderr, "[Terrain] Loi link shader program:\n%s\n", log.data());
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }

    void buildGridIndices(std::vector<unsigned int>& indices) {
        const std::size_t cells = static_cast<std::size_t>(kGridResolution - 1) *
                                  static_cast<std::size_t>(kGridResolution - 1);
        indices.reserve(cells * 6U);

        for (int z = 0; z < kGridResolution - 1; ++z) {
            for (int x = 0; x < kGridResolution - 1; ++x) {
                const unsigned int a = static_cast<unsigned int>(z * kGridResolution + x);
                const unsigned int b = a + 1U;
                const unsigned int c = a + static_cast<unsigned int>(kGridResolution);
                const unsigned int d = c + 1U;

                // Thu tu dinh tao normal huong len tren (Y+).
                indices.push_back(a);
                indices.push_back(c);
                indices.push_back(b);
                indices.push_back(b);
                indices.push_back(c);
                indices.push_back(d);
            }
        }
    }

    bool buildTerrainMesh(std::vector<float>& heights) {
        const std::size_t vertexCount = static_cast<std::size_t>(kGridResolution) *
                                        static_cast<std::size_t>(kGridResolution);
        heights.resize(vertexCount);

        for (int z = 0; z < kGridResolution; ++z) {
            for (int x = 0; x < kGridResolution; ++x) {
                const float worldX = kTerrainMin + static_cast<float>(x) * kGridStep;
                const float worldZ = kTerrainMin + static_cast<float>(z) * kGridStep;
                heights[static_cast<std::size_t>(z) * kGridResolution + x] =
                    calculateHeight(worldX, worldZ);
            }
        }

        const auto heightAtGrid = [&heights](int x, int z) -> float {
            x = std::max(0, std::min(kGridResolution - 1, x));
            z = std::max(0, std::min(kGridResolution - 1, z));
            return heights[static_cast<std::size_t>(z) * kGridResolution + x];
        };

        // Moi vertex: position xyz + normal xyz.
        std::vector<float> vertices;
        vertices.reserve(vertexCount * 6U);
        for (int z = 0; z < kGridResolution; ++z) {
            for (int x = 0; x < kGridResolution; ++x) {
                const float worldX = kTerrainMin + static_cast<float>(x) * kGridStep;
                const float worldZ = kTerrainMin + static_cast<float>(z) * kGridStep;
                const float height = heightAtGrid(x, z);

                glm::vec3 normal(
                    heightAtGrid(x - 1, z) - heightAtGrid(x + 1, z),
                    2.0f * kGridStep,
                    heightAtGrid(x, z - 1) - heightAtGrid(x, z + 1));
                const float normalLength = glm::length(normal);
                normal = normalLength > 1e-6f
                    ? normal / normalLength
                    : glm::vec3(0.0f, 1.0f, 0.0f);

                vertices.push_back(worldX);
                vertices.push_back(height);
                vertices.push_back(worldZ);
                vertices.push_back(normal.x);
                vertices.push_back(normal.y);
                vertices.push_back(normal.z);
            }
        }

        std::vector<unsigned int> indices;
        buildGridIndices(indices);

        glGenVertexArrays(1, &gMesh.vao);
        glGenBuffers(1, &gMesh.vbo);
        glGenBuffers(1, &gMesh.ebo);
        if (gMesh.vao == 0 || gMesh.vbo == 0 || gMesh.ebo == 0) {
            std::fprintf(stderr, "[Terrain] Khong cap phat duoc VAO/VBO/EBO.\n");
            return false;
        }

        glBindVertexArray(gMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gMesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gMesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                     indices.data(), GL_STATIC_DRAW);

        constexpr GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(3 * sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        gMesh.indexCount = static_cast<GLsizei>(indices.size());
        return true;
    }

    bool createHeightTexture(const std::vector<float>& heights) {
        glGenTextures(1, &gHeightTexture);
        if (gHeightTexture == 0) {
            std::fprintf(stderr, "[Terrain] Khong tao duoc height texture.\n");
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, gHeightTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F,
                     kGridResolution, kGridResolution, 0,
                     GL_RED, GL_FLOAT, heights.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::fprintf(stderr, "[Terrain] Loi OpenGL khi tao height texture: 0x%X\n",
                         static_cast<unsigned int>(error));
            glDeleteTextures(1, &gHeightTexture);
            gHeightTexture = 0;
            return false;
        }
        return true;
    }

    void setFloat(GLuint program, const char* name, float value) {
        glUniform1f(glGetUniformLocation(program, name), value);
    }

    void setInt(GLuint program, const char* name, int value) {
        glUniform1i(glGetUniformLocation(program, name), value);
    }

    void setVec2(GLuint program, const char* name, const glm::vec2& value) {
        glUniform2fv(glGetUniformLocation(program, name), 1, &value[0]);
    }

    void setVec3(GLuint program, const char* name, const glm::vec3& value) {
        glUniform3fv(glGetUniformLocation(program, name), 1, &value[0]);
    }

    void setMat4(GLuint program, const char* name, const glm::mat4& value) {
        glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, &value[0][0]);
    }

    const char* kTerrainVertexShader = R"GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uVP;

out vec3 vPos;
out vec3 vNormal;

void main() {
    vPos = aPos;
    vNormal = aNormal;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)GLSL";

    const char* kTerrainFragmentShader = R"GLSL(
in vec3 vPos;
in vec3 vNormal;
out vec4 FragColor;

uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;

float softShadow(vec3 p, vec3 L) {
    if (L.y <= 0.0) return 0.0;

    float result = 1.0;
    float t = 0.4;
    for (int i = 0; i < 28; ++i) {
        vec3 q = p + L * t;
        if (q.y > 45.0) break;

        // Khong march vuot ra ngoai mien heightmap.
        if (q.x < uTMin.x || q.z < uTMin.y ||
            q.x > uTMin.x + uTSize || q.z > uTMin.y + uTSize) {
            break;
        }

        float delta = q.y - terrH(q.xz);
        result = min(result, 10.0 * delta / t);
        if (result < 0.001) break;
        t += 0.5 + t * 0.18;
    }
    return clamp(result, 0.0, 1.0);
}

void main() {
    vec3 p = vPos;
    vec3 N = normalize(vNormal);

    // Doi mau cat am theo duong song len/xuong.
    float runup = 0.55
        + 0.25 * sin(uTime * 0.8 + p.z * 0.07)
        + 0.12 * sin(uTime * 1.7 - p.z * 0.13);
    float wet = 1.0 - smoothstep(runup - 0.15, runup + 0.45, p.y);

    vec3 drySand = vec3(0.74, 0.55, 0.31);
    vec3 wetSand = vec3(0.20, 0.14, 0.08);
    float grain = 0.5 * vnoise(p.xz * 8.0) + 0.5 * vnoise(p.xz * 35.0);
    float largePattern = vnoise(p.xz * 0.05);
    drySand = mix(drySand, drySand * vec3(0.92, 0.85, 0.75), largePattern);
    drySand *= 0.88 + 0.24 * grain;
    vec3 albedo = mix(drySand, wetSand, wet);

    // Gion cat nho chi anh huong shading, khong lam thay doi hinh hoc mesh.
    vec2 q = p.xz * 2.5;
    vec2 grainGradient = vec2(
        vnoise(q + vec2(0.15, 0.0)) - vnoise(q - vec2(0.15, 0.0)),
        vnoise(q + vec2(0.0, 0.15)) - vnoise(q - vec2(0.0, 0.15)));
    N = normalize(N + vec3(-grainGradient.x, 0.0, -grainGradient.y)
                     * 1.2 * (1.0 - 0.7 * wet));

    vec3 L = normalize(uLightDir);
    float shadow = softShadow(p + N * 0.15, L);
    float ndl = max(dot(N, L), 0.0);
    vec3 color = albedo * (uLightColor * ndl * shadow
                         + uAmbient * (0.55 + 0.45 * N.y));

    vec3 V = normalize(uCamPos - p);
    vec3 H = normalize(L + V);
    float specularPower = mix(24.0, 160.0, wet);
    float specularStrength = mix(0.04, 0.50, wet);
    float specular = pow(max(dot(N, H), 0.0), specularPower)
                      * specularStrength;
    color += uLightColor * specular * shadow * ndl;

    // Vet bot trang sat duong song tren bai cat.
    float edge = 1.0 - smoothstep(0.0, 0.07, abs(p.y - runup));
    float foamNoise = vnoise(p.xz * 3.0 + uTime * 0.2);
    vec3 foamColor = vec3(0.90) * (uLightColor * 0.5 + uAmbient * 1.3);
    color = mix(color, foamColor,
                edge * smoothstep(0.35, 0.70, foamNoise) * 0.6);

    // Dia hinh duoi muc nuoc nhan mau xanh va hap thu theo do sau.
    float depthBelowSea = max(-p.y, 0.0);
    if (depthBelowSea > 0.0) {
        vec3 attenuation = exp(-depthBelowSea * vec3(0.35, 0.10, 0.06));
        vec3 underwaterTint = vec3(0.02, 0.14, 0.16)
                            * (uAmbient * 1.5 + uLightColor * 0.2);
        color = mix(color * attenuation, underwaterTint,
                    1.0 - exp(-depthBelowSea * 0.25));
    }

    // Suong khi quyen gan chan troi, cung dung ham atmosphere chung.
    vec3 toPoint = p - uCamPos;
    float distanceToPoint = length(toPoint);
    if (distanceToPoint > 1e-4 && p.y > -3.0) {
        vec3 viewDirection = toPoint / distanceToPoint;
        float fogAmount = 1.0 - exp(-pow(distanceToPoint * 0.006, 2.0));
        vec3 fogColor = atmosphere(
            normalize(vec3(viewDirection.x,
                           0.02 + max(viewDirection.y, 0.0) * 0.3,
                           viewDirection.z)), 6, 3);
        color = mix(color, fogColor, fogAmount);
    }

    FragColor = vec4(tonemap(color), 1.0);
}
)GLSL";
} // namespace

bool init() {
    if (gProgram != 0 || gMesh.vao != 0 || gHeightTexture != 0) {
        // Chuan hoa hanh vi neu init bi goi nhieu lan.
        destroy();
    }

    const std::string vertexSource = std::string(kGLSL_VERSION) + kTerrainVertexShader;
    const std::string fragmentSource = std::string(kGLSL_VERSION)
        + kGLSL_COMMON + kGLSL_HEIGHT + kTerrainFragmentShader;

    gProgram = createProgram(vertexSource, fragmentSource);
    if (gProgram == 0) {
        destroy();
        return false;
    }

    std::printf("[Terrain] Dang sinh heightmap %d x %d bang Perlin/fBm...\n",
                kGridResolution, kGridResolution);
    std::vector<float> heights;
    if (!buildTerrainMesh(heights)) {
        destroy();
        return false;
    }
    if (!createHeightTexture(heights)) {
        destroy();
        return false;
    }

    std::printf("[Terrain] Khoi tao thanh cong.\n");
    return true;
}

void render(const FrameContext& frame) {
    if (gProgram == 0 || gMesh.vao == 0 || gMesh.indexCount <= 0) return;

    glUseProgram(gProgram);
    setMat4(gProgram, "uVP", frame.vp);
    setVec3(gProgram, "uCamPos", frame.camPos);
    setVec3(gProgram, "uSunDir", frame.lighting.sunDir);
    setVec3(gProgram, "uLightDir", frame.lighting.lightDir);
    setVec3(gProgram, "uLightColor", frame.lighting.lightColor);
    setVec3(gProgram, "uAmbient", frame.lighting.ambient);
    setFloat(gProgram, "uExposure", frame.lighting.exposure);
    setFloat(gProgram, "uTime", frame.time);

    setInt(gProgram, "uHeight", 0);
    setVec2(gProgram, "uTMin", frame.terrainMin);
    setFloat(gProgram, "uTSize", frame.terrainSize);
    setVec2(gProgram, "uHS", frame.heightScaleBias);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gHeightTexture);
    glBindVertexArray(gMesh.vao);
    glDrawElements(GL_TRIANGLES, gMesh.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void destroy() {
    if (gHeightTexture != 0) {
        glDeleteTextures(1, &gHeightTexture);
        gHeightTexture = 0;
    }
    if (gMesh.ebo != 0) {
        glDeleteBuffers(1, &gMesh.ebo);
        gMesh.ebo = 0;
    }
    if (gMesh.vbo != 0) {
        glDeleteBuffers(1, &gMesh.vbo);
        gMesh.vbo = 0;
    }
    if (gMesh.vao != 0) {
        glDeleteVertexArrays(1, &gMesh.vao);
        gMesh.vao = 0;
    }
    gMesh.indexCount = 0;

    if (gProgram != 0) {
        glDeleteProgram(gProgram);
        gProgram = 0;
    }
}

float heightAt(float x, float z) {
    return calculateHeight(x, z);
}

GLuint getHeightTexture() {
    return gHeightTexture;
}

} // namespace Terrain
