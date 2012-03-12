#if TARGET_OS_IPHONE || defined(ANDROID)
#define PRECISION_DECL "precision mediump float;\n"
#else
#define PRECISION_DECL ""
#endif

#define YUV2RGB_VERTEX_SHADER "attribute vec2 position;\n" \
"attribute vec2 uv;\n" \
"uniform mat4 proj_matrix;\n" \
"uniform float rotation;\n" \
"varying vec2 uvVarying;\n" \
"\n" \
"void main()\n" \
"{\n" \
"   mat3 rot = mat3(vec3(cos(rotation), sin(rotation),0.0), vec3(-sin(rotation), cos(rotation), 0.0), vec3(0.0, 0.0, 1.0));\n" \
"    gl_Position = proj_matrix * vec4(rot * vec3(position.xy, 0.0), 1.0);\n" \
"    uvVarying = uv;\n" \
"}\n"
