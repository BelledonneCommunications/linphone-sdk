#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D t_texture_y;
uniform sampler2D t_texture_u;
uniform sampler2D t_texture_v;
varying vec2 uvVarying;
void main()
{
	float y,u,v,r,g,b, gradx, grady;
	y = texture2D(t_texture_y, uvVarying).r;
	u = texture2D(t_texture_u, uvVarying).r;
	v = texture2D(t_texture_v, uvVarying).r;
	y = 1.16438355 * (y - 0.0625);
	u = u - 0.5;
	v = v - 0.5;
	r = clamp(y + 1.596 * v, 0.0, 1.0);
	g = clamp(y - 0.391 * u - 0.813 * v, 0.0, 1.0);
	b = clamp(y + 2.018 * u, 0.0, 1.0);
	gl_FragColor = vec4(r,g,b,1.0);
}

