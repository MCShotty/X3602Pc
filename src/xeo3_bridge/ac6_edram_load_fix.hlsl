cbuffer Parameters : register(b0)
{
    uint4 sourceRect;
    uint4 destinationRect;
    uint2 sourceSize;
    uint2 destinationSize;
    uint sampleScale;
    uint tileBase;
    uint reserved;
    uint tilePitch;
};

RWStructuredBuffer<uint> sourceBuffer : register(u0);

uint4 main(float4 position : SV_Position) : SV_Target
{
    const uint x = uint(position.x);
    const uint y = uint(position.y);
    const uint scaledX = x / sampleScale;
    const uint scaledY = y / sampleScale;
    const uint sampleX = x % sampleScale;
    const uint sampleY = y % sampleScale;
    const uint sampleIndex = sampleY * sampleScale + sampleX;

    const uint tileX = scaledX % 80;
    const uint tileY = scaledY & 15;
    const uint macroX = scaledX / 80;
    const uint macroY = scaledY >> 4;
    const uint row = (macroY * tilePitch + tileBase + macroX) & 2047;
    const uint index = row * 1280 +
                       (sampleIndex * 2621440 | (tileY * 80 + tileX));
    const uint packed = sourceBuffer[index];

    return uint4(
        packed & 0xFF,
        (packed >> 8) & 0xFF,
        (packed >> 16) & 0xFF,
        (packed >> 24) & 0xFF);
}
