cbuffer TaskData : register(b1)
{
    uint4 TaskRegisters[36];
};

RWTexture2D<uint> OutputTexture : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint width = 1280;
    const uint height = 720;
    const uint linearIndex = dispatchThreadId.x;
    if (linearIndex >= width * height)
    {
        return;
    }

    const uint2 position =
        uint2(linearIndex % width, linearIndex / width);
    OutputTexture[position] = TaskRegisters[1].x;
}
