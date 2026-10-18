cbuffer FrameConstants : register(b0) {
    float elapsedSeconds;
    float aspectRatio;
    float2 padding;
};

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 p = (uv - 0.5) * float2(aspectRatio, 1.0);
    float wave = 0.5 + 0.5 * sin(p.x * 3.0 + p.y * 2.0 + elapsedSeconds * 0.65);
    float glow = exp(-dot(p, p) * 2.0);
    float3 color = lerp(float3(0.025, 0.05, 0.16), float3(0.08, 0.40, 0.56), wave);
    color += float3(0.15, 0.07, 0.20) * glow;
    // Output SDR/sRGB from linear procedural color.
    color = lerp(12.92 * color, 1.055 * pow(color, 1.0 / 2.4) - 0.055, step(0.0031308, color));
    return float4(saturate(color), 1.0);
}
