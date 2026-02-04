#include "object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 baseColor = gMaterial.color.rgb;

    // 強めのネオン強度
    float intensity = 2.5f;

    // 疑似発光（常に乗せる）
    float3 glow = baseColor * 1.0f;

    float3 color = baseColor * intensity + glow;

    // トーンマップ
    color = color / (color + 1.0f);

    output.color = float4(color, gMaterial.color.a);
    return output;
}



