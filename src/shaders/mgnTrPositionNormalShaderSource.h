#pragma once
#ifndef __MGN_TERRAIN_POSITION_NORMAL_SHADER_SOURCE_H__
#define __MGN_TERRAIN_POSITION_NORMAL_SHADER_SOURCE_H__

static const char* kPositionNormalVertexShaderSource = "     \r\n\
// === Vertex shader prerequisite ===                        \r\n\
#ifndef GL_ES                                                \r\n\
#define highp                                                \r\n\
#define mediump                                              \r\n\
#define lowp                                                 \r\n\
#endif                                                       \r\n\
// ====================================                      \r\n\
                                                             \r\n\
attribute vec3 a_position;                                   \r\n\
attribute vec3 a_normal;                                     \r\n\
                                                             \r\n\
uniform mat4 u_projection;                                   \r\n\
uniform mat4 u_view;                                         \r\n\
uniform mat4 u_model;                                        \r\n\
                                                             \r\n\
varying vec3 v_normal;                                       \r\n\
varying vec3 v_view_position;                                \r\n\
                                                             \r\n\
void main()                                                  \r\n\
{                                                            \r\n\
    v_normal = a_normal;                                     \r\n\
    vec4 view_pos = u_view * u_model * vec4(a_position, 1.0);\r\n\
    v_view_position = view_pos.xyz;                          \r\n\
    gl_Position = u_projection * view_pos;                   \r\n\
}";

static const char* kPositionNormalFragmentShaderSource = "                                        \r\n\
// === Fragment shader prerequisite ===                                                           \r\n\
#ifndef GL_ES                                                                                     \r\n\
#define highp                                                                                     \r\n\
#define mediump                                                                                   \r\n\
#define lowp                                                                                      \r\n\
#else                                                                                             \r\n\
precision highp float;                                                                            \r\n\
#endif                                                                                            \r\n\
// ====================================                                                           \r\n\
                                                                                                  \r\n\
uniform vec3 u_color;                                                                             \r\n\
                                                                                                  \r\n\
// Fog parameters                                                                                 \r\n\
uniform float u_fog_modifier;                                                                     \r\n\
uniform float u_z_far;                                                                            \r\n\
                                                                                                  \r\n\
varying vec3 v_normal;                                                                            \r\n\
varying vec3 v_view_position;                                                                     \r\n\
                                                                                                  \r\n\
void main()                                                                                       \r\n\
{                                                                                                 \r\n\
    const float kAmbientFactor = 0.2;                                                             \r\n\
    const float kDiffuseFactor = 0.8;                                                             \r\n\
    const vec3 light = vec3(0.0, 1.0, 0.0);                                                       \r\n\
    vec3 normal = normalize(v_normal);                                                            \r\n\
    float diffuse = max(dot(normal, light), 0.0);                                                 \r\n\
    vec4 color = vec4(u_color * (kAmbientFactor + kDiffuseFactor * diffuse), 1.0);                \r\n\
    float view_length = length(v_view_position);                                                  \r\n\
    float fog_distance = view_length / u_z_far;                                                   \r\n\
    const float kFogBegin = 0.6;                                                                  \r\n\
    const float kFogEnd   = 0.8;                                                                  \r\n\
    float fog_factor = clamp((fog_distance - kFogBegin) / (kFogEnd - kFogBegin), 0.0, 1.0);       \r\n\
    fog_factor = fog_factor * u_fog_modifier;                                                     \r\n\
    const vec4 fog_color = vec4(0.5, 0.6, 0.8, 1.0);                                              \r\n\
    color = mix(color, fog_color, fog_factor);                                                    \r\n\
    gl_FragColor = color;                                                                         \r\n\
}";

#endif