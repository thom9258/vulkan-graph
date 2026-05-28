#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout (set = 0, binding = 0)
uniform Info
{
	mat4 view;
	mat4 proj;
	mat4 model;
} info;

//position is post MVP translation of vertex
vec4 to_low_precision(vec4 position,vec2 resolution)
{
	//Perform perspective divide
	vec3 perspective_divide = position.xyz / vec3(position.w);
	
	//Convert to screenspace coordinates
	vec2 screen_coords = (perspective_divide.xy 
							+ vec2(1.0,1.0)) 
							* vec2(resolution.x,resolution.y) 
							* 0.5;

	//Truncate to integer
	vec2 screen_coords_truncated = vec2(int(screen_coords.x),
										int(screen_coords.y));
	
	//Convert back to clip range -1 to 1
	vec2 reconverted_xy = ((screen_coords_truncated * vec2(2,2))
							 / vec2(resolution.x,resolution.y)) 
							- vec2(1,1);

	//Construct return value
	vec4 ps1_pos = vec4(reconverted_xy.x,
						reconverted_xy.y,
						perspective_divide.z,
						position.w);

	ps1_pos.xyz = ps1_pos.xyz * vec3(	position.w,
										position.w,
										position.w);

	return ps1_pos;
}


void main() {
	const vec2 resolution = vec2(320.0, 240.0);
    const mat4 transform = info.proj * info.view * info.model;
	const vec4 position = transform * vec4(inPosition, 1.0f);
	gl_Position = to_low_precision(position, resolution);
    fragColor = inColor;
}
