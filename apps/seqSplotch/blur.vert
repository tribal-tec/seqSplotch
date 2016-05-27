#version 330

uniform mat4 projection;

in vec4 position;

out vec2 texcoord;

void main()
{
    texcoord	= vec2(position);
    gl_Position	= projection * position;
}
