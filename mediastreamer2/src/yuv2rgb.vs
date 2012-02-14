attribute vec4 position;
attribute vec2 uv;
uniform mat4 matrix;
varying vec2 uvVarying;

void main()
{
    gl_Position = matrix * position;
    uvVarying = uv;
}
