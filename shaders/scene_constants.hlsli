cbuffer LayerConstants : register(b0) {
    float4 centerAxisX; // center.xy, horizontal half-axis.xy in NDC
    float4 axisYTime;   // vertical half-axis.xy, seconds, canvas aspect ratio
    float4 uvRect;
    float4 tint;
    float4 colorA;
    float4 colorB;
    float4 parameters; // opacity, speed, particle size, particle preset
    float4 flags;      // layer type, pulse, inverse canvas width/height
};
