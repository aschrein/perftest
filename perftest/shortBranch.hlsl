#define LONG_BRANCH 0
Texture2D<float4> sourceData1 : register(t0);
Texture2D<float4> sourceData2 : register(t1);
#include "branchesBody.hlsli"