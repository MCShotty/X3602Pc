Texture2DMS<uint4, 2> sourceTexture : register(t0);

uint4 main(float4 position : SV_Position) : SV_Target
{
    const uint2 sourcePosition = uint2(uint(position.x) >> 1, uint(position.y));
    uint4 resolved = sourceTexture.Load(sourcePosition, 0);
    resolved += sourceTexture.Load(sourcePosition, 1);
    return (resolved + 1) >> 1;
}
