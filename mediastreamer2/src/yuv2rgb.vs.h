#define YUV2RGB_VERTEX_SHADER "attribute vec4 position;\n" \
"attribute vec2 uv;\n" \
"uniform mat4 matrix;\n" \
"varying vec2 uvVarying;\n" \
"\n" \
"void main()\n" \
"{\n" \
"    gl_Position = matrix * position;\n" \
"    uvVarying = uv;\n" \
"}\n"
