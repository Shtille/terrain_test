#pragma once
#ifndef __MGN_TERRAIN_POSITION_SHADER_SOURCE_H__
#define __MGN_TERRAIN_POSITION_SHADER_SOURCE_H__

static const char* kPositionVertexShaderSource = "            \r\n\
// === Vertex shader prerequisite ===                         \r\n\
#ifndef GL_ES                                                 \r\n\
#define highp                                                 \r\n\
#define mediump                                               \r\n\
#define lowp                                                  \r\n\
#endif                                                        \r\n\
// ====================================                       \r\n\
                                                              \r\n\
attribute vec3 a_position;                                    \r\n\
                                                              \r\n\
uniform mat4 u_projection;                                    \r\n\
uniform mat4 u_view;                                          \r\n\
uniform mat4 u_model;                                         \r\n\
                                                              \r\n\
varying vec3 v_view_position;                                 \r\n\
                                                              \r\n\
void main()                                                   \r\n\
{                                                             \r\n\
    vec4 view_pos = u_view * u_model * vec4(a_position, 1.0); \r\n\
    v_view_position = view_pos.xyz;                           \r\n\
    gl_Position = u_projection * view_pos;                    \r\n\
}";

static const char* kPositionFragmentShaderSource = "                                             \r\n\
// === Fragment shader prerequisite ===                                                          \r\n\
#ifndef GL_ES                                                                                    \r\n\
#define highp                                                                                    \r\n\
#define mediump                                                                                  \r\n\
#define lowp                                                                                     \r\n\
#else                                                                                            \r\n\
precision highp float;                                                                           \r\n\
#endif                                                                                           \r\n\
// ====================================                                                          \r\n\
                                                                                                 \r\n\
uniform vec4 u_color;                                                                            \r\n\
                                                                                                 \r\n\
// Fog parameters                                                                                \r\n\
uniform float u_fog_modifier;                                                                    \r\n\
uniform float u_z_far;                                                                           \r\n\
                                                                                                 \r\n\
varying vec3 v_view_position;                                                                    \r\n\
                                                                                                 \r\n\
void main()                                                                                      \r\n\
{                                                                                                \r\n\
    vec4 color = u_color;                                                                        \r\n\
    float view_length = length(v_view_position);                                                 \r\n\
    float fog_distance = view_length / u_z_far;                                                  \r\n\
    const float kFogBegin = 0.6;                                                                 \r\n\
    const float kFogEnd   = 0.8;                                                                 \r\n\
    float fog_factor = clamp((fog_distance - kFogBegin) / (kFogEnd - kFogBegin), 0.0, 1.0);      \r\n\
    fog_factor = fog_factor * u_fog_modifier;                                                    \r\n\
    const vec4 fog_color = vec4(0.5, 0.6, 0.8, 1.0);                                             \r\n\
    color = mix(color, fog_color, fog_factor);                                                   \r\n\
    gl_FragColor = color;                                                                        \r\n\
}";

#endif