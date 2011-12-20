#define YUV2RGB_FRAGMENT_SHADER "\n" \
    "uniform sampler2D t_texture_y;\n" \
    "uniform sampler2D t_texture_u;\n" \
    "uniform sampler2D t_texture_v;\n" \
    "varying vec2 uvVarying;\n" \
    "void main()\n" \
    "{\n" \
        "float y,u,v,r,g,b;\n" \
        "y = texture2D(t_texture_y, uvVarying).r;\n" \
        "u = texture2D(t_texture_u, uvVarying).r;\n" \
        "v = texture2D(t_texture_v, uvVarying).r;\n" \
        "\n" \
        "y = 1.16438355 * (y - 0.0625);\n" \
        "u = u - 0.5;\n" \
        "v = v - 0.5;\n" \
        "\n" \
        "r = clamp(y + 1.596 * v, 0.0, 1.0);\n" \
        "g = clamp(y - 0.391 * u - 0.813 * v, 0.0, 1.0);\n" \
        "b = clamp(y + 2.018 * u, 0.0, 1.0);\n" \
        "\n" \
        "gl_FragColor = vec4(r,g,b,1.0);\n" \
    "}\n"

