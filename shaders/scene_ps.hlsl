#include "scene_constants.hlsli"

Texture2D imageTexture : register(t0);
SamplerState imageSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0,
            float brightness : TEXCOORD1) : SV_TARGET {
    float4 color;
    if (flags.x < 0.5) {
        float2 p = (uv - 0.5) * float2(axisYTime.w, 1.0);
        float wave = 0.5 + 0.5 * sin(p.x * 3.0 + p.y * 2.0 + axisYTime.z * parameters.y);
        color = lerp(colorA, colorB, wave);
        color.rgb += float3(0.15, 0.07, 0.20) * exp(-dot(p, p) * 2.0);
    } else if (flags.x < 1.5) {
        color = imageTexture.Sample(imageSampler, uv);
    } else {
        float2 q = uv * 2.0 - 1.0;
        float coverage = 1.0 - smoothstep(0.25, 1.0, dot(q, q));
        color = float4(brightness.xxx, coverage);
    }
    color *= tint;
    color.a *= parameters.x;
    if (flags.y > 0.5) color.a *= 0.65 + 0.35 * sin(axisYTime.z * 2.0);
    return color; // Linear blending; the sRGB RTV encodes output for presentation.
}
