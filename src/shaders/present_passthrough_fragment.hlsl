cbuffer Constants : register(b0, space3)
{
    float4 grade0;
    float4 tint_rgb;
    float4 flash_rgb;
};

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    float4 rgba = u_texture.Sample(u_sampler, input.v_uv);
    float3 rgb = rgba.rgb * grade0.x;
    rgb = lerp(rgb, rgb * tint_rgb.rgb, grade0.y);
    rgb += flash_rgb.rgb * grade0.z;
    output.o_color = float4(rgb, rgba.a) * input.v_color;
    return output;
}
