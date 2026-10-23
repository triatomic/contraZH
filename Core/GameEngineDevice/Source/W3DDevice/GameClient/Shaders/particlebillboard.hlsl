// These registers match DX8VertexShadingClass.
float4 ProjectionColumns[4] : register(c0);
float4 ViewRows[3] : register(c4);
float4 Fog : register(c12);
float4 StageSource[4] : register(c24);
float4 StageColumns[16] : register(c28);

struct VsIn
{
    float3 Corner : POSITION;
    float2 UV : TEXCOORD0;
    float4 CenterSize : TEXCOORD4;
    float2 Rotation : TEXCOORD5;
    float4 Diffuse : COLOR0;
    float4 UVRect : TEXCOORD6;
    float Triangle : TEXCOORD7;
};

struct VsOut
{
    float4 Position : POSITION;
    float4 Diffuse : COLOR0;
    float4 Specular : COLOR1;
    float4 TexCoord0 : TEXCOORD0;
    float4 TexCoord1 : TEXCOORD1;
    float4 TexCoord2 : TEXCOORD2;
    float4 TexCoord3 : TEXCOORD3;
    float Fog : FOG;
};

float4 StageCoord(int stage, float4 uv, float4 view)
{
    float4 source = (StageSource[stage].x + StageSource[stage].y) * uv +
        StageSource[stage].w * view;
    return float4(dot(source, StageColumns[stage * 4]), dot(source, StageColumns[stage * 4 + 1]),
        dot(source, StageColumns[stage * 4 + 2]), dot(source, StageColumns[stage * 4 + 3]));
}

VsOut main(VsIn input)
{
    float4 center = float4(input.CenterSize.xyz, 1.0f);
    float3 viewCenter = float3(dot(center, ViewRows[0]), dot(center, ViewRows[1]), dot(center, ViewRows[2]));
    float2 offset = input.Corner.xy * (1.0f - 2.0f * input.Triangle);
    float2 corner = float2(offset.x * input.Rotation.x - offset.y * input.Rotation.y,
        offset.x * input.Rotation.y + offset.y * input.Rotation.x);
    float4 view = float4(viewCenter + float3(corner * input.CenterSize.w, 0.0f), 1.0f);
    float2 cornerUV = lerp(input.UV, 1.0f - input.UV, input.Triangle);
    float4 uv = float4(lerp(input.UVRect.xy, input.UVRect.zw, cornerUV), 1.0f, 0.0f);

    VsOut output;
    output.Position = float4(dot(view, ProjectionColumns[0]), dot(view, ProjectionColumns[1]),
        dot(view, ProjectionColumns[2]), dot(view, ProjectionColumns[3]));
    output.Diffuse = input.Diffuse;
    output.Specular = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.TexCoord0 = StageCoord(0, uv, view);
    output.TexCoord1 = StageCoord(1, uv, view);
    output.TexCoord2 = StageCoord(2, uv, view);
    output.TexCoord3 = StageCoord(3, uv, view);
    output.Fog = saturate(Fog.x - lerp(view.z, length(view.xyz), Fog.z) * Fog.y);
    return output;
}
