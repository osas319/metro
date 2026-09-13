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
    outColor = vec4(acesFilm(hdr), 1.0);
}
