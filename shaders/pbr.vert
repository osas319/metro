#version 450
// PBR vertex shader: dünya uzayı pozisyon/normal üretir.
// Frame UBO (set 0) kamerayı, push-constant ise model matrisi + malzemeyi taşır.
layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 viewProj;
    vec4 lightDir;   // xyz: ışığın gittiği yön (L = -lightDir)
    vec4 cameraPos;  // xyz: göz/kamera dünya konumu
    vec4 headlightPos;   // xyz: tren farı dünya konumu
    vec4 headlightColor; // rgb: far rengi, a: menzil
} frame;

layout(push_constant) uniform ModelPush {
    mat4 model;      // lokal -> dünya
    vec4 baseColor;  // rgb + alpha (metallic-roughness baseColorFactor)
    float metallic;
    float roughness;
    float materialId;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;

out gl_PerVertex { vec4 gl_Position; };

void main() {
    vec4 world = push.model * vec4(inPosition, 1.0);
    vWorldPos = world.xyz;
    // Uniform ölçek varsayımı (istasyon assetleri); uniform-olmayan
    // ölçeklerde inverse-transpose gerekir.
    vNormal = normalize(mat3(push.model) * inNormal);
    vTexCoord = inTexCoord;
    gl_Position = frame.viewProj * world;
}
