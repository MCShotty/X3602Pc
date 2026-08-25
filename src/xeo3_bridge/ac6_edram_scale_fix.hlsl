cbuffer RestoreParameters : register(b0)
{
    int4 sourceRectangle;
    int4 destinationRectangle;
    int2 sourceDimensions;
    int2 destinationDimensions;
    uint resolutionDivisor;
    uint edramBaseTiles;
    uint sampleIndex;
    uint edramPitchTiles;
};

RWStructuredBuffer<uint> edram : register(u0);

struct PixelInput
{
    float4 position : SV_Position;
};

uint GetEdramAddress(uint2 coordinate)
{
    uint divisor = max(resolutionDivisor, 1u);
    uint2 scaledCoordinate = coordinate / divisor;
    uint2 resolutionSample = coordinate % divisor;
    uint resolutionSamplePlane =
        (resolutionSample.y * divisor + resolutionSample.x) * 2621440u;

    uint evenX = scaledCoordinate.x & ~1u;
    uint xWithinPair = ((scaledCoordinate.x - evenX) << 1u) +
                       (sampleIndex >> 1u);
    uint yWithinPair = ((scaledCoordinate.y << 1u) & 2u) |
                       (sampleIndex & 1u);
    if ((xWithinPair - 1u) < 2u)
    {
        xWithinPair ^= 3u;
    }
    if ((yWithinPair - 1u) < 2u)
    {
        yWithinPair ^= 3u;
    }

    uint tileX = evenX % 40u;
    uint tileY = ((scaledCoordinate.y << 1u) & 12u) |
                 (yWithinPair & 3u);
    uint tileColumn = scaledCoordinate.x / 40u;
    uint tileRow = (scaledCoordinate.y >> 3u) & 0x00FFFFFFu;
    uint pitch = edramPitchTiles & 0x00FFFFFFu;
    uint tile = (tileRow * pitch + edramBaseTiles + tileColumn) & 2047u;

    return tile * 1280u +
           ((tileY * 40u + tileX) << 1u) +
           xWithinPair + resolutionSamplePlane;
}

uint4 main(PixelInput input) : SV_Target0
{
    uint packed = edram[GetEdramAddress(uint2(input.position.xy))];
    return uint4(
        packed & 0xFFu,
        (packed >> 8u) & 0xFFu,
        (packed >> 16u) & 0xFFu,
        (packed >> 24u) & 0xFFu);
}
