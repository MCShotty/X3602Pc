#define VgpuAssertBufferSlot u0
#define MemExportByteBufferSlot u0
#define MemExportDwordBufferSlot u1
#define EdramMirrorBuffer u0
#define EdramMirrorExtraZBuffer u1

cbuffer renderstatepatch : register(b0)
{
    float c_bias : packoffset(c9.x);
    float4 tfpatch[32] : packoffset(c10);
    float2 vpos_Scale : packoffset(c48.y);
};

cbuffer cbloop : register(b1)
{
    uint4 loopConst[32];
};

cbuffer cbbool : register(b2)
{
    uint4 boolConst[2];
};

cbuffer cbTexValueInfo : register(b3)
{
    float4 TexValueInfo[16];
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

Buffer<float4> cstackps : register(t32);
Buffer<float4> cstackvs : register(t33);
Buffer<uint> cind : register(t34);

static const float2 vport_Scale = float2(1.0f, 1.0f);
static const float2 vport_Offset = float2(0.0f, 0.0f);
static const float alpha_ref = 0.0f;
static const float psz_size = 1.0f;
static const float psz_min = 1.0f;
static const float psz_max = 1.0f;
static const uint vport = 0u;
static const uint xe_vport = 0u;
static const uint AlphaToMaskRef = 0u;
static const uint MsaaLevel = 0u;
