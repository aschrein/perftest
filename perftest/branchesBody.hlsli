#include "hash.hlsli"
#include "loadConstantsGPU.h"

RWBuffer<float> output : register(u0);

cbuffer CB0 : register(b0)
{
	LoadConstants loadConstants;
};

#define THREAD_GROUP_DIM 32

groupshared float dummyLDS[THREAD_GROUP_DIM][THREAD_GROUP_DIM];

int ALUPayload(float input1, float input2)
{
	[loop]
	for (int i = 0; i < loadConstants.readStartAddress; i++)
	{
		input1 = input2 * 0.555f + input1 * input2 + input1 * 0.9f + input1 * input1;
	}
	return input1;
}

#define ADDTERM 1.0f
//dot(s1, s2)

[numthreads(THREAD_GROUP_DIM, THREAD_GROUP_DIM, 1)]
void main(uint3 tid : SV_DispatchThreadID, uint3 gid : SV_GroupThreadID, uint3 wid : SV_GroupID)
{
	float4 value = 0.0;

	uint2 htid = gid.xy + wid.xy * 32;

	[loop]
	for (int y = 0; y < 1; ++y)
	{
		[loop]
		for (int x = 0; x < 1; ++x)
		{
			// Mask with runtime constant to prevent unwanted compiler optimizations
			uint2 elemIdx = (htid + uint2(x, y)) | loadConstants.elementsMask;
			// Random access to the second texture to increase the cost
			uint2 elemIdx2 = uint2((hash1(elemIdx.x) & 0x3ff), (hash1(elemIdx.y) & 0x3ff)) | loadConstants.elementsMask;
			elemIdx.x %= 1024;
			elemIdx.y %= 1024;
			float4 s1 = sourceData1[elemIdx].xyzw;
			float4 s2 = 0.0f;
#if BASE_BRANCH == 1
			// Baseline
			s2 = sourceData2[elemIdx2].xyzw;
			value.x += ALUPayload(value.x + ADDTERM, value.x);
#elif LONG_BRANCH == 1
			// Here we put a branch and sink payload into both paths
			if (dot(s1, s1) != 0.0f)
			{
				s2 = sourceData2[elemIdx2].xyzw;
				value.x += ALUPayload(value.x + ADDTERM, value.x);
			}
			else
			{
				value.x += ALUPayload(value.x + ADDTERM, value.x);
			}

#else
			// Here we put a branch only for the sample instruction
			if (dot(s1, s1) != 0.0f)
			{
				s2 = sourceData2[elemIdx2].xyzw;
			}
			value.x += ALUPayload(value.x + ADDTERM, value.x);
#endif

			value.x += dot(s1, s2);
		}
	}
	// Linear write to LDS (no bank conflicts). Significantly faster than memory loads.
	dummyLDS[gid.y][gid.x] = value.x + value.y + value.z + value.w;

	GroupMemoryBarrierWithGroupSync();

	// This branch is never taken, but the compiler doesn't know it
	// Optimizer would remove all the memory loads if the data wouldn't be potentially used
	[branch]
	if (loadConstants.writeIndex != 0xffffffff)
	{
		output[tid.x + tid.y] = dummyLDS[(loadConstants.writeIndex >> 8) & 0xff][loadConstants.writeIndex & 0xff];
	}
}
