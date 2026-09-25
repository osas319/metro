#version 450
// Post-process geçişi: sahne pass'ının yazdığı HDR (lineer, sıkıştırılmamış)
// rengi örnekler, ACES filmic tonemap ile LDR'a sıkıştırır. Swapchain SRGB
// formatında olduğundan burada lineer 0-1 yazılır; donanım sRGB OETF'ini
// otomatik uygular (pbr.frag'daki "lineer yaz, donanım kodlasın" kuralıyla
// tutarlı).
layout(set = 0, binding = 0) uniform sampler2D uHdrColor;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Narkowicz 2015 ACES filmic yaklaşık eğrisi: tam ACES RRT+ODT zincirinin
// ucuz yaklaşımı, gerçek zamanlı kullanım için yeterli görsel doğrulukta.
vec3 acesFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHdrColor, vUv).rgb;

    // Tek geçişte ucuz HDR bloom: parlak komşu piksellerden küçük bir hale.
    ivec2 imageSize = textureSize(uHdrColor, 0);
    vec2 texel = 1.0 / vec2(imageSize);
    vec3 bloom = vec3(0.0);
    float weightSum = 0.0;
    const float weights[5] = float[](0.24, 0.16, 0.10, 0.06, 0.03);
    for (int axis = 0; axis < 2; ++axis) {
        for (int radius = 1; radius <= 4; ++radius) {
            vec2 offset = axis == 0
                ? vec2(texel.x * float(radius), 0.0)
                : vec2(0.0, texel.y * float(radius));
            vec3 a = texture(uHdrColor, vUv + offset).rgb;
            vec3 b = texture(uHdrColor, vUv - offset).rgb;
            float la = dot(a, vec3(0.2126, 0.7152, 0.0722));
            float lb = dot(b, vec3(0.2126, 0.7152, 0.0722));
            bloom += max(a - vec3(1.0), vec3(0.0)) * weights[radius] +
                     max(b - vec3(1.0), vec3(0.0)) * weights[radius];
            weightSum += 2.0 * weights[radius] *
                         (step(1.0, la) + step(1.0, lb)) * 0.5;
        }
    }
    if (weightSum > 0.0) bloom /= weightSum;
    hdr += bloom * 0.18;

    // Hafif sinema/vignette; merkez dışını çok az karartır.
    vec2 centered = vUv * 2.0 - 1.0;
    float vignette = 1.0 - smoothstep(0.35, 1.15, dot(centered, centered));
    hdr *= mix(0.92, 1.0, vignette);

    outColor = vec4(acesFilm(hdr), 1.0);
}
