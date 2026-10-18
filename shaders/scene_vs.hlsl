#include "scene_constants.hlsli"

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float brightness : TEXCOORD1;
};

float hashValue(uint value) {
    value = (value ^ 61u) ^ (value >> 16);
    value *= 9u;
    value ^= value >> 4;
    value *= 0x27d4eb2du;
    value ^= value >> 15;
    return float(value & 0x00ffffffu) / 16777216.0;
}

VertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {
    const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        float2(-1, 1), float2(1, -1), float2(1, 1)
    };
    float2 corner = corners[vertexId];
    float2 local = corner;
    float2 uv = corner * 0.5 + 0.5;
    float brightness = 1.0;
    if (flags.x > 1.5) {
        float2 seed = float2(hashValue(instanceId * 2u + 1u), hashValue(instanceId * 2u + 2u));
        float t = axisYTime.z * parameters.y;
        float2 drift = float2(0, t * 0.035);
        if (parameters.w > 0.5 && parameters.w < 1.5) drift = float2(-t * 0.15, t * 0.8);
        if (parameters.w > 1.5) drift = float2(sin(t + seed.y * 6.283) * 0.025, t * 0.1);
        float2 particleCenter = frac(seed + drift) * 2.0 - 1.0;
        float2 shape = float2(1, parameters.w > 0.5 && parameters.w < 1.5 ? 6 : 1);
        local = particleCenter + corner * parameters.z * flags.zw * shape;
        brightness = 0.6 + 0.4 * sin(t + seed.x * 6.283);
    } else {
        uv = uvRect.xy + uv * uvRect.zw;
    }
    VertexOutput output;
    output.position = float4(centerAxisX.xy + centerAxisX.zw * local.x + axisYTime.xy * local.y, 0, 1);
    output.uv = uv;
    output.brightness = brightness;
    return output;
}
