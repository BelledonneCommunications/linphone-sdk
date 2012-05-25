attribute vec2 position;
attribute vec2 uv;
uniform mat4 proj_matrix;
uniform float rotation;
varying vec2 uvVarying;

void main()
{
    mat3 rot = mat3(vec3(cos(rotation), sin(rotation),0.0), vec3(-sin(rotation), cos(rotation), 0.0), vec3(0.0, 0.0, 1.0));
    gl_Position = proj_matrix * vec4(rot * vec3(position.xy, 0.0), 1.0);
    uvVarying = uv;
}
