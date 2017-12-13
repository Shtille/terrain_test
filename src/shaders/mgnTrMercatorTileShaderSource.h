#pragma once
#ifndef __MGN_TERRAIN_MERCATOR_TILE_SHADER_SOURCE_H__
#define __MGN_TERRAIN_MERCATOR_TILE_SHADER_SOURCE_H__

static const char* kMercatorTileVertexShaderSource = "       \r\n\
// === Vertex shader prerequisite ===                        \r\n\
#ifndef GL_ES                                                \r\n\
#define highp                                                \r\n\
#define mediump                                              \r\n\
#define lowp                                                 \r\n\
#endif                                                       \r\n\
// ====================================                      \r\n\
                                                             \r\n\
attribute vec3 a_position;                                   \r\n\
                                                             \r\n\
uniform mat4 u_projection_view_model;                        \r\n\
uniform float u_map_size_max;                                \r\n\
                                                             \r\n\
uniform vec4  u_stuv_scale;                                  \r\n\
uniform vec4  u_stuv_position;                               \r\n\
//uniform float u_planet_height;                             \r\n\
uniform float u_skirt_height;                                \r\n\
                                                             \r\n\
varying vec2 v_texcoord;                                     \r\n\
                                                             \r\n\
vec3 QuadPointToPixelPoint(vec2 quad_point, float height, float msm)\r\n\
{                                                                   \r\n\
    return vec3(                                                    \r\n\
    (quad_point.x + 1.0) * 0.5 * msm,                               \r\n\
    height,                                                         \r\n\
    (1.0 - (quad_point.y + 1.0) * 0.5) * msm                        \r\n\
    );                                                              \r\n\
}                                                                   \r\n\
                                                                    \r\n\
void main()                                                         \r\n\
{                                                                                        \r\n\
    // Vector is laid out as (s, t, u, v)                                                \r\n\
    vec4 stuv_point = a_position.xyxy * u_stuv_scale + u_stuv_position;                  \r\n\
                                                                                         \r\n\
    float height = 0.0;//u_planet_height * texture2DLod(heightMap, stuv_point.zw, 0.0).x;\r\n\
    float skirt_height = a_position.z * u_skirt_height;                                  \r\n\
    vec3 pixel_point = QuadPointToPixelPoint(                                            \r\n\
        stuv_point.xy, height + skirt_height, u_map_size_max);                           \r\n\
                                                                                         \r\n\
    gl_Position = u_projection_view_model * vec4(pixel_point, 1.0);                      \r\n\
                                                                                         \r\n\
    v_texcoord = stuv_point.zw;                                                          \r\n\
}";

static const char* kMercatorTileFragmentShaderSource = "      \r\n\
// === Fragment shader prerequisite ===                       \r\n\
#ifndef GL_ES                                                 \r\n\
#define highp                                                 \r\n\
#define mediump                                               \r\n\
#define lowp                                                  \r\n\
#else                                                         \r\n\
precision highp float;                                        \r\n\
#endif                                                        \r\n\
// ====================================                       \r\n\
                                                              \r\n\
uniform sampler2D u_texture;                                  \r\n\
                                                              \r\n\
varying vec2 v_texcoord;                                      \r\n\
                                                              \r\n\
void main()                                                   \r\n\
{                                                             \r\n\
    gl_FragColor = texture2D(u_texture, v_texcoord);          \r\n\
}";

#endif