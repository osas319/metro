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
    vec4 headlightPos;   // xyz: tren farı dünya konumu
    vec4 headlightColor; // rgb: far rengi, a: menzil
} frame;

layout(push_constant) uniform ModelPush {
    mat4 model;
    vec4 baseColor;
    float metallic;
    float roughness;
    float materialId;
} push;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i + vec3(0,0,0));
    float n100 = hash13(i + vec3(1,0,0));
    float n010 = hash13(i + vec3(0,1,0));
    float n110 = hash13(i + vec3(1,1,0));
    float n001 = hash13(i + vec3(0,0,1));
    float n101 = hash13(i + vec3(1,0,1));
    float n011 = hash13(i + vec3(0,1,1));
    float n111 = hash13(i + vec3(1,1,1));
    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    return mix(mix(nx00, nx10, f.y), mix(nx01, nx11, f.y), f.z);
}

float tileMask(vec2 uv, vec2 tileScale, float groutWidth) {
    vec2 cell = fract(uv * tileScale);
    float gx = 1.0 - smoothstep(0.0, groutWidth, min(cell.x, 1.0 - cell.x));
    float gy = 1.0 - smoothstep(0.0, groutWidth, min(cell.y, 1.0 - cell.y));
    return max(gx, gy);
}

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
    float materialId = push.materialId;

    // Dahili materyaller için görünür yüzey dokusu: çok ölçekli beton/metal
    // tanesi, panel kirleri ve kireç derzleri. Bunlar düz renk yerine
    // UV + dünya koordinatlarından örneklenen prosedürel texture katmanlarıdır.
    float macroNoise = noise3(vWorldPos * 0.42);
    float fineNoise = noise3(vWorldPos * 4.8);
    float microNoise = noise3(vWorldPos * 14.0);
    float surfaceTexture = 0.78 + macroNoise * 0.16 +
                            fineNoise * 0.06 + microNoise * 0.025;
    albedo *= surfaceTexture;

    float checker = mod(abs(floor(vWorldPos.x * 1.5) +
                            floor(vWorldPos.z * 1.5)), 2.0);

    if (materialId == 1.0) {
        float seam = max(step(0.94, fract(vWorldPos.x * 1.8)),
                         step(0.94, fract(vWorldPos.z * 1.8)));
        albedo *= mix(1.04, 0.62, seam);
    } else if (materialId == 2.0) {
        float grain = noise3(vWorldPos * vec3(0.8, 8.0, 12.0));
        albedo *= 0.78 + grain * 0.30;
    } else if (materialId == 3.0) {
        // Paslanmaz/çelik kaplama: ince fırçalı metal çizgileri.
        float brushed = 0.72 + 0.24 *
            noise3(vec3(vWorldPos.x * 2.0, vWorldPos.y * 18.0,
                        vWorldPos.z * 0.35));
        albedo = mix(albedo, vec3(0.46, 0.49, 0.53), 0.32);
        albedo *= brushed;
    } else if (materialId == 4.0) {
        // Cam: koyu gövde + düzensiz yansıma/kir.
        float glassNoise = noise3(vWorldPos * 2.4);
        albedo = mix(albedo, vec3(0.012, 0.038, 0.070), 0.72);
        albedo *= 0.86 + glassNoise * 0.18;
    } else if (materialId == 6.0) {
        // Beton/kaldırım blokları.
        albedo *= checker < 1.0 ? 1.08 : 0.72;
        albedo *= 0.90 + noise3(vWorldPos * 3.2) * 0.18;
    } else if (materialId == 7.0) {
        float stripe = smoothstep(0.40, 0.60, fract(vWorldPos.y * 1.5));
        float stripeNoise = 0.92 + noise3(vWorldPos * 6.0) * 0.10;
        albedo = mix(albedo, vec3(0.05, 0.34, 0.62), stripe * 0.35);
        albedo *= stripeNoise;
    }
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

    // Tavan armatürü: nearest 18 m modular luminaire gerçekten ışık verir.
    // Bu, emissive panelin yalnızca bloom ile parlaması yerine zemine ve
    // tren govdesine lokal isik dusurmesini saglar.
    float lampZ = round(vWorldPos.z / 18.0) * 18.0;
    vec3 toLamp = vec3(0.0, 4.05, lampZ) - vWorldPos;
    float lampDistance = length(toLamp);
    vec3 LL = normalize(toLamp);
    vec3 LH = normalize(V + LL);
    float LNdotL = max(dot(N, LL), 0.0);
    float LHdotV = max(dot(LH, V), 0.0);
    float LNdotH = max(dot(N, LH), 0.0);
    vec3 LF = F0 + (1.0 - F0) *
              pow(clamp(1.0 - LHdotV, 0.0, 1.0), 5.0);
    float LD = distributionGGX(LNdotH, alpha);
    float LG = geometrySmith(NdotV, max(LNdotL, 1e-4), k);
    vec3 Lspec = (LD * LG * LF) /
                 max(4.0 * NdotV * max(LNdotL, 1e-4), 1e-4);
    vec3 Ldiff = (1.0 - LF) * (1.0 - metallic) * albedo / PI;
    float lampAttenuation = 1.0 - smoothstep(2.0, 8.5, lampDistance);
    Lo += (Ldiff + Lspec) * vec3(1.0, 0.82, 0.62) *
          LNdotL * lampAttenuation * 1.6;

    // Hafif hemisfer ambient — tünelde gökyüzü yok, zemin yansıması hissi
    vec3 ambient = albedo * 0.10 * (0.5 + 0.5 * N.y);

    vec3 color = ambient + Lo;
    if (materialId == 5.0) {
        color += push.baseColor.rgb * 2.5;
    } else if (materialId == 8.0) {
        color += push.baseColor.rgb * 1.4;
    } else if (materialId == 9.0) {
        // Platform/tactile taş: büyük plakalar + açık derz + leke.
        float grout = tileMask(vWorldPos.xz, vec2(3.2, 6.5), 0.045);
        float stoneNoise = noise3(vWorldPos * vec3(2.2, 1.0, 2.2));
        float stain = noise3(vWorldPos * 0.55);
        color *= mix(0.48, 1.0, 1.0 - grout);
        color *= 0.78 + stoneNoise * 0.22;
        color *= 0.92 + stain * 0.10;
    } else if (materialId == 10.0) {
        // Fırçalanmış paslanmaz çelik.
        float brushed = 0.80 + 0.18 *
            noise3(vec3(vWorldPos.x * 1.5, vWorldPos.y * 20.0,
                        vWorldPos.z * 0.75));
        color *= brushed;
    } else if (materialId == 11.0) {
        // CAF gövde: panel araları, boya ton farkı ve kir.
        vec2 panelUV = vec2(vWorldPos.x, vWorldPos.z);
        float panelSeam = max(
            1.0 - smoothstep(0.025, 0.075, abs(fract(panelUV.y * 0.11) - 0.5)),
            1.0 - smoothstep(0.025, 0.075, abs(fract(panelUV.x * 0.45) - 0.5))
        );
        float grime = smoothstep(0.34, 0.86, noise3(vWorldPos * 1.6));
        float paintVariation = 0.88 + 0.12 * noise3(vWorldPos * 5.0);
        color *= paintVariation;
        color *= mix(1.0, 0.56, panelSeam * 0.75);
        color *= mix(1.0, 0.72, grime * 0.32);
    } else if (materialId == 12.0) {
        // İstasyon/tünel beton ve fayans.
        vec2 tileUV = vec2(vWorldPos.x, vWorldPos.z);
        float grout = tileMask(tileUV, vec2(3.0, 5.0), 0.045);
        float tileNoise = noise3(vWorldPos * 2.8);
        float damp = noise3(vWorldPos * 0.65);
        color *= mix(0.52, 1.0, 1.0 - grout);
        color *= 0.76 + tileNoise * 0.22;
        color *= 0.94 + damp * 0.10;
    } else if (materialId == 13.0) {
        // Hafif kirli cam yansimasi.
        float tint = 0.94 + 0.06 * noise3(vWorldPos * 1.6);
        color *= tint;
        color += vec3(0.015, 0.025, 0.04) * (1.0 - NdotV);
    }

    // Tünel atmosferi: uzak geometriyi yumuşatıp sahne derinliği sağlar.
    const vec3 tunnelFogColor = vec3(0.028, 0.035, 0.050);
    float cameraDistance = distance(frame.cameraPos.xyz, vWorldPos);
    float fogFactor = exp(-cameraDistance * 0.0045);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    color = mix(tunnelFogColor, color, fogFactor);

    // Swapchain SRGB: lineer yaz, donanım kodlasın. ACES tonemap post ile.
    outColor = vec4(color, push.baseColor.a);
}
