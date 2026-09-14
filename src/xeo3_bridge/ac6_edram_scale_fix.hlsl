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

    // XeO3 stores the four D3D12 MSAA samples in a hardware-specific 2x2
    // swizzle. Keep its original addressing and only repair the color unpack.
    uint evenX = scaledCoordinate.x & ~1u;
    uint doubledXParity =
        (2u * (scaledCoordinate.x - evenX)) & 0x01FFFFFEu;
    uint doubledY = scaledCoordinate.y << 1u;
    uint sampleX = doubledXParity + (sampleIndex >> 1u);
    uint sampleY = (doubledY & 2u) | (sampleIndex & 1u);
    if ((sampleX - 1u) < 2u)
    {
        sampleX ^= 3u;
    }
    if ((sampleY - 1u) < 2u)
    {
        sampleY ^= 3u;
    }

    uint tileX = scaledCoordinate.x / 40u;
    uint tileY = scaledCoordinate.y >> 3u;
    uint withinTileX = evenX % 40u;
    uint withinTileY = (sampleY & 3u) | (doubledY & 12u);
    uint pitch = edramPitchTiles & 0x00FFFFFFu;
    uint tile =
        (tileY * pitch + edramBaseTiles + tileX) & 2047u;

    return tile * 1280u +
           2u * (withinTileY * 40u + withinTileX) + sampleX +
           resolutionSamplePlane;
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
