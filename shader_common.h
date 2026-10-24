#pragma once

// Cac chuoi GLSL dung chung cho cac shader cua Terrain, Ocean va Sky.
// Shader module phai ghep version truoc, roi moi toi cac doan GLSL can dung.
// Vi du: std::string(kGLSL_VERSION) + kGLSL_COMMON + shaderFragmentRieng.

inline constexpr char kGLSL_VERSION[] = "#version 330 core\n";

// Noise, tone mapping va single-scattering Rayleigh/Mie.
// Cac uniform trong doan nay can duoc set neu shader co su dung:
// uSunDir, uExposure, uTime.
inline constexpr char kGLSL_COMMON[] = R"GLSL(
const float PI = 3.14159265359;
uniform vec3  uSunDir;      // huong tu diem quan sat ve phia mat troi
uniform float uExposure;
uniform float uTime;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash13(vec3 p3) {
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec3 tonemap(vec3 c) {
    c = vec3(1.0) - exp(-c * uExposure);
    return pow(c, vec3(1.0 / 2.2));
}

// Tham so khi quyen (don vi km).
const float R_E    = 6360.0;
const float R_A    = 6420.0;
const float H_R    = 7.994;
const float H_M    = 1.2;
const vec3  BETA_R = vec3(5.8e-3, 13.5e-3, 33.1e-3);
const float BETA_M = 21.0e-3;
const float G_M    = 0.76;
const float SUN_I  = 22.0;

vec2 raySphere(vec3 o, vec3 d, float r) {
    float b = dot(o, d);
    float c = dot(o, o) - r * r;
    float disc = b * b - c;
    if (disc < 0.0) return vec2(-1.0);
    float s = sqrt(disc);
    return vec2(-b - s, -b + s);
}

// Tinh mau khi quyen bang tich phan single-scattering.
// nView/nLight la so mau, co the giam o shader tinh toan tren moi pixel.
vec3 atmosphere(vec3 dir, int nView, int nLight) {
    dir = normalize(vec3(dir.x, max(dir.y, 1e-3), dir.z));
    vec3 o = vec3(0.0, R_E + 0.002, 0.0);
    float tMax = raySphere(o, dir, R_A).y;

    float mu = dot(dir, uSunDir);
    float phaseR = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
    float g2 = G_M * G_M;
    float phaseM = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu * mu))
                 / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * G_M * mu, 1.5));

    vec3 sumR = vec3(0.0);
    vec3 sumM = vec3(0.0);
    float optR = 0.0;
    float optM = 0.0;
    float fn = float(nView);

    for (int i = 0; i < nView; ++i) {
        float t0 = tMax * float(i) * float(i) / (fn * fn);
        float t1 = tMax * float(i + 1) * float(i + 1) / (fn * fn);
        float dt = t1 - t0;
        vec3 p = o + dir * (0.5 * (t0 + t1));
        float h = max(length(p) - R_E, 0.0);
        float dR = exp(-h / H_R) * dt;
        float dM = exp(-h / H_M) * dt;
        optR += dR;
        optM += dM;

        // Neu hanh tinh chan tia sang thi bo qua mau nay.
        vec2 planetHit = raySphere(p, uSunDir, R_E);
        if (planetHit.x > 0.0) continue;

        float tl = raySphere(p, uSunDir, R_A).y;
        float lR = 0.0;
        float lM = 0.0;
        float fl = float(nLight);
        for (int j = 0; j < nLight; ++j) {
            float s0 = tl * float(j) * float(j) / (fl * fl);
            float s1 = tl * float(j + 1) * float(j + 1) / (fl * fl);
            vec3 q = p + uSunDir * (0.5 * (s0 + s1));
            float hq = max(length(q) - R_E, 0.0);
            lR += exp(-hq / H_R) * (s1 - s0);
            lM += exp(-hq / H_M) * (s1 - s0);
        }

        vec3 tau = BETA_R * (optR + lR) + BETA_M * 1.1 * (optM + lM);
        vec3 att = exp(-tau);
        sumR += att * dR;
        sumM += att * dM;
    }

    return SUN_I * (sumR * BETA_R * phaseR + sumM * BETA_M * phaseM);
}
)GLSL";

// Truy cap height texture. Doan nay dung duoc trong vertex hoac fragment shader.
// Module su dung phai set uHeight, uTMin, uTSize va uHS tu FrameContext.
inline constexpr char kGLSL_HEIGHT[] = R"GLSL(
uniform sampler2D uHeight;
uniform vec2  uTMin;
uniform float uTSize;
uniform vec2  uHS;          // scale, bias de can tam texel

float terrH(vec2 xz) {
    vec2 uv = (xz - uTMin) / uTSize * uHS.x + uHS.y;
    return textureLod(uHeight, uv, 0.0).r;
}
)GLSL";
