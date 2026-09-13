#version 450
// Post-process tam ekran üçgeni: vertex/index buffer yok, gl_VertexIndex'ten
// üretilir (3 vertex'lik tek üçgen ekranı kaplar, ekran dışı kısım kırpılır).
// Standart teknik: bkz. Sascha Willems "fullscreen triangle" örneği.
layout(location = 0) out vec2 vUv;

void main() {
    vUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUv * 2.0 - 1.0, 0.0, 1.0);
}
