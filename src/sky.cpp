#include "sky.h"
#include "shader_common.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cmath>
#include <string>

namespace Sky {

    static GLuint gSkyVAO    = 0;
    static GLuint gSkyVBO    = 0;
    static GLuint gSkyShader = 0;

    static GLuint compileShader(const char* source, GLenum type) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, log);
            std::fprintf(stderr, "[Sky] Shader compile error:\n%s\n", log);
        }
        return shader;
    }

    bool init() {
        float vertices[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f
        };

        glGenVertexArrays(1, &gSkyVAO);
        glGenBuffers(1, &gSkyVBO);
        glBindVertexArray(gSkyVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gSkyVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        const char* vsSource = R"GLSL(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            out vec2 vNdc;
            void main() {
                vNdc = aPos.xy;
                gl_Position = vec4(aPos, 1.0);
            }
        )GLSL";

        std::string fsSource = std::string(kGLSL_VERSION) + kGLSL_COMMON + R"GLSL(
            in vec2 vNdc;
            out vec4 FragColor;
            uniform mat4 uInvSkyVP;

            // Ham tao sao tu nhien hon
            float renderStars(vec3 dir) {
                // Su dung phep chieu de tranh bien dang o cuc
                vec2 starUV = dir.xz / (abs(dir.y) + 0.2); 
                
                // Mat do sao
                float density = 120.0; 
                starUV *= density;

                vec2 gridCell = floor(starUV);
                vec2 localPos = fract(starUV) - 0.5;

                // Dung nhieu de phan bo sao tu nhien hon (thay vi moi o deu co sao)
                float starChance = hash12(gridCell);
                
                // Chi nhung o co gia tri cao moi co sao
                if (starChance > 0.985) {
                    // Vi tri ngau nhien trong o
                    vec2 starPos = vec2(hash12(gridCell + 1.0), hash12(gridCell + 2.0)) - 0.5;
                    float dist = length(localPos - starPos);
                    
                    // Kich thuoc sao ngau nhien (nho, vua, to)
                    float baseRadius = mix(0.05, 0.15, hash12(gridCell + 3.0));
                    
                    // Hinh dang sao (su dung smoothstep de co vien mem)
                    float starShape = smoothstep(baseRadius, 0.0, dist);
                    
                    // Do sang ngau nhien
                    float brightness = 0.3 + 0.7 * hash12(gridCell + 4.0);

                    // Hieu ung lap lanh rat nhe (nhu sao that)
                    float twinkle = 0.8 + 0.2 * sin(uTime * 2.0 + hash12(gridCell + 5.0) * 100.0);

                    return starShape * brightness * twinkle;
                }
                return 0.0;
            }

            void main() {
                // --- BUOC 1: RAY-CASTING ---
                vec4 rayClip = vec4(vNdc.x, vNdc.y, 1.0, 1.0);
                vec4 rayWorld = uInvSkyVP * rayClip;
                vec3 dir = normalize(rayWorld.xyz / rayWorld.w);

                // --- BUOC 2: MAU BAU TROI ---
                vec3 color = atmosphere(dir, 8, 4);

                // --- BUOC 3: MAT TRANG VA SAO ---
                float skyMask = smoothstep(0.0, 0.1, dir.y);
                float sunHeight = uSunDir.y;
                
                if (sunHeight < 0.05 && skyMask > 0.0) {
                    float nightFactor = smoothstep(0.05, -0.15, sunHeight);

                    // [MAT TRANG]
                    vec3 moonDir = normalize(vec3(-uSunDir.x, -uSunDir.y, -uSunDir.z)); 
                    float moonDot = dot(dir, moonDir);
                    float moonRadius = 0.9990; 
                    float isMoon = smoothstep(moonRadius, moonRadius + 0.0003, moonDot);
                    vec3 moonColor = vec3(1.0, 0.98, 0.92);
                    color = mix(color, moonColor, isMoon * nightFactor * skyMask);

                    // [NGOI SAO]
                    float starIntensity = renderStars(dir);
                    color = mix(color, vec3(1.0), starIntensity * nightFactor * skyMask);
                }

                // --- BUOC 4: TONE MAPPING ---
                color = tonemap(color);
                FragColor = vec4(color, 1.0);
            }
        )GLSL";

        GLuint vs = compileShader(vsSource, GL_VERTEX_SHADER);
        GLuint fs = compileShader(fsSource.c_str(), GL_FRAGMENT_SHADER);
        if (vs == 0 || fs == 0) return false;

        gSkyShader = glCreateProgram();
        glAttachShader(gSkyShader, vs);
        glAttachShader(gSkyShader, fs);
        glLinkProgram(gSkyShader);

        GLint linked = 0;
        glGetProgramiv(gSkyShader, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[1024];
            glGetProgramInfoLog(gSkyShader, 1024, nullptr, log);
            std::fprintf(stderr, "[Sky] Shader link error:\n%s\n", log);
            return false;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return true;
    }

    Lighting computeLighting(float phase) {
        Lighting L;
        float cosA = std::cos(phase);
        float sinA = std::sin(phase);
        glm::vec3 sunDir = glm::normalize(glm::vec3(cosA, sinA, 0.0f));
        L.sunDir   = sunDir;
        L.lightDir = -sunDir;

        if (sinA > 0.0f) {
            L.lightColor = glm::vec3(1.0f, 0.95f, 0.85f);
            float intensity = glm::smoothstep(0.0f, 0.3f, sinA);
            L.lightColor *= (0.5f + 1.5f * intensity);
            L.ambient   = glm::vec3(0.35f, 0.40f, 0.50f);
            L.exposure  = 1.0f;
        } else {
            L.lightColor = glm::vec3(0.35f, 0.45f, 0.65f) * 0.4f;
            L.ambient    = glm::vec3(0.03f, 0.04f, 0.08f);
            L.exposure   = 2.0f;
        }
        return L;
    }

    void render(const FrameContext& frame) {
        glUseProgram(gSkyShader);

        glUniformMatrix4fv(glGetUniformLocation(gSkyShader, "uInvSkyVP"),
                           1, GL_FALSE, glm::value_ptr(frame.invSkyVP));
        glUniform3fv(glGetUniformLocation(gSkyShader, "uSunDir"),
                     1, glm::value_ptr(frame.lighting.sunDir));
        glUniform1f(glGetUniformLocation(gSkyShader, "uExposure"),
                    frame.lighting.exposure);
        glUniform1f(glGetUniformLocation(gSkyShader, "uTime"),
                    frame.time);

        glBindVertexArray(gSkyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glUseProgram(0);
    }

    void destroy() {
        if (gSkyVBO)    { glDeleteBuffers(1, &gSkyVBO);        gSkyVBO = 0; }
        if (gSkyVAO)    { glDeleteVertexArrays(1, &gSkyVAO);   gSkyVAO = 0; }
        if (gSkyShader) { glDeleteProgram(gSkyShader);         gSkyShader = 0; }
    }

}