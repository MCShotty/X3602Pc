#define VgpuAssertBufferSlot u0
#define MemExportByteBufferSlot u0
#define MemExportDwordBufferSlot u1
#define EdramMirrorBuffer u0
#define EdramMirrorExtraZBuffer u1

cbuffer cbloop : register(b0)
{
    uint4 loopConst[32];
};

cbuffer cbbool : register(b1)
{
    uint4 boolConst[2];
};

cbuffer cbTexValueInfo : register(b2)
{
    float4 TexValueInfo[16];
};

cbuffer renderstatepatch : register(b3)
{
    float2 vport_Scale;
    float2 vport_Offset;
    float alpha_ref;
    float psz_size;
    float psz_min;
    float psz_max;
    float c_bias;
    uint vport;
    uint xe_vport;
    float2 vpos_Scale;
    uint AlphaToMaskRef;
    uint MsaaLevel;
};

cbuffer bufferstatepatch : register(b5)
{
    int vertexOffset;
    uint UseIndexBuf;
    uint IndexCount;
    uint vfetchEndianness;
    uint PackedIbDesc;
    uint ResetIndex;
    uint IbBase;
};

Buffer<float4> cstackvs : register(t33);
