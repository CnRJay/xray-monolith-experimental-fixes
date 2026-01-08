#include "common.h"

// Rolling Structured Buffer for grass instancing
// Instance data structure (must match C++ GrassInstanceData)
struct GrassInstanceData
{
    float4 mat0;    // Matrix row 0: _11*scale, _21*scale, _31*scale, _41 (pos.x)
    float4 mat1;    // Matrix row 1: _12*scale, _22*scale, _32*scale, _42 (pos.y)
    float4 mat2;    // Matrix row 2: _13*scale, _23*scale, _33*scale, _43 (pos.z)
    float4 color;   // sun, sun, sun, hemi
    float4 params;  // normal.x, normal.y, normal.z, alpha
};

// Structured buffer bound to slot t15
StructuredBuffer<GrassInstanceData> g_GrassInstances : register(t15);

// Ring buffer offset (passed as float, cast to uint)
float4 c_buffer_offset;

// Standard detail constants
float4 consts;      // scale.xy, l_aniso, l_ambient
float4 wave;        // wave params
float4 dir2D;       // wind direction
float4x4 xform;     // view-projection matrix

// Vertex input (matches vertHW_Instanced struct)
struct v_detail
{
    float3 pos : POSITION;
    int4   misc : TEXCOORD0;  // u, v, height_normalized, padding
};

// Vertex output
struct v2p
{
    float4 hpos     : SV_Position;
    float2 tc       : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float4 color    : COLOR0;
    float  alpha    : TEXCOORD2;
};

v2p main(v_detail v, uint InstanceID : SV_InstanceID)
{
    v2p O;
    
    // Calculate actual index in the ring buffer
    uint buffer_offset = (uint)c_buffer_offset.x;
    uint actualIndex = InstanceID + buffer_offset;
    
    // Fetch instance data from structured buffer
    GrassInstanceData inst = g_GrassInstances[actualIndex];
    
    // Reconstruct 3x4 matrix from rows
    // Matrix is stored transposed: columns become rows
    float3x4 world_matrix;
    world_matrix[0] = inst.mat0;
    world_matrix[1] = inst.mat1;
    world_matrix[2] = inst.mat2;
    
    // Transform position
    float4 local_pos = float4(v.pos, 1.0);
    float3 world_pos;
    world_pos.x = dot(world_matrix[0], local_pos);
    world_pos.y = dot(world_matrix[1], local_pos);
    world_pos.z = dot(world_matrix[2], local_pos);
    
    // Apply wave animation based on height
    float height_factor = (float)v.misc.z / 16384.0;  // Denormalize from short
    
    // Simple wind animation
    float2 wind_offset = dir2D.xz * wave.w * height_factor * height_factor;
    world_pos.xz += wind_offset;
    
    // Transform to clip space
    O.hpos = mul(xform, float4(world_pos, 1.0));
    
    // Texture coordinates (denormalize from short)
    O.tc.x = (float)v.misc.x / 16384.0;
    O.tc.y = (float)v.misc.y / 16384.0;
    
    // Pass normal and color
    O.normal = inst.params.xyz;
    O.color = inst.color;
    O.alpha = inst.params.w;
    
    return O;
}
