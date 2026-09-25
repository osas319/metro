#version 450
// Cook-Torrance PBR (metallic-roughness): D(GGX) + Smith geometrisi +
// Schlick Fresnel. Tek yönlü ışık + hafif ambient; doku yok — faktörler
// push-constant'tan. HDR offscreen hedefe (kHdrFormat) yazar; ACES tonemap
// artık ayrı bir post-process geçişinde (tonemap.frag) uygulanıyor, bu
// yüzden burada ışık şiddetini 1.0 üstüne çıkarmaktan çekinmeye gerek yok —
// parlak alanlar sert kırpılmak yerine ACES eğrisiyle yumuşak biçimde
// tepe yapar.
layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 viewProj;
    vec4 lightDir;
    vec4 cameraPos;
} frame;

layout(push_constant) uniform ModelPush {
    mat4 model;
    vec4 baseColor;
    float metallic;
    float roughness;
} push;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Trowbridge-Reitz GGX normal dağılımı
float distributionGGX(float NdotH, float alpha) {
    float a2 = alpha * alpha;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
}

// Schlick-GGX geometri (Smith'in ayrık k approximations'ı)
float geometrySchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}
float geometrySmith(float NdotV, float NdotL, float k) {
    return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(frame.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(-frame.lightDir.xyz);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 albedo = push.baseColor.rgb;
    float metallic = clamp(push.metallic, 0.0, 1.0);
    float roughness = clamp(push.roughness, 0.045, 1.0); // sıfır parlaklık patlamasını önle
    float alpha = roughness * roughness;

    // IBL gelene kadar: k direkt ışık için alpha^2/2 kuralı
    float k = alpha * alpha * 0.5;

    // dielectric F0 = 0.04, metalde albedo'ya kayar
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);

    float D = distributionGGX(NdotH, alpha);
    float G = geometrySmith(NdotV, NdotL, k);

    vec3 specular = vec3(0.0);
    if (NdotL > 0.0) {
        specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
    }
    // metalde difüz yok; enerji korunumu (1-F) ile
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;

    vec3 radiance = vec3(2.6); // tek yönlü ışık şiddeti (sonra: ışık listesi + intensity)
    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    // Tren farı: menzili yumuşak düşen lokal ışık.
    vec3 toHeadlight = frame.headlightPos.xyz - vWorldPos;
    float distanceToHeadlight = length(toHeadlight);
    vec3 HL = normalize(toHeadlight);
    float HNdL = max(dot(N, HL), 0.0);
    float falloff = 1.0 - smoothstep(frame.headlightColor.a * 0.45,
                                     frame.headlightColor.a,
                                     distanceToHeadlight);
    float headlightFacing = pow(max(dot(-HL, vec3(0.0, 0.0, -1.0)), 0.0), 1.5);
    vec3 Hh = normalize(V + HL);
    float hNdotH = max(dot(N, Hh), 0.0);
    float hHdotV = max(dot(Hh, V), 0.0);
    vec3 hF = F0 + (1.0 - F0) * pow(clamp(1.0 - hHdotV, 0.0, 1.0), 5.0);
    float hD = distributionGGX(hNdotH, alpha);
    float hG = geometrySmith(NdotV, max(HNdL, 1e-4), k);
    vec3 hSpec = (hD * hG * hF) / max(4.0 * NdotV * max(HNdL, 1e-4), 1e-4);
    vec3 hDiffuse = (1.0 - hF) * (1.0 - metallic) * albedo / PI;
    Lo += (hDiffuse + hSpec) * frame.headlightColor.rgb * HNdL *
          falloff * headlightFacing * 3.0;

    // Hafif hemisfer ambient — tünelde gökyüzü yok, zemin yansıması hissi
    vec3 ambient = albedo * 0.10 * (0.5 + 0.5 * N.y);

    vec3 color = ambient + Lo;
    // Swapchain SRGB: lineer yaz, donanım kodlasın. ACES tonemap post ile.
    outColor = vec4(color, push.baseColor.a);
}
