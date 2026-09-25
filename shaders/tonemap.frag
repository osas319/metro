#version 450
// Post-process geçişi: sahne pass'ının yazdığı HDR (lineer, sıkıştırılmamış)
// rengi örnekler, ACES filmic tonemap ile LDR'a sıkıştırır. Swapchain SRGB
// formatında olduğundan burada lineer 0-1 yazılır; donanım sRGB OETF'ini
// otomatik uygular (pbr.frag'daki "lineer yaz, donanım kodlasın" kuralıyla
// tutarlı).
layout(set = 0, binding = 0) uniform sampler2D uHdrColor;

layout(push_constant) uniform PostProcessPush {
    float trainSpeedMps;
    float pad0;
    float pad1;
    float pad2;
};

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

    // İleri hareket bulanıklığı tren hızına göre yumuşak biçimde artar.
    // smoothstep kullanıldığı için belirli bir hızda bir anda açılmaz.
    float speed = abs(trainSpeedMps);
    float speed01 = smoothstep(2.0, 20.0, speed);
    float motionWeight = speed01 * speed01;
    float blurRadius = 0.030 * motionWeight;

    if (blurRadius > 0.0005) {
        vec2 centered = vUv * 2.0 - 1.0;
        float distanceFromCenter = length(centered);
        vec2 radialDir = centered / max(distanceFromCenter, 0.0001);

        // Görüş merkezine yakın bölgeyi daha az, kenarları daha çok sürükle.
        vec2 blurStep = radialDir * blurRadius *
                        mix(0.20, 1.0, smoothstep(0.05, 0.85, distanceFromCenter));

        vec3 motion = hdr * 0.26;
        motion += texture(uHdrColor, vUv - blurStep * 0.85).rgb * 0.14;
        motion += texture(uHdrColor, vUv - blurStep * 0.55).rgb * 0.15;
        motion += texture(uHdrColor, vUv - blurStep * 0.25).rgb * 0.15;
        motion += texture(uHdrColor, vUv + blurStep * 0.25).rgb * 0.11;
        motion += texture(uHdrColor, vUv + blurStep * 0.55).rgb * 0.09;
        motion += texture(uHdrColor, vUv + blurStep * 0.85).rgb * 0.10;

        hdr = mix(hdr, motion, 0.72 * motionWeight);
    }

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
