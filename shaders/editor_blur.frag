#version 450
// Editor cam efekti: sadece UI cam panellerinde gösterilecek tam ekran
// düşük çözünürlüklü bulanık sahne. Asıl swapchain görüntüsü değiştirilmez.
layout(set = 0, binding = 0) uniform sampler2D uHdrColor;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

vec3 acesFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    ivec2 size = textureSize(uHdrColor, 0);
    vec2 texel = 1.0 / vec2(size);

    // Yumuşak 13 örnekli blur. Bu texture yalnızca ImGui cam panellerinin
    // arkasına çizilir; 3D viewport'un kendisi keskin kalır.
    vec3 sum = texture(uHdrColor, vUv).rgb * 0.20;
    sum += texture(uHdrColor, vUv + texel * vec2( 1.5,  0.0)).rgb * 0.10;
    sum += texture(uHdrColor, vUv + texel * vec2(-1.5,  0.0)).rgb * 0.10;
    sum += texture(uHdrColor, vUv + texel * vec2( 0.0,  1.5)).rgb * 0.10;
    sum += texture(uHdrColor, vUv + texel * vec2( 0.0, -1.5)).rgb * 0.10;
    sum += texture(uHdrColor, vUv + texel * vec2( 1.7,  1.7)).rgb * 0.07;
    sum += texture(uHdrColor, vUv + texel * vec2(-1.7,  1.7)).rgb * 0.07;
    sum += texture(uHdrColor, vUv + texel * vec2( 1.7, -1.7)).rgb * 0.07;
    sum += texture(uHdrColor, vUv + texel * vec2(-1.7, -1.7)).rgb * 0.07;
    sum += texture(uHdrColor, vUv + texel * vec2( 3.0,  0.0)).rgb * 0.03;
    sum += texture(uHdrColor, vUv + texel * vec2(-3.0,  0.0)).rgb * 0.03;
    sum += texture(uHdrColor, vUv + texel * vec2( 0.0,  3.0)).rgb * 0.03;
    sum += texture(uHdrColor, vUv + texel * vec2( 0.0, -3.0)).rgb * 0.03;

    vec3 color = acesFilm(sum * 0.95);
    outColor = vec4(color, 1.0);
}
