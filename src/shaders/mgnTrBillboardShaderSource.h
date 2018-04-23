#pragma once
#ifndef __MGN_TERRAIN_BILLBOARD_SHADER_SOURCE_H__
#define __MGN_TERRAIN_BILLBOARD_SHADER_SOURCE_H__

static const char* kBillboardVertexShaderSource = "          \r\n\
// === Vertex shader prerequisite ===                        \r\n\
#ifndef GL_ES                                                \r\n\
#define highp                                                \r\n\
#define mediump                                              \r\n\
#define lowp                                                 \r\n\
#endif                                                       \r\n\
// ====================================                      \r\n\
                                                             \r\n\
attribute vec3 a_position;                                   \r\n\
attribute vec2 a_texcoord;                                   \r\n\
                                                             \r\n\
uniform mat4 u_projection;                                   \r\n\
uniform mat4 u_view;                                         \r\n\
uniform mat4 u_model;                                        \r\n\
                                                             \r\n\
varying vec2 v_texcoord;                                     \r\n\
varying vec3 v_view_position;                                \r\n\
                                                             \r\n\
void main()                                                  \r\n\
{                                                            \r\n\
    v_texcoord = a_texcoord;                                 \r\n\
    vec4 view_pos = u_view * u_model * vec4(a_position, 1.0);\r\n\
    v_view_position = view_pos.xyz;                          \r\n\
    gl_Position = u_projection * view_pos;                   \r\n\
}";

static const char* kBillboardFragmentShaderSource = "                                                      \r\n\
// === Fragment shader prerequisite ===                                                                    \r\n\
#ifndef GL_ES                                                                                              \r\n\
#define highp                                                                                              \r\n\
#define mediump                                                                                            \r\n\
#define lowp                                                                                               \r\n\
#else                                                                                                      \r\n\
precision highp float;                                                                                     \r\n\
#endif                                                                                                     \r\n\
// ====================================                                                                    \r\n\
                                                                                                           \r\n\
uniform sampler2D u_texture;                                                                               \r\n\
                                                                                                           \r\n\
// Fog parameters                                                                                          \r\n\
uniform float u_fog_modifier;                                                                              \r\n\
uniform float u_z_far;                                                                                     \r\n\
                                                                                                           \r\n\
struct OccludeeParams {                                                                                    \r\n\
    vec2 center; // screen center                                                                          \r\n\
    float radius; // screen radius                                                                         \r\n\
    float distance;                                                                                        \r\n\
    float size;                                                                                            \r\n\
};                                                                                                         \r\n\
                                                                                                           \r\n\
// Occlusion parameters                                                                                    \r\n\
uniform bool u_occlusion_enabled;                                                                          \r\n\
uniform float u_occlusion_distance; // distance to occluder                                                \r\n\
uniform OccludeeParams u_occludee_params; // (center.x, center.y, radius, depth)                           \r\n\
                                                                                                           \r\n\
varying vec2 v_texcoord;                                                                                   \r\n\
varying vec3 v_view_position;                                                                              \r\n\
                                                                                                           \r\n\
void main()                                                                                                \r\n\
{                                                                                                          \r\n\
    vec4 color = texture2D(u_texture, v_texcoord);                                                         \r\n\
    float view_length = length(v_view_position);                                                           \r\n\
    float fog_distance = view_length / u_z_far;                                                            \r\n\
    const float kFogBegin = 0.6;                                                                           \r\n\
    const float kFogEnd   = 0.8;                                                                           \r\n\
    float fog_factor = clamp((fog_distance - kFogBegin) / (kFogEnd - kFogBegin), 0.0, 1.0);                \r\n\
    fog_factor = fog_factor * u_fog_modifier;                                                              \r\n\
    const vec4 fog_color = vec4(0.5, 0.6, 0.8, 1.0);                                                       \r\n\
    color = mix(color, fog_color, fog_factor);                                                             \r\n\
    if (u_occlusion_enabled)                                                                               \r\n\
    {                                                                                                      \r\n\
        bool is_occluding = distance(gl_FragCoord.xy, u_occludee_params.center) < u_occludee_params.radius;\r\n\
        if (is_occluding)                                                                                  \r\n\
        {                                                                                                  \r\n\
            if (abs(u_occlusion_distance - u_occludee_params.distance) < u_occludee_params.size)           \r\n\
                color.a *= 0.2;                                                                            \r\n\
        }                                                                                                  \r\n\
    }                                                                                                      \r\n\
    gl_FragColor = color;                                                                                  \r\n\
}";

#endif