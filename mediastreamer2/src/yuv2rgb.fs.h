#if TARGET_OS_IPHONE || defined(ANDROID)
#define PRECISION_DECL "precision mediump float;\n"
#else
#define PRECISION_DECL ""
#endif

#define YUV2RGB_FRAGMENT_SHADER PRECISION_DECL \
    "uniform sampler2D t_texture_y;\n" \
    "uniform sampler2D t_texture_u;\n" \
    "uniform sampler2D t_texture_v;\n" \
    "varying vec2 uvVarying;\n" \
    "void main()\n" \
    "{\n" \
        "float y,u,v,r,g,b;\n" \
        "const float c1 = float(255.0/219.0);\n" \
        "const float c2 = float(16.0/256.0);\n" \
        "const float c3 = float(1.0/2.0);\n" \
        "const float c4 = float(1596.0/1000.0);\n" \
        "const float c5 = float(391.0/1000.0);\n" \
        "const float c6 = float(813.0/1000.0);\n" \
        "const float c7 = float(2018.0/1000.0);\n" \
        "y = texture2D(t_texture_y, uvVarying).r;\n" \
        "u = texture2D(t_texture_u, uvVarying).r;\n" \
        "v = texture2D(t_texture_v, uvVarying).r;\n" \
        "\n" \
        "y = c1 * (y - c2);\n" \
        "u = u - c3;\n" \
        "v = v - c3;\n" \
        "\n" \
        "r = clamp(y + c4 * v, 0.0, 1.0);\n" \
        "g = clamp(y - c5 * u - c6 * v, 0.0, 1.0);\n" \
        "b = clamp(y + c7 * u, 0.0, 1.0);\n" \
        "\n" \
        "gl_FragColor = vec4(r,g,b,1.0);\n" \
    "}\n"

