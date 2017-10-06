#pragma once
#ifndef __MGN_TERRAIN_POSITION_SHADE_TEXCOORD_SHADER_SOURCE_H__
#define __MGN_TERRAIN_POSITION_SHADE_TEXCOORD_SHADER_SOURCE_H__

static const char* kPositionShadeTexcoordVertexShaderSource = "        \r\n\
// === Vertex shader prerequisite ===                                  \r\n\
#ifndef GL_ES                                                          \r\n\
#define highp                                                          \r\n\
#define mediump                                                        \r\n\
#define lowp                                                           \r\n\
#endif                                                                 \r\n\
// ====================================                                \r\n\
                                                                       \r\n\
attribute vec3 a_position;                                             \r\n\
attribute float a_shade;                                               \r\n\
attribute vec2 a_texcoord;                                             \r\n\
                                                                       \r\n\
uniform mat4 u_projection_view;                                        \r\n\
uniform mat4 u_model;                                                  \r\n\
                                                                       \r\n\
varying float v_shade;                                                 \r\n\
varying vec2 v_texcoord;                                               \r\n\
                                                                       \r\n\
void main()                                                            \r\n\
{                                                                      \r\n\
    v_shade = a_shade;                                                 \r\n\
    v_texcoord = a_texcoord;                                           \r\n\
    gl_Position = u_projection_view * u_model * vec4(a_position, 1.0); \r\n\
}";

static const char* kPositionShadeTexcoordFragmentShaderSource = "\r\n\
// === Fragment shader prerequisite ===                          \r\n\
#ifndef GL_ES                                                    \r\n\
#define highp                                                    \r\n\
#define mediump                                                  \r\n\
#define lowp                                                     \r\n\
#else                                                            \r\n\
precision highp float;                                           \r\n\
#endif                                                           \r\n\
// ====================================                          \r\n\
                                                                 \r\n\
uniform sampler2D u_texture;                                     \r\n\
                                                                 \r\n\
varying float v_shade;                                           \r\n\
varying vec2 v_texcoord;                                         \r\n\
                                                                 \r\n\
void main()                                                      \r\n\
{                                                                \r\n\
    vec4 sample = texture2D(u_texture, v_texcoord);              \r\n\
    gl_FragColor = vec4(sample.xyz * v_shade, 1.0);              \r\n\
}";

#endif