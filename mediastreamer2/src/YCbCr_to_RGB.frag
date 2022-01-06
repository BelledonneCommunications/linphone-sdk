#version 410 core

/* Takes in 3-planar YUV 420 data in partial range 601 colour space
   and draws it to an RGB(A) surface.
   https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
*/

uniform sampler2D t_texture_y;
uniform sampler2D t_texture_u;
uniform sampler2D t_texture_v;
in vec2 uvVarying;
out vec4 color;

const float Kr = 0.299;
const float Kg = 0.587;
const float Kb = 0.114;

// Pivoted because GLSL stores matrices as column major
const mat3 inverse_color_matrix = mat3(
	1.          , 1.                        , 1.          ,
	0.          , - Kb / Kg * (2. - 2. * Kb), 2. - 2. * Kb,
	2. - 2. * Kr, - Kr / Kg * (2. - 2. * Kr), 0.          );

const vec2 digital_scale = vec2(
	255. / 219., // Luma
	255. / 224.  // Chroma
);

const vec2 digital_shift = digital_scale * -vec2(
	16.  / 256., // Luma
	128. / 256.  // Chroma
);

void main()
{
	vec3 YCbCr = vec3(
		texture(t_texture_y, uvVarying).r,
		texture(t_texture_u, uvVarying).r,
		texture(t_texture_v, uvVarying).r
	);
	// Accounting for partial range (and shifting chroma to positive range)
	YCbCr = digital_scale.xyy * YCbCr + digital_shift.xyy; // as a MAD operation
	color = vec4(inverse_color_matrix * YCbCr, 1.0);
}
