# PerfTest

A simple GPU shader memory operation performance test tool. Current implementation is DirectX 11.0 based.

The purpose of this application is not to benchmark different brand GPUs against each other. Its purpose is to help rendering programmers to choose right types of resources when optimizing their compute shader performance.

This application is designed to measure peak data load performance from L1 caches. I tried to avoid known hardware bottlenecks. **If you notice something wrong or suspicious in the shader workload, please inform me immediately and I will fix it.** If my shaders are affected by some hardware bottlenecks, I am glad to hear about it and write more test cases to show the best performance. The goal is that developers gain better understanding of various GPU hardware on the market and gain insight to optimize code for them.

## Features

Designed to measure performance of various types of buffer and image loads. This application is not a GPU memory bandwidth measurement tool. All tests operate inside GPUs L1 caches (no larger than 16 KB working sets). 

- Coalesced loads (100% L1 cache hit)
- Random loads (100% L1 cache hit)
- Uniform address loads (same address for all threads)
- Typed Buffer SRVs: 1/2/4 channels, 8/16/32 bits per channel
- ByteAddressBuffer SRVs: load, load2, load3, load4 - aligned and unaligned
- Structured Buffer SRVs: float/float2/float4
- Constant Buffer float4 array indexed loads
- Texture2D loads: 1/2/4 channels, 8/16/32 bits per channel

## Explanations

**Coalesced loads:**
GPUs optimize linear address patterns. Coalescing occurs when all threads in a warp/wave (32/64 threads) load from contiguous addresses. In my "linear" test case, memory loads access contiguous addresses in the whole thread group (256 threads). This should coalesce perfectly on all GPUs, independent of warp/wave width.

**Random loads:**
I add a random start offset of 0-15 elements for each thread (still aligned). This prevents GPU coalescing, and provides more realistic view of performance for common case (non-linear) memory accessing. This benchmark is as cache efficient as the previous. All data still comes from the L1 cache.

**Uniform loads:**
All threads in group simultaneously load from the same address. This triggers coalesced path on some GPUs and additonal optimizations on some GPUs, such as scalar loads (SGPR storage) on AMD GCN. I have noticed that recent Intel and Nvidia drivers also implement a software optimization for uniform load loop case (which is employed by this benchmark).

**Notes:**
**Compiler optimizations** can ruin the results. We want to measure only load (read) performance, but write (store) is also needed, otherwise the compiler will just optimize the whole shader away. To avoid this, each thread does first 256 loads followed by a single linear groupshared memory write (no bank-conflicts). Cbuffer contains a write mask (not known at compile time). It controls which elements are written from the groupshared memory to the output buffer. The mask is always zero at runtime. Compilers can also combine multiple narrow raw buffer loads together (as bigger 4d loads) if it an be proven at compile time that loads from the same thread access contiguous offsets. This is prevented by applying an address mask from cbuffer (not known at compile time). 

## Todo list

- UAV loads (RWBuffer, RWByteAddressBuffer)
- Port to Vulkan and/or DX12 (upload heap load performance, etc)

## Uniform Load Investigation
When I first implemented this benchmark, I noticed that Intel uniform address loads were surprisingly fast. Intel ISA documents don't mention anything about a scalar unit or other hardware feature to make uniform address loads fast. This optimization affected every single resource type, unlike AMDs hardware scalar unit (which only works for raw data loads). I didnt't investigate this further however at that point. When Nvidia released Volta GPUs, they brought new driver that implemented similar compiler optimization. Later drivers introduced the same optimization to Maxwell and Pascal too. And now Turing also has it. It's certainly not hardware based, since 20x+ gains apply to all their existing GPUs too.

In Nov 10-11 weekend (2018) I was toying around with Vulkan/DX12 wave intrinsics, and came up with a crazy idea to use a single wave wide load and then use wave intrinsics to broadcast scalar result (single lane) to each loop iteration. This results in up to wave width reduced amount of loads. 

See the gist and Shader Playground links here:
https://gist.github.com/sebbbi/ba4415339b535d22fb18e2d824564ec4

In Nvidia's uniform load optimization case, their wave width = 32, and their uniform load optimization performance boost is up to 28x. This finding really made me curious. Could Nvidia implement a similar warp shuffle based optimization for this use case? The funny thing is that my tweets escalated the situation, and made Intel reveal their hand:

https://twitter.com/JoshuaBarczak/status/1062060067334189056

Intel has now officially revealed that their driver does a wave shuffle optimization for uniform address loads. They have been doing it for years already. This explains Intel GPU benchmark results perfectly. Now that we have confirmation of Intel's (original) optimization, I suspect that Nvidia's shader compiler employs a highly similar optimization in this case. Both optimizations are great, because Nvidia/Intel do not have a dedicated scalar unit. They need to lean more on vector loads, and this trick allows sharing one vector load with multiple uniform address load loop iterations.

## Results
All results are compared to ```Buffer<RGBA8>.Load random``` result (=1.0x) on the same GPU.

### AMD GCN2 (R9 390X)
```markdown
Buffer<R8>.Load uniform: 11.302ms 3.907x
Buffer<R8>.Load linear: 11.327ms 3.899x
Buffer<R8>.Load random: 44.150ms 1.000x
Buffer<RG8>.Load uniform: 49.611ms 0.890x
Buffer<RG8>.Load linear: 49.835ms 0.886x
Buffer<RG8>.Load random: 49.615ms 0.890x
Buffer<RGBA8>.Load uniform: 44.149ms 1.000x
Buffer<RGBA8>.Load linear: 44.806ms 0.986x
Buffer<RGBA8>.Load random: 44.164ms 1.000x
Buffer<R16f>.Load uniform: 11.131ms 3.968x
Buffer<R16f>.Load linear: 11.139ms 3.965x
Buffer<R16f>.Load random: 44.076ms 1.002x
Buffer<RG16f>.Load uniform: 49.552ms 0.891x
Buffer<RG16f>.Load linear: 49.560ms 0.891x
Buffer<RG16f>.Load random: 49.559ms 0.891x
Buffer<RGBA16f>.Load uniform: 44.066ms 1.002x
Buffer<RGBA16f>.Load linear: 44.687ms 0.988x
Buffer<RGBA16f>.Load random: 44.066ms 1.002x
Buffer<R32f>.Load uniform: 11.132ms 3.967x
Buffer<R32f>.Load linear: 11.139ms 3.965x
Buffer<R32f>.Load random: 44.071ms 1.002x
Buffer<RG32f>.Load uniform: 49.558ms 0.891x
Buffer<RG32f>.Load linear: 49.560ms 0.891x
Buffer<RG32f>.Load random: 49.559ms 0.891x
Buffer<RGBA32f>.Load uniform: 44.061ms 1.002x
Buffer<RGBA32f>.Load linear: 44.613ms 0.990x
Buffer<RGBA32f>.Load random: 49.583ms 0.891x
ByteAddressBuffer.Load uniform: 10.322ms 4.278x
ByteAddressBuffer.Load linear: 11.546ms 3.825x
ByteAddressBuffer.Load random: 44.153ms 1.000x
ByteAddressBuffer.Load2 uniform: 11.499ms 3.841x
ByteAddressBuffer.Load2 linear: 49.628ms 0.890x
ByteAddressBuffer.Load2 random: 49.651ms 0.889x
ByteAddressBuffer.Load3 uniform: 16.985ms 2.600x
ByteAddressBuffer.Load3 linear: 44.142ms 1.000x
ByteAddressBuffer.Load3 random: 88.176ms 0.501x
ByteAddressBuffer.Load4 uniform: 22.472ms 1.965x
ByteAddressBuffer.Load4 linear: 44.212ms 0.999x
ByteAddressBuffer.Load4 random: 49.346ms 0.895x
ByteAddressBuffer.Load2 unaligned uniform: 11.422ms 3.867x
ByteAddressBuffer.Load2 unaligned linear: 49.552ms 0.891x
ByteAddressBuffer.Load2 unaligned random: 49.561ms 0.891x
ByteAddressBuffer.Load4 unaligned uniform: 22.373ms 1.974x
ByteAddressBuffer.Load4 unaligned linear: 44.095ms 1.002x
ByteAddressBuffer.Load4 unaligned random: 54.464ms 0.811x
StructuredBuffer<float>.Load uniform: 12.585ms 3.509x
StructuredBuffer<float>.Load linear: 11.770ms 3.752x
StructuredBuffer<float>.Load random: 44.176ms 1.000x
StructuredBuffer<float2>.Load uniform: 13.210ms 3.343x
StructuredBuffer<float2>.Load linear: 50.217ms 0.879x
StructuredBuffer<float2>.Load random: 49.645ms 0.890x
StructuredBuffer<float4>.Load uniform: 13.818ms 3.196x
StructuredBuffer<float4>.Load random: 49.666ms 0.889x
StructuredBuffer<float4>.Load linear: 44.721ms 0.988x
cbuffer{float4} load uniform: 16.702ms 2.644x
cbuffer{float4} load linear: 44.447ms 0.994x
cbuffer{float4} load random: 49.656ms 0.889x
Texture2D<R8>.Load uniform: 44.214ms 0.999x
Texture2D<R8>.Load linear: 44.795ms 0.986x
Texture2D<R8>.Load random: 44.808ms 0.986x
Texture2D<RG8>.Load uniform: 49.706ms 0.888x
Texture2D<RG8>.Load linear: 50.231ms 0.879x
Texture2D<RG8>.Load random: 50.200ms 0.880x
Texture2D<RGBA8>.Load uniform: 44.760ms 0.987x
Texture2D<RGBA8>.Load linear: 45.339ms 0.974x
Texture2D<RGBA8>.Load random: 45.405ms 0.973x
Texture2D<R16F>.Load uniform: 44.175ms 1.000x
Texture2D<R16F>.Load linear: 44.157ms 1.000x
Texture2D<R16F>.Load random: 44.096ms 1.002x
Texture2D<RG16F>.Load uniform: 49.739ms 0.888x
Texture2D<RG16F>.Load linear: 49.661ms 0.889x
Texture2D<RG16F>.Load random: 49.622ms 0.890x
Texture2D<RGBA16F>.Load uniform: 44.257ms 0.998x
Texture2D<RGBA16F>.Load linear: 44.267ms 0.998x
Texture2D<RGBA16F>.Load random: 88.126ms 0.501x
Texture2D<R32F>.Load uniform: 44.259ms 0.998x
Texture2D<R32F>.Load linear: 44.193ms 0.999x
Texture2D<R32F>.Load random: 44.099ms 1.001x
Texture2D<RG32F>.Load uniform: 49.739ms 0.888x
Texture2D<RG32F>.Load linear: 49.667ms 0.889x
Texture2D<RG32F>.Load random: 88.110ms 0.501x
Texture2D<RGBA32F>.Load uniform: 44.288ms 0.997x
Texture2D<RGBA32F>.Load linear: 66.145ms 0.668x
Texture2D<RGBA32F>.Load random: 88.124ms 0.501x
```
**AMD GCN2** was a very popular architecture. First card using this architecture was Radeon 7790. Many Radeon 200 and 300 series cards also use this architecture. Both Xbox and PS4 (base model) GPUs are based on GCN2 architecture, making this architecture very important optimization target.

**Typed loads:** GCN coalesces linear typed loads. But only 1d loads (R8, R16F, R32F). Coalesced load performance is 4x. Both linear access pattern (all threads in wave load subsequent addresses) and uniform access (all threads in wave load the same address) coalesce perfectly. Typed loads of every dimension (1d/2d/4d) and channel width (8b/16b/32b) perform identically. Best bytes/cycle rate can be achieved either by R32 coalesced load (when access pattern suits this) or always with RGBA32 load.

**Raw (ByteAddressBuffer) loads:** Similar to typed loads. 1d raw loads coalesce perfectly (4x) on linear access. Uniform address raw loads generates scalar unit loads on GCN. Scalar loads use separate cache and are stored to separate SGPR register file -> reduced register & cache pressure & doesn't stress vector load path. Scalar 1d load is 4x faster than normal 1d load. Scalar 2d load is 4x faster than normal 2d load. Scalar 4d load is 2x faster than normal 4d load. Unaligned (alignment=4) loads have equal performance to aligned (alignment=8/16). 3d raw linear loads have equal performance to 4d loads, but random 3d loads are slightly slower.

**Texture loads:** Similar performance as typed buffer loads. However no coalescing in 1d linear access and no scalar unit offload of uniform access. Random access of wide formats tends to be slightly slower (but my 2d random produces different access pattern than 1d).

**Structured buffer loads:** Performance is identical to similar width raw buffer loads.

**Cbuffer loads:** AMD GCN architecture doesn't have special constant buffer hardware. Constant buffer load performance is identical to raw and structured buffers. Prefer uniform addresses to allow the compiler to generate scalar loads, which is around 4x faster and has much lower latency and doesn't waste VGPRs.

**Suggestions:** Prefer wide fat 4d loads instead of multiple narrow loads. If you have perfectly linear memory access pattern, 1d coalesced loads are also fast. ByteAddressBuffers (raw loads) have good performance: Full speed 128 bit 4d loads, 4x rate 1d loads (linear access), and the compiler offloads uniform address loads to scalar unit, saving VGPR pressure and vector memory instructions.

These results match with AMDs wide loads & coalescing documents, see: http://gpuopen.com/gcn-memory-coalescing/. I would be glad if AMD released a public document describing all scalar load optimization cases supported by their compiler.

### AMD GCN3 (R9 Fury 56 CU)
```markdown
Buffer<R8>.Load uniform: 8.963ms 3.911x
Buffer<R8>.Load linear: 8.917ms 3.931x
Buffer<R8>.Load random: 35.058ms 1.000x
Buffer<RG8>.Load uniform: 39.416ms 0.889x
Buffer<RG8>.Load linear: 39.447ms 0.889x
Buffer<RG8>.Load random: 39.413ms 0.889x
Buffer<RGBA8>.Load uniform: 35.051ms 1.000x
Buffer<RGBA8>.Load linear: 35.048ms 1.000x
Buffer<RGBA8>.Load random: 35.051ms 1.000x
Buffer<R16f>.Load uniform: 8.898ms 3.939x
Buffer<R16f>.Load linear: 8.909ms 3.934x
Buffer<R16f>.Load random: 35.050ms 1.000x
Buffer<RG16f>.Load uniform: 39.405ms 0.890x
Buffer<RG16f>.Load linear: 39.435ms 0.889x
Buffer<RG16f>.Load random: 39.407ms 0.889x
Buffer<RGBA16f>.Load uniform: 35.041ms 1.000x
Buffer<RGBA16f>.Load linear: 35.043ms 1.000x
Buffer<RGBA16f>.Load random: 35.046ms 1.000x
Buffer<R32f>.Load uniform: 8.897ms 3.940x
Buffer<R32f>.Load linear: 8.910ms 3.934x
Buffer<R32f>.Load random: 35.048ms 1.000x
Buffer<RG32f>.Load uniform: 39.407ms 0.889x
Buffer<RG32f>.Load linear: 39.433ms 0.889x
Buffer<RG32f>.Load random: 39.406ms 0.889x
Buffer<RGBA32f>.Load uniform: 35.043ms 1.000x
Buffer<RGBA32f>.Load linear: 35.045ms 1.000x
Buffer<RGBA32f>.Load random: 39.405ms 0.890x
ByteAddressBuffer.Load uniform: 10.956ms 3.199x
ByteAddressBuffer.Load linear: 9.100ms 3.852x
ByteAddressBuffer.Load random: 35.038ms 1.000x
ByteAddressBuffer.Load2 uniform: 11.070ms 3.166x
ByteAddressBuffer.Load2 linear: 39.413ms 0.889x
ByteAddressBuffer.Load2 random: 39.411ms 0.889x
ByteAddressBuffer.Load3 uniform: 13.534ms 2.590x
ByteAddressBuffer.Load3 linear: 35.047ms 1.000x
ByteAddressBuffer.Load3 random: 70.033ms 0.500x
ByteAddressBuffer.Load4 uniform: 17.944ms 1.953x
ByteAddressBuffer.Load4 linear: 35.072ms 0.999x
ByteAddressBuffer.Load4 random: 39.149ms 0.895x
ByteAddressBuffer.Load2 unaligned uniform: 11.209ms 3.127x
ByteAddressBuffer.Load2 unaligned linear: 39.408ms 0.889x
ByteAddressBuffer.Load2 unaligned random: 39.406ms 0.890x
ByteAddressBuffer.Load4 unaligned uniform: 17.933ms 1.955x
ByteAddressBuffer.Load4 unaligned linear: 35.066ms 1.000x
ByteAddressBuffer.Load4 unaligned random: 43.241ms 0.811x
StructuredBuffer<float>.Load uniform: 12.653ms 2.770x
StructuredBuffer<float>.Load linear: 8.913ms 3.932x
StructuredBuffer<float>.Load random: 35.059ms 1.000x
StructuredBuffer<float2>.Load uniform: 12.799ms 2.739x
StructuredBuffer<float2>.Load linear: 39.445ms 0.889x
StructuredBuffer<float2>.Load random: 39.413ms 0.889x
StructuredBuffer<float4>.Load uniform: 12.834ms 2.731x
StructuredBuffer<float4>.Load linear: 35.049ms 1.000x
StructuredBuffer<float4>.Load random: 39.411ms 0.889x
cbuffer{float4} load uniform: 14.861ms 2.359x
cbuffer{float4} load linear: 35.534ms 0.986x
cbuffer{float4} load random: 39.412ms 0.889x
Texture2D<R8>.Load uniform: 35.063ms 1.000x
Texture2D<R8>.Load linear: 35.038ms 1.000x
Texture2D<R8>.Load random: 35.040ms 1.000x
Texture2D<RG8>.Load uniform: 39.430ms 0.889x
Texture2D<RG8>.Load linear: 39.436ms 0.889x
Texture2D<RG8>.Load random: 39.436ms 0.889x
Texture2D<RGBA8>.Load uniform: 35.059ms 1.000x
Texture2D<RGBA8>.Load linear: 35.061ms 1.000x
Texture2D<RGBA8>.Load random: 35.055ms 1.000x
Texture2D<R16F>.Load uniform: 35.056ms 1.000x
Texture2D<R16F>.Load linear: 35.038ms 1.000x
Texture2D<R16F>.Load random: 35.040ms 1.000x
Texture2D<RG16F>.Load uniform: 39.431ms 0.889x
Texture2D<RG16F>.Load linear: 39.440ms 0.889x
Texture2D<RG16F>.Load random: 39.436ms 0.889x
Texture2D<RGBA16F>.Load uniform: 35.054ms 1.000x
Texture2D<RGBA16F>.Load linear: 35.061ms 1.000x
Texture2D<RGBA16F>.Load random: 70.037ms 0.500x
Texture2D<R32F>.Load uniform: 35.055ms 1.000x
Texture2D<R32F>.Load linear: 35.041ms 1.000x
Texture2D<R32F>.Load random: 35.041ms 1.000x
Texture2D<RG32F>.Load uniform: 39.433ms 0.889x
Texture2D<RG32F>.Load linear: 39.439ms 0.889x
Texture2D<RG32F>.Load random: 70.039ms 0.500x
Texture2D<RGBA32F>.Load uniform: 35.054ms 1.000x
Texture2D<RGBA32F>.Load linear: 52.549ms 0.667x
Texture2D<RGBA32F>.Load random: 70.037ms 0.500x
 ```
  
**AMD GCN3** results (ratios) are identical to GCN2. See GCN2 for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between different GCN revisions.

### AMD GCN4 (RX 480)
```markdown
Buffer<R8>.Load uniform: 11.008ms 3.900x
Buffer<R8>.Load linear: 11.187ms 3.838x
Buffer<R8>.Load random: 42.906ms 1.001x
Buffer<RG8>.Load uniform: 48.280ms 0.889x
Buffer<RG8>.Load linear: 48.685ms 0.882x
Buffer<RG8>.Load random: 48.246ms 0.890x
Buffer<RGBA8>.Load uniform: 42.911ms 1.001x
Buffer<RGBA8>.Load linear: 43.733ms 0.982x
Buffer<RGBA8>.Load random: 42.934ms 1.000x
Buffer<R16f>.Load uniform: 10.852ms 3.956x
Buffer<R16f>.Load linear: 10.840ms 3.961x
Buffer<R16f>.Load random: 42.820ms 1.003x
Buffer<RG16f>.Load uniform: 48.153ms 0.892x
Buffer<RG16f>.Load linear: 48.161ms 0.891x
Buffer<RG16f>.Load random: 48.161ms 0.891x
Buffer<RGBA16f>.Load uniform: 42.832ms 1.002x
Buffer<RGBA16f>.Load linear: 42.900ms 1.001x
Buffer<RGBA16f>.Load random: 42.844ms 1.002x
Buffer<R32f>.Load uniform: 10.852ms 3.956x
Buffer<R32f>.Load linear: 10.841ms 3.960x
Buffer<R32f>.Load random: 42.816ms 1.003x
Buffer<RG32f>.Load uniform: 48.158ms 0.892x
Buffer<RG32f>.Load linear: 48.161ms 0.891x
Buffer<RG32f>.Load random: 48.161ms 0.891x
Buffer<RGBA32f>.Load uniform: 42.827ms 1.002x
Buffer<RGBA32f>.Load linear: 42.913ms 1.000x
Buffer<RGBA32f>.Load random: 48.176ms 0.891x
ByteAddressBuffer.Load uniform: 13.403ms 3.203x
ByteAddressBuffer.Load linear: 11.118ms 3.862x
ByteAddressBuffer.Load random: 42.911ms 1.001x
ByteAddressBuffer.Load2 uniform: 13.503ms 3.180x
ByteAddressBuffer.Load2 linear: 48.235ms 0.890x
ByteAddressBuffer.Load2 random: 48.242ms 0.890x
ByteAddressBuffer.Load3 uniform: 16.646ms 2.579x
ByteAddressBuffer.Load3 linear: 42.913ms 1.001x
ByteAddressBuffer.Load3 random: 85.682ms 0.501x
ByteAddressBuffer.Load4 uniform: 21.836ms 1.966x
ByteAddressBuffer.Load4 linear: 42.929ms 1.000x
ByteAddressBuffer.Load4 random: 47.936ms 0.896x
ByteAddressBuffer.Load2 unaligned uniform: 13.454ms 3.191x
ByteAddressBuffer.Load2 unaligned linear: 48.150ms 0.892x
ByteAddressBuffer.Load2 unaligned random: 48.163ms 0.891x
ByteAddressBuffer.Load4 unaligned uniform: 21.765ms 1.973x
ByteAddressBuffer.Load4 unaligned linear: 42.853ms 1.002x
ByteAddressBuffer.Load4 unaligned random: 52.866ms 0.812x
StructuredBuffer<float>.Load uniform: 15.513ms 2.768x
StructuredBuffer<float>.Load linear: 10.895ms 3.941x
StructuredBuffer<float>.Load random: 42.885ms 1.001x
StructuredBuffer<float2>.Load uniform: 15.695ms 2.736x
StructuredBuffer<float2>.Load linear: 48.231ms 0.890x
StructuredBuffer<float2>.Load random: 48.217ms 0.890x
StructuredBuffer<float4>.Load uniform: 15.810ms 2.716x
StructuredBuffer<float4>.Load linear: 42.907ms 1.001x
StructuredBuffer<float4>.Load random: 48.224ms 0.890x
cbuffer{float4} load uniform: 17.249ms 2.489x
cbuffer{float4} load linear: 43.054ms 0.997x
cbuffer{float4} load random: 48.214ms 0.890x
Texture2D<R8>.Load uniform: 42.889ms 1.001x
Texture2D<R8>.Load linear: 42.877ms 1.001x
Texture2D<R8>.Load random: 42.889ms 1.001x
Texture2D<RG8>.Load uniform: 48.252ms 0.890x
Texture2D<RG8>.Load linear: 48.254ms 0.890x
Texture2D<RG8>.Load random: 48.254ms 0.890x
Texture2D<RGBA8>.Load uniform: 42.939ms 1.000x
Texture2D<RGBA8>.Load linear: 42.969ms 0.999x
Texture2D<RGBA8>.Load random: 42.945ms 1.000x
Texture2D<R16F>.Load uniform: 42.891ms 1.001x
Texture2D<R16F>.Load linear: 42.915ms 1.000x
Texture2D<R16F>.Load random: 42.866ms 1.002x
Texture2D<RG16F>.Load uniform: 48.234ms 0.890x
Texture2D<RG16F>.Load linear: 48.365ms 0.888x
Texture2D<RG16F>.Load random: 48.220ms 0.890x
Texture2D<RGBA16F>.Load uniform: 42.911ms 1.001x
Texture2D<RGBA16F>.Load linear: 42.943ms 1.000x
Texture2D<RGBA16F>.Load random: 85.655ms 0.501x
Texture2D<R32F>.Load uniform: 42.896ms 1.001x
Texture2D<R32F>.Load linear: 42.910ms 1.001x
Texture2D<R32F>.Load random: 42.871ms 1.001x
Texture2D<RG32F>.Load uniform: 48.239ms 0.890x
Texture2D<RG32F>.Load linear: 48.367ms 0.888x
Texture2D<RG32F>.Load random: 85.634ms 0.501x
Texture2D<RGBA32F>.Load uniform: 42.927ms 1.000x
Texture2D<RGBA32F>.Load linear: 64.284ms 0.668x
Texture2D<RGBA32F>.Load random: 85.638ms 0.501x
```
**AMD GCN4** results (ratios) are identical to GCN2/3. See GCN2 for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between different GCN revisions.

### AMD GCN5 (Vega Frontier Edition)
```markdown
Buffer<R8>.Load uniform: 6.024ms 3.693x
Buffer<R8>.Load linear: 5.798ms 3.838x
Buffer<R8>.Load random: 21.411ms 1.039x
Buffer<RG8>.Load uniform: 21.648ms 1.028x
Buffer<RG8>.Load linear: 21.108ms 1.054x
Buffer<RG8>.Load random: 21.721ms 1.024x
Buffer<RGBA8>.Load uniform: 22.315ms 0.997x
Buffer<RGBA8>.Load linear: 22.055ms 1.009x
Buffer<RGBA8>.Load random: 22.251ms 1.000x
Buffer<R16f>.Load uniform: 6.421ms 3.465x
Buffer<R16f>.Load linear: 6.119ms 3.636x
Buffer<R16f>.Load random: 21.534ms 1.033x
Buffer<RG16f>.Load uniform: 21.010ms 1.059x
Buffer<RG16f>.Load linear: 20.785ms 1.071x
Buffer<RG16f>.Load random: 20.903ms 1.064x
Buffer<RGBA16f>.Load uniform: 21.083ms 1.055x
Buffer<RGBA16f>.Load linear: 22.849ms 0.974x
Buffer<RGBA16f>.Load random: 22.189ms 1.003x
Buffer<R32f>.Load uniform: 6.374ms 3.491x
Buffer<R32f>.Load linear: 6.265ms 3.552x
Buffer<R32f>.Load random: 21.892ms 1.016x
Buffer<RG32f>.Load uniform: 21.918ms 1.015x
Buffer<RG32f>.Load linear: 21.081ms 1.056x
Buffer<RG32f>.Load random: 22.866ms 0.973x
Buffer<RGBA32f>.Load uniform: 22.022ms 1.010x
Buffer<RGBA32f>.Load linear: 22.025ms 1.010x
Buffer<RGBA32f>.Load random: 24.889ms 0.894x
ByteAddressBuffer.Load uniform: 5.187ms 4.289x
ByteAddressBuffer.Load linear: 6.682ms 3.330x
ByteAddressBuffer.Load random: 22.153ms 1.004x
ByteAddressBuffer.Load2 uniform: 5.907ms 3.767x
ByteAddressBuffer.Load2 linear: 21.541ms 1.033x
ByteAddressBuffer.Load2 random: 22.435ms 0.992x
ByteAddressBuffer.Load3 uniform: 8.896ms 2.501x
ByteAddressBuffer.Load3 linear: 22.019ms 1.011x
ByteAddressBuffer.Load3 random: 43.438ms 0.512x
ByteAddressBuffer.Load4 uniform: 10.671ms 2.085x
ByteAddressBuffer.Load4 linear: 20.912ms 1.064x
ByteAddressBuffer.Load4 random: 23.508ms 0.947x
ByteAddressBuffer.Load2 unaligned uniform: 6.080ms 3.660x
ByteAddressBuffer.Load2 unaligned linear: 21.813ms 1.020x
ByteAddressBuffer.Load2 unaligned random: 22.436ms 0.992x
ByteAddressBuffer.Load4 unaligned uniform: 11.457ms 1.942x
ByteAddressBuffer.Load4 unaligned linear: 21.817ms 1.020x
ByteAddressBuffer.Load4 unaligned random: 27.530ms 0.808x
StructuredBuffer<float>.Load uniform: 6.384ms 3.486x
StructuredBuffer<float>.Load linear: 6.314ms 3.524x
StructuredBuffer<float>.Load random: 21.424ms 1.039x
StructuredBuffer<float2>.Load uniform: 6.257ms 3.556x
StructuredBuffer<float2>.Load linear: 20.940ms 1.063x
StructuredBuffer<float2>.Load random: 23.044ms 0.966x
StructuredBuffer<float4>.Load uniform: 6.620ms 3.361x
StructuredBuffer<float4>.Load linear: 21.771ms 1.022x
StructuredBuffer<float4>.Load random: 25.229ms 0.882x
cbuffer{float4} load uniform: 8.011ms 2.778x
cbuffer{float4} load linear: 22.951ms 0.969x
cbuffer{float4} load random: 24.806ms 0.897x
Texture2D<R8>.Load uniform: 22.585ms 0.985x
Texture2D<R8>.Load linear: 21.733ms 1.024x
Texture2D<R8>.Load random: 21.371ms 1.041x
Texture2D<RG8>.Load uniform: 20.774ms 1.071x
Texture2D<RG8>.Load linear: 20.806ms 1.069x
Texture2D<RG8>.Load random: 22.936ms 0.970x
Texture2D<RGBA8>.Load uniform: 22.022ms 1.010x
Texture2D<RGBA8>.Load linear: 21.644ms 1.028x
Texture2D<RGBA8>.Load random: 22.586ms 0.985x
Texture2D<R16F>.Load uniform: 22.620ms 0.984x
Texture2D<R16F>.Load linear: 22.730ms 0.979x
Texture2D<R16F>.Load random: 21.356ms 1.042x
Texture2D<RG16F>.Load uniform: 20.722ms 1.074x
Texture2D<RG16F>.Load linear: 20.723ms 1.074x
Texture2D<RG16F>.Load random: 21.893ms 1.016x
Texture2D<RGBA16F>.Load uniform: 22.287ms 0.998x
Texture2D<RGBA16F>.Load linear: 22.116ms 1.006x
Texture2D<RGBA16F>.Load random: 42.739ms 0.521x
Texture2D<R32F>.Load uniform: 21.325ms 1.043x
Texture2D<R32F>.Load linear: 21.370ms 1.041x
Texture2D<R32F>.Load random: 21.393ms 1.040x
Texture2D<RG32F>.Load uniform: 20.747ms 1.072x
Texture2D<RG32F>.Load linear: 20.754ms 1.072x
Texture2D<RG32F>.Load random: 41.415ms 0.537x
Texture2D<RGBA32F>.Load uniform: 20.551ms 1.083x
Texture2D<RGBA32F>.Load linear: 31.748ms 0.701x
Texture2D<RGBA32F>.Load random: 42.097ms 0.529x
```
**AMD GCN5** results (ratios) are identical to GCN2/3/4. See GCN2 for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between different GCN revisions.

### NVidia Maxwell (GTX 980 Ti)
```markdown
Buffer<R8>.Load uniform: 1.249ms 28.812x
Buffer<R8>.Load linear: 34.105ms 1.055x
Buffer<R8>.Load random: 34.187ms 1.053x
Buffer<RG8>.Load uniform: 1.847ms 19.485x
Buffer<RG8>.Load linear: 34.106ms 1.055x
Buffer<RG8>.Load random: 34.477ms 1.044x
Buffer<RGBA8>.Load uniform: 2.452ms 14.680x
Buffer<RGBA8>.Load linear: 35.773ms 1.006x
Buffer<RGBA8>.Load random: 35.996ms 1.000x
Buffer<R16f>.Load uniform: 1.491ms 24.148x
Buffer<R16f>.Load linear: 34.077ms 1.056x
Buffer<R16f>.Load random: 34.463ms 1.044x
Buffer<RG16f>.Load uniform: 1.916ms 18.785x
Buffer<RG16f>.Load linear: 34.229ms 1.052x
Buffer<RG16f>.Load random: 34.597ms 1.040x
Buffer<RGBA16f>.Load uniform: 2.519ms 14.291x
Buffer<RGBA16f>.Load linear: 35.787ms 1.006x
Buffer<RGBA16f>.Load random: 35.996ms 1.000x
Buffer<R32f>.Load uniform: 1.478ms 24.350x
Buffer<R32f>.Load linear: 34.098ms 1.056x
Buffer<R32f>.Load random: 34.353ms 1.048x
Buffer<RG32f>.Load uniform: 1.845ms 19.514x
Buffer<RG32f>.Load linear: 34.138ms 1.054x
Buffer<RG32f>.Load random: 34.495ms 1.044x
Buffer<RGBA32f>.Load uniform: 2.374ms 15.163x
Buffer<RGBA32f>.Load linear: 67.973ms 0.530x
Buffer<RGBA32f>.Load random: 68.054ms 0.529x
ByteAddressBuffer.Load uniform: 21.403ms 1.682x
ByteAddressBuffer.Load linear: 21.906ms 1.643x
ByteAddressBuffer.Load random: 24.336ms 1.479x
ByteAddressBuffer.Load2 uniform: 45.620ms 0.789x
ByteAddressBuffer.Load2 linear: 55.815ms 0.645x
ByteAddressBuffer.Load2 random: 48.744ms 0.738x
ByteAddressBuffer.Load3 uniform: 52.929ms 0.680x
ByteAddressBuffer.Load3 linear: 79.057ms 0.455x
ByteAddressBuffer.Load3 random: 93.636ms 0.384x
ByteAddressBuffer.Load4 uniform: 68.510ms 0.525x
ByteAddressBuffer.Load4 linear: 114.561ms 0.314x
ByteAddressBuffer.Load4 random: 209.280ms 0.172x
ByteAddressBuffer.Load2 unaligned uniform: 45.640ms 0.789x
ByteAddressBuffer.Load2 unaligned linear: 55.802ms 0.645x
ByteAddressBuffer.Load2 unaligned random: 48.717ms 0.739x
ByteAddressBuffer.Load4 unaligned uniform: 68.685ms 0.524x
ByteAddressBuffer.Load4 unaligned linear: 115.244ms 0.312x
ByteAddressBuffer.Load4 unaligned random: 210.358ms 0.171x
StructuredBuffer<float>.Load uniform: 1.116ms 32.267x
StructuredBuffer<float>.Load linear: 34.094ms 1.056x
StructuredBuffer<float>.Load random: 34.092ms 1.056x
StructuredBuffer<float2>.Load uniform: 1.569ms 22.942x
StructuredBuffer<float2>.Load linear: 34.143ms 1.054x
StructuredBuffer<float2>.Load random: 34.125ms 1.055x
StructuredBuffer<float4>.Load uniform: 2.087ms 17.245x
StructuredBuffer<float4>.Load linear: 67.959ms 0.530x
StructuredBuffer<float4>.Load random: 67.950ms 0.530x
cbuffer{float4} load uniform: 1.298ms 27.733x
cbuffer{float4} load linear: 798.703ms 0.045x
cbuffer{float4} load random: 324.356ms 0.111x
Texture2D<R8>.Load uniform: 1.962ms 18.351x
Texture2D<R8>.Load linear: 34.027ms 1.058x
Texture2D<R8>.Load random: 34.029ms 1.058x
Texture2D<RG8>.Load uniform: 1.994ms 18.054x
Texture2D<RG8>.Load linear: 34.334ms 1.048x
Texture2D<RG8>.Load random: 34.102ms 1.056x
Texture2D<RGBA8>.Load uniform: 2.247ms 16.018x
Texture2D<RGBA8>.Load linear: 36.077ms 0.998x
Texture2D<RGBA8>.Load random: 35.930ms 1.002x
Texture2D<R16F>.Load uniform: 2.021ms 17.814x
Texture2D<R16F>.Load linear: 34.040ms 1.057x
Texture2D<R16F>.Load random: 34.021ms 1.058x
Texture2D<RG16F>.Load uniform: 2.020ms 17.822x
Texture2D<RG16F>.Load linear: 34.308ms 1.049x
Texture2D<RG16F>.Load random: 34.095ms 1.056x
Texture2D<RGBA16F>.Load uniform: 2.199ms 16.372x
Texture2D<RGBA16F>.Load linear: 36.074ms 0.998x
Texture2D<RGBA16F>.Load random: 68.064ms 0.529x
Texture2D<R32F>.Load uniform: 2.014ms 17.869x
Texture2D<R32F>.Load linear: 34.042ms 1.057x
Texture2D<R32F>.Load random: 34.028ms 1.058x
Texture2D<RG32F>.Load uniform: 1.981ms 18.166x
Texture2D<RG32F>.Load linear: 34.320ms 1.049x
Texture2D<RG32F>.Load random: 67.948ms 0.530x
Texture2D<RGBA32F>.Load uniform: 2.064ms 17.440x
Texture2D<RGBA32F>.Load linear: 67.974ms 0.530x
Texture2D<RGBA32F>.Load random: 68.049ms 0.529x
```

**Typed loads:** Maxwell doesn't coalesce any typed loads. Dimensions (1d/2d/4d) and channel widths (8b/16b/32b) don't directly affect performance. All up to 64 bit loads are full rate. 128 bit loads are half rate (only RGBA32). Best bytes per cycle rate can be achieved by 64+ bit loads (RGBA16, RG32, RGBA32).

**Raw (ByteAddressBuffer) loads:** Oddly we see no coalescing here either. CUDA code shows big performance improvement with similar linear access pattern. All 1d raw loads are as fast as typed buffer loads. However NV doesn't seem to emit wide raw loads either. 2d is exactly 2x slower, 3d is 3x slower and 4d is 4x slower than 1d. NVIDIA supports 64 bit and 128 wide raw loads in CUDA, see: https://devblogs.nvidia.com/parallelforall/cuda-pro-tip-increase-performance-with-vectorized-memory-access/. Wide loads in CUDA however require memory alignment (8/16 bytes). My test case is perfectly aligned, but HLSL ByteAddressBuffer.Load4() specification only requires alignment of 4. In general case it's hard to prove alignment of 16 (in my code there's an explicit multiply address by 16).

**Structured buffer loads:** Structured buffer loads guarantee natural alignment. Nvidia has full rate 1d and 2d structured buffer loads. But 4d loads (128 bit) are half rate as usual.

**Texture loads:** Similar performance as typed buffer loads. Random access of wide formats tends to be slightly slower (but my 2d random produces different access pattern than 1d).

**Cbuffer loads:** Nvidia Maxwell (and newer GPUs) have a special constant buffer hardware unit. Uniform address constant buffer loads are up to 32x faster (warp width) than standard memory loads. However non-uniform constant buffer loads are dead slow. Nvidia CUDA documents tell us that constant buffer load gets serialized for each unique address. Thus we can see up to 32x performance drop compared to best case. But in my test case (each lane = different address), we se up to 200x slow down. This result tells us that there's likely a small constant buffer cache on each SM, and if your access pattern is bad enough, this cache starts to trash badly. Unfortunately Nvidia doesn't provide us a public document describing best practices to avoid this pitfall.

**Uniform address optimization:** New Nvidia drivers introduced a shader compiler based uniform address load optimization for loops. This speeds up the loads in these cases by up to 28x (close to 32x theoretical warp width). This new optimization is awesome, because previously Nvidia had to fully lean to their constant buffer hardware for good uniform load performance. As we all know constant buffers are very limited (vec4 arrays only and 64KB size limit). See "Uniform Address Load Investigation" chapter for more info.

**Suggestions:** Prefer 64+ bit typed loads (RGBA16, RG32, RGBA32). ByteAddressBuffer wide loads and coalescing doesn't seem to work in DirectX. Uniform address loads (inside loop using loop index) have a fast path. Use it whenever possible. This results in similar performance as using constant buffer hardware, but supports all buffer and texture types. No size or alignment limitations!

### NVidia Pascal (GTX 1070 Ti)
```markdown
Buffer<R8>.Load uniform: 0.845ms 36.835x
Buffer<R8>.Load linear: 29.335ms 1.061x
Buffer<R8>.Load random: 28.981ms 1.074x
Buffer<RG8>.Load uniform: 1.151ms 27.036x
Buffer<RG8>.Load linear: 30.267ms 1.028x
Buffer<RG8>.Load random: 29.359ms 1.060x
Buffer<RGBA8>.Load uniform: 1.534ms 20.286x
Buffer<RGBA8>.Load linear: 31.214ms 0.997x
Buffer<RGBA8>.Load random: 31.118ms 1.000x
Buffer<R16f>.Load uniform: 0.808ms 38.516x
Buffer<R16f>.Load linear: 28.943ms 1.075x
Buffer<R16f>.Load random: 29.870ms 1.042x
Buffer<RG16f>.Load uniform: 1.119ms 27.803x
Buffer<RG16f>.Load linear: 29.458ms 1.056x
Buffer<RG16f>.Load random: 29.904ms 1.041x
Buffer<RGBA16f>.Load uniform: 1.467ms 21.207x
Buffer<RGBA16f>.Load linear: 31.222ms 0.997x
Buffer<RGBA16f>.Load random: 30.223ms 1.030x
Buffer<R32f>.Load uniform: 0.847ms 36.746x
Buffer<R32f>.Load linear: 30.240ms 1.029x
Buffer<R32f>.Load random: 28.963ms 1.074x
Buffer<RG32f>.Load uniform: 1.087ms 28.615x
Buffer<RG32f>.Load linear: 30.391ms 1.024x
Buffer<RG32f>.Load random: 29.475ms 1.056x
Buffer<RGBA32f>.Load uniform: 1.434ms 21.706x
Buffer<RGBA32f>.Load linear: 59.394ms 0.524x
Buffer<RGBA32f>.Load random: 57.593ms 0.540x
ByteAddressBuffer.Load uniform: 18.151ms 1.714x
ByteAddressBuffer.Load linear: 18.451ms 1.686x
ByteAddressBuffer.Load random: 21.305ms 1.461x
ByteAddressBuffer.Load2 uniform: 41.123ms 0.757x
ByteAddressBuffer.Load2 linear: 40.461ms 0.769x
ByteAddressBuffer.Load2 random: 49.244ms 0.632x
ByteAddressBuffer.Load3 uniform: 44.836ms 0.694x
ByteAddressBuffer.Load3 linear: 65.966ms 0.472x
ByteAddressBuffer.Load3 random: 77.712ms 0.400x
ByteAddressBuffer.Load4 uniform: 58.439ms 0.532x
ByteAddressBuffer.Load4 linear: 97.260ms 0.320x
ByteAddressBuffer.Load4 random: 174.779ms 0.178x
ByteAddressBuffer.Load2 unaligned uniform: 41.147ms 0.756x
ByteAddressBuffer.Load2 unaligned linear: 40.483ms 0.769x
ByteAddressBuffer.Load2 unaligned random: 55.911ms 0.557x
ByteAddressBuffer.Load4 unaligned uniform: 58.126ms 0.535x
ByteAddressBuffer.Load4 unaligned linear: 99.081ms 0.314x
ByteAddressBuffer.Load4 unaligned random: 179.514ms 0.173x
StructuredBuffer<float>.Load uniform: 0.887ms 35.091x
StructuredBuffer<float>.Load linear: 29.878ms 1.042x
StructuredBuffer<float>.Load random: 29.408ms 1.058x
StructuredBuffer<float2>.Load uniform: 1.141ms 27.279x
StructuredBuffer<float2>.Load linear: 30.575ms 1.018x
StructuredBuffer<float2>.Load random: 28.985ms 1.074x
StructuredBuffer<float4>.Load uniform: 1.523ms 20.436x
StructuredBuffer<float4>.Load linear: 58.493ms 0.532x
StructuredBuffer<float4>.Load random: 58.546ms 0.532x
cbuffer{float4} load uniform: 1.390ms 22.394x
cbuffer{float4} load linear: 684.120ms 0.045x
cbuffer{float4} load random: 273.085ms 0.114x
Texture2D<R8>.Load uniform: 1.627ms 19.125x
Texture2D<R8>.Load linear: 28.924ms 1.076x
Texture2D<R8>.Load random: 28.923ms 1.076x
Texture2D<RG8>.Load uniform: 1.378ms 22.577x
Texture2D<RG8>.Load linear: 29.041ms 1.072x
Texture2D<RG8>.Load random: 28.938ms 1.075x
Texture2D<RGBA8>.Load uniform: 1.563ms 19.914x
Texture2D<RGBA8>.Load linear: 30.666ms 1.015x
Texture2D<RGBA8>.Load random: 30.334ms 1.026x
Texture2D<R16F>.Load uniform: 1.313ms 23.704x
Texture2D<R16F>.Load linear: 28.961ms 1.074x
Texture2D<R16F>.Load random: 28.968ms 1.074x
Texture2D<RG16F>.Load uniform: 1.360ms 22.883x
Texture2D<RG16F>.Load linear: 29.048ms 1.071x
Texture2D<RG16F>.Load random: 28.926ms 1.076x
Texture2D<RGBA16F>.Load uniform: 1.501ms 20.729x
Texture2D<RGBA16F>.Load linear: 30.649ms 1.015x
Texture2D<RGBA16F>.Load random: 57.629ms 0.540x
Texture2D<R32F>.Load uniform: 1.384ms 22.477x
Texture2D<R32F>.Load linear: 28.955ms 1.075x
Texture2D<R32F>.Load random: 28.968ms 1.074x
Texture2D<RG32F>.Load uniform: 1.408ms 22.101x
Texture2D<RG32F>.Load linear: 29.056ms 1.071x
Texture2D<RG32F>.Load random: 57.672ms 0.540x
Texture2D<RGBA32F>.Load uniform: 1.538ms 20.232x
Texture2D<RGBA32F>.Load linear: 57.653ms 0.540x
Texture2D<RGBA32F>.Load random: 57.557ms 0.541x
```
**NVIDIA Pascal** results (ratios) are identical to Maxwell. See Maxwell for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between Maxwell and Pascal.

### NVIDIA Kepler (600/700 series)
```markdown
OLD VERSION RESULTS REMOVED. ANALYSIS STILL VALID.
PLEASE SEND ME NEW RESULTS IF YOU OWN KEPLER GPU
```

**NVIDIA Kepler** results (ratios) are identical to Maxwell & Pascal. See Maxwell for analysis. Clock and SM scaling reveal that there's no bandwidth/issue related changes in the texture/L1$ architecture between Kepler, Maxwell and Pascal.

### NVidia Volta (Titan V)
```markdown
Buffer<R8>.Load uniform: 2.241ms 8.139x
Buffer<R8>.Load linear: 14.806ms 1.232x
Buffer<R8>.Load random: 16.514ms 1.104x
Buffer<RG8>.Load uniform: 4.576ms 3.985x
Buffer<RG8>.Load linear: 16.397ms 1.112x
Buffer<RG8>.Load random: 16.707ms 1.092x
Buffer<RGBA8>.Load uniform: 5.155ms 3.538x
Buffer<RGBA8>.Load linear: 16.726ms 1.090x
Buffer<RGBA8>.Load random: 18.236ms 1.000x
Buffer<R16f>.Load uniform: 2.807ms 6.497x
Buffer<R16f>.Load linear: 14.771ms 1.235x
Buffer<R16f>.Load random: 16.857ms 1.082x
Buffer<RG16f>.Load uniform: 4.128ms 4.418x
Buffer<RG16f>.Load linear: 16.155ms 1.129x
Buffer<RG16f>.Load random: 15.140ms 1.205x
Buffer<RGBA16f>.Load uniform: 4.747ms 3.841x
Buffer<RGBA16f>.Load linear: 17.517ms 1.041x
Buffer<RGBA16f>.Load random: 17.727ms 1.029x
Buffer<R32f>.Load uniform: 2.630ms 6.935x
Buffer<R32f>.Load linear: 17.341ms 1.052x
Buffer<R32f>.Load random: 15.922ms 1.145x
Buffer<RG32f>.Load uniform: 4.769ms 3.824x
Buffer<RG32f>.Load linear: 15.745ms 1.158x
Buffer<RG32f>.Load random: 15.801ms 1.154x
Buffer<RGBA32f>.Load uniform: 4.772ms 3.822x
Buffer<RGBA32f>.Load linear: 29.343ms 0.621x
Buffer<RGBA32f>.Load random: 29.427ms 0.620x
ByteAddressBuffer.Load uniform: 8.948ms 2.038x
ByteAddressBuffer.Load linear: 8.722ms 2.091x
ByteAddressBuffer.Load random: 10.403ms 1.753x
ByteAddressBuffer.Load2 uniform: 10.132ms 1.800x
ByteAddressBuffer.Load2 linear: 11.406ms 1.599x
ByteAddressBuffer.Load2 random: 10.999ms 1.658x
ByteAddressBuffer.Load3 uniform: 12.638ms 1.443x
ByteAddressBuffer.Load3 linear: 13.708ms 1.330x
ByteAddressBuffer.Load3 random: 14.081ms 1.295x
ByteAddressBuffer.Load4 uniform: 15.421ms 1.183x
ByteAddressBuffer.Load4 linear: 26.412ms 0.690x
ByteAddressBuffer.Load4 random: 18.078ms 1.009x
ByteAddressBuffer.Load2 unaligned uniform: 11.076ms 1.647x
ByteAddressBuffer.Load2 unaligned linear: 11.474ms 1.589x
ByteAddressBuffer.Load2 unaligned random: 12.227ms 1.492x
ByteAddressBuffer.Load4 unaligned uniform: 15.817ms 1.153x
ByteAddressBuffer.Load4 unaligned linear: 25.894ms 0.704x
ByteAddressBuffer.Load4 unaligned random: 18.138ms 1.005x
StructuredBuffer<float>.Load uniform: 6.606ms 2.761x
StructuredBuffer<float>.Load linear: 6.555ms 2.782x
StructuredBuffer<float>.Load random: 9.063ms 2.012x
StructuredBuffer<float2>.Load uniform: 8.332ms 2.189x
StructuredBuffer<float2>.Load linear: 8.545ms 2.134x
StructuredBuffer<float2>.Load random: 7.271ms 2.508x
StructuredBuffer<float4>.Load uniform: 8.890ms 2.051x
StructuredBuffer<float4>.Load linear: 9.650ms 1.890x
StructuredBuffer<float4>.Load random: 9.677ms 1.885x
cbuffer{float4} load uniform: 1.381ms 13.202x
cbuffer{float4} load linear: 320.961ms 0.057x
cbuffer{float4} load random: 150.072ms 0.122x
Texture2D<R8>.Load uniform: 4.481ms 4.070x
Texture2D<R8>.Load linear: 15.953ms 1.143x
Texture2D<R8>.Load random: 15.058ms 1.211x
Texture2D<RG8>.Load uniform: 4.594ms 3.970x
Texture2D<RG8>.Load linear: 14.838ms 1.229x
Texture2D<RG8>.Load random: 14.938ms 1.221x
Texture2D<RGBA8>.Load uniform: 5.140ms 3.548x
Texture2D<RGBA8>.Load linear: 14.915ms 1.223x
Texture2D<RGBA8>.Load random: 15.031ms 1.213x
Texture2D<R16F>.Load uniform: 5.748ms 3.173x
Texture2D<R16F>.Load linear: 15.321ms 1.190x
Texture2D<R16F>.Load random: 15.044ms 1.212x
Texture2D<RG16F>.Load uniform: 4.609ms 3.957x
Texture2D<RG16F>.Load linear: 14.918ms 1.222x
Texture2D<RG16F>.Load random: 14.851ms 1.228x
Texture2D<RGBA16F>.Load uniform: 5.182ms 3.519x
Texture2D<RGBA16F>.Load linear: 14.915ms 1.223x
Texture2D<RGBA16F>.Load random: 29.841ms 0.611x
Texture2D<R32F>.Load uniform: 4.462ms 4.087x
Texture2D<R32F>.Load linear: 15.615ms 1.168x
Texture2D<R32F>.Load random: 15.519ms 1.175x
Texture2D<RG32F>.Load uniform: 4.585ms 3.977x
Texture2D<RG32F>.Load linear: 16.651ms 1.095x
Texture2D<RG32F>.Load random: 29.710ms 0.614x
Texture2D<RGBA32F>.Load uniform: 5.163ms 3.532x
Texture2D<RGBA32F>.Load linear: 29.970ms 0.608x
Texture2D<RGBA32F>.Load random: 29.358ms 0.621x
```

**NVIDIA Volta** results (ratios) of most common load/sample operations are identical to Pascal. However there are some huge changes raw load performance. Raw loads: 1d ~2x faster, 2d-4d ~4x faster (slightly more on 3d and 4d). Nvidia definitely seems to now use a faster direct memory path for raw loads. Raw loads are now the best choice on Nvidia hardware (which is a direct opposite of their last gen hardware). Independent studies of Volta architecture show that their raw load L1$ latency also dropped from 85 cycles (Pascal) down to 28 cycles (Volta). This should makes raw loads even more viable in real applications. My benchmark measures only throughput, so latency improvement isn't visible.

**Uniform address optimization:** Uniform address optimization no longer affects StructuredBuffers. My educated guess is that StructuredBuffers (like raw buffers) now use the same lower latency direct memory path. Nvidia most likely hasn't yet implemented uniform address optimization for these new memory operations. Another curiosity is that Volta also has much lower performance advantage in the uniform address optimized cases (versus any other Nvidia GPU, including Turing).

### NVidia Turing (RTX 2080 Ti)
```
Buffer<R8>.Load uniform: 0.703ms 23.287x
Buffer<R8>.Load linear: 16.179ms 1.011x
Buffer<R8>.Load random: 15.435ms 1.060x
Buffer<RG8>.Load uniform: 0.881ms 18.567x
Buffer<RG8>.Load linear: 15.983ms 1.024x
Buffer<RG8>.Load random: 17.044ms 0.960x
Buffer<RGBA8>.Load uniform: 1.336ms 12.247x
Buffer<RGBA8>.Load linear: 16.825ms 0.973x
Buffer<RGBA8>.Load random: 16.364ms 1.000x
Buffer<R16f>.Load uniform: 0.662ms 24.729x
Buffer<R16f>.Load linear: 15.431ms 1.060x
Buffer<R16f>.Load random: 15.916ms 1.028x
Buffer<RG16f>.Load uniform: 0.870ms 18.811x
Buffer<RG16f>.Load linear: 16.861ms 0.971x
Buffer<RG16f>.Load random: 16.384ms 0.999x
Buffer<RGBA16f>.Load uniform: 1.331ms 12.296x
Buffer<RGBA16f>.Load linear: 15.892ms 1.030x
Buffer<RGBA16f>.Load random: 15.949ms 1.026x
Buffer<R32f>.Load uniform: 0.651ms 25.143x
Buffer<R32f>.Load linear: 15.438ms 1.060x
Buffer<R32f>.Load random: 16.851ms 0.971x
Buffer<RG32f>.Load uniform: 1.369ms 11.953x
Buffer<RG32f>.Load linear: 15.440ms 1.060x
Buffer<RG32f>.Load random: 15.917ms 1.028x
Buffer<RGBA32f>.Load uniform: 1.348ms 12.141x
Buffer<RGBA32f>.Load linear: 30.695ms 0.533x
Buffer<RGBA32f>.Load random: 32.514ms 0.503x
ByteAddressBuffer.Load uniform: 7.013ms 2.333x
ByteAddressBuffer.Load linear: 6.308ms 2.594x
ByteAddressBuffer.Load random: 7.347ms 2.227x
ByteAddressBuffer.Load2 uniform: 9.510ms 1.721x
ByteAddressBuffer.Load2 linear: 16.912ms 0.968x
ByteAddressBuffer.Load2 random: 9.715ms 1.684x
ByteAddressBuffer.Load3 uniform: 14.700ms 1.113x
ByteAddressBuffer.Load3 linear: 19.200ms 0.852x
ByteAddressBuffer.Load3 random: 14.804ms 1.105x
ByteAddressBuffer.Load4 uniform: 18.228ms 0.898x
ByteAddressBuffer.Load4 linear: 43.493ms 0.376x
ByteAddressBuffer.Load4 random: 32.616ms 0.502x
ByteAddressBuffer.Load2 unaligned uniform: 9.549ms 1.714x
ByteAddressBuffer.Load2 unaligned linear: 16.901ms 0.968x
ByteAddressBuffer.Load2 unaligned random: 9.719ms 1.684x
ByteAddressBuffer.Load4 unaligned uniform: 18.218ms 0.898x
ByteAddressBuffer.Load4 unaligned linear: 41.476ms 0.395x
ByteAddressBuffer.Load4 unaligned random: 32.081ms 0.510x
StructuredBuffer<float>.Load uniform: 6.535ms 2.504x
StructuredBuffer<float>.Load linear: 6.706ms 2.440x
StructuredBuffer<float>.Load random: 6.911ms 2.368x
StructuredBuffer<float2>.Load uniform: 8.057ms 2.031x
StructuredBuffer<float2>.Load linear: 16.874ms 0.970x
StructuredBuffer<float2>.Load random: 8.374ms 1.954x
StructuredBuffer<float4>.Load uniform: 15.491ms 1.056x
StructuredBuffer<float4>.Load linear: 20.327ms 0.805x
StructuredBuffer<float4>.Load random: 18.112ms 0.903x
cbuffer{float4} load uniform: 0.834ms 19.616x
cbuffer{float4} load linear: 328.935ms 0.050x
cbuffer{float4} load random: 125.135ms 0.131x
Texture2D<R8>.Load uniform: 0.746ms 21.929x
Texture2D<R8>.Load linear: 16.173ms 1.012x
Texture2D<R8>.Load random: 15.400ms 1.063x
Texture2D<RG8>.Load uniform: 1.043ms 15.691x
Texture2D<RG8>.Load linear: 15.421ms 1.061x
Texture2D<RG8>.Load random: 15.400ms 1.063x
Texture2D<RGBA8>.Load uniform: 1.876ms 8.725x
Texture2D<RGBA8>.Load linear: 16.462ms 0.994x
Texture2D<RGBA8>.Load random: 16.461ms 0.994x
Texture2D<R16F>.Load uniform: 0.741ms 22.092x
Texture2D<R16F>.Load linear: 16.253ms 1.007x
Texture2D<R16F>.Load random: 16.222ms 1.009x
Texture2D<RG16F>.Load uniform: 1.053ms 15.546x
Texture2D<RG16F>.Load linear: 15.440ms 1.060x
Texture2D<RG16F>.Load random: 16.148ms 1.013x
Texture2D<RGBA16F>.Load uniform: 1.890ms 8.659x
Texture2D<RGBA16F>.Load linear: 16.125ms 1.015x
Texture2D<RGBA16F>.Load random: 31.047ms 0.527x
Texture2D<R32F>.Load uniform: 0.746ms 21.930x
Texture2D<R32F>.Load linear: 16.403ms 0.998x
Texture2D<R32F>.Load random: 16.638ms 0.983x
Texture2D<RG32F>.Load uniform: 1.060ms 15.441x
Texture2D<RG32F>.Load linear: 15.439ms 1.060x
Texture2D<RG32F>.Load random: 31.903ms 0.513x
Texture2D<RGBA32F>.Load uniform: 1.888ms 8.668x
Texture2D<RGBA32F>.Load linear: 31.525ms 0.519x
Texture2D<RGBA32F>.Load random: 32.783ms 0.499x
```

**NVIDIA Turing** results (ratios) of most common load/sample operations are identical Volta. Except wide raw buffer load performance is closer to Maxwell/Pascal. In Volta, Nvidia used one large 128KB shared L1$ (freely configurable between groupshared mem and L1$), while in Turing they have 96KB shared L1$ which can be configured only as 64/32 or 32/64. This benchmark seems to point out that this halves their L1$ bandwidth for raw loads.

**Uniform address optimization:** Like Volta, the new uniform address optimization no longer affects StructuredBuffers. My educated guess is that StructuredBuffers (like raw buffers) now use the same lower latency direct memory path. Nvidia most likely hasn't yet implemented uniform address optimization for these new memory operations. Turing uniform address optimization performance however (in other cases) returns to similar 20x+ figures than Maxwell/Pascal.

### Intel Gen9 (HD 630 / i7 6700K)
```markdown
Buffer<R8>.Load uniform: 48.527ms 5.955x
Buffer<R8>.Load linear: 243.487ms 1.187x
Buffer<R8>.Load random: 286.351ms 1.009x
Buffer<RG8>.Load uniform: 49.022ms 5.895x
Buffer<RG8>.Load linear: 242.316ms 1.193x
Buffer<RG8>.Load random: 288.927ms 1.000x
Buffer<RGBA8>.Load uniform: 48.962ms 5.902x
Buffer<RGBA8>.Load linear: 244.140ms 1.184x
Buffer<RGBA8>.Load random: 288.981ms 1.000x
Buffer<R16f>.Load uniform: 49.989ms 5.781x
Buffer<R16f>.Load linear: 242.649ms 1.191x
Buffer<R16f>.Load random: 287.790ms 1.004x
Buffer<RG16f>.Load uniform: 48.921ms 5.907x
Buffer<RG16f>.Load linear: 243.826ms 1.185x
Buffer<RG16f>.Load random: 286.305ms 1.009x
Buffer<RGBA16f>.Load uniform: 48.855ms 5.915x
Buffer<RGBA16f>.Load linear: 242.278ms 1.193x
Buffer<RGBA16f>.Load random: 288.235ms 1.003x
Buffer<R32f>.Load uniform: 49.272ms 5.865x
Buffer<R32f>.Load linear: 241.286ms 1.198x
Buffer<R32f>.Load random: 286.946ms 1.007x
Buffer<RG32f>.Load uniform: 48.587ms 5.948x
Buffer<RG32f>.Load linear: 242.442ms 1.192x
Buffer<RG32f>.Load random: 287.429ms 1.005x
Buffer<RGBA32f>.Load uniform: 48.562ms 5.951x
Buffer<RGBA32f>.Load linear: 241.818ms 1.195x
Buffer<RGBA32f>.Load random: 287.268ms 1.006x
ByteAddressBuffer.Load uniform: 15.647ms 18.469x
ByteAddressBuffer.Load linear: 49.962ms 5.784x
ByteAddressBuffer.Load random: 51.418ms 5.620x
ByteAddressBuffer.Load2 uniform: 13.941ms 20.728x
ByteAddressBuffer.Load2 linear: 93.546ms 3.089x
ByteAddressBuffer.Load2 random: 140.016ms 2.064x
ByteAddressBuffer.Load3 uniform: 19.754ms 14.629x
ByteAddressBuffer.Load3 linear: 168.581ms 1.714x
ByteAddressBuffer.Load3 random: 312.721ms 0.924x
ByteAddressBuffer.Load4 uniform: 13.932ms 20.743x
ByteAddressBuffer.Load4 linear: 175.224ms 1.649x
ByteAddressBuffer.Load4 random: 340.677ms 0.848x
ByteAddressBuffer.Load2 unaligned uniform: 15.152ms 19.072x
ByteAddressBuffer.Load2 unaligned linear: 99.901ms 2.893x
ByteAddressBuffer.Load2 unaligned random: 145.827ms 1.982x
ByteAddressBuffer.Load4 unaligned uniform: 16.249ms 17.784x
ByteAddressBuffer.Load4 unaligned linear: 199.205ms 1.451x
ByteAddressBuffer.Load4 unaligned random: 378.326ms 0.764x
StructuredBuffer<float>.Load uniform: 14.309ms 20.195x
StructuredBuffer<float>.Load linear: 50.181ms 5.759x
StructuredBuffer<float>.Load random: 51.750ms 5.584x
StructuredBuffer<float2>.Load uniform: 13.856ms 20.856x
StructuredBuffer<float2>.Load linear: 94.388ms 3.062x
StructuredBuffer<float2>.Load random: 141.301ms 2.045x
StructuredBuffer<float4>.Load uniform: 13.493ms 21.417x
StructuredBuffer<float4>.Load linear: 175.457ms 1.647x
StructuredBuffer<float4>.Load random: 340.806ms 0.848x
cbuffer{float4} load uniform: 13.443ms 21.497x
cbuffer{float4} load linear: 242.860ms 1.190x
cbuffer{float4} load random: 285.850ms 1.011x
Texture2D<R8>.Load uniform: 24.519ms 11.786x
Texture2D<R8>.Load linear: 97.392ms 2.967x
Texture2D<R8>.Load random: 97.824ms 2.954x
Texture2D<RG8>.Load uniform: 24.376ms 11.855x
Texture2D<RG8>.Load linear: 97.068ms 2.977x
Texture2D<RG8>.Load random: 97.767ms 2.956x
Texture2D<RGBA8>.Load uniform: 24.509ms 11.791x
Texture2D<RGBA8>.Load linear: 101.171ms 2.856x
Texture2D<RGBA8>.Load random: 101.069ms 2.859x
Texture2D<R16F>.Load uniform: 24.874ms 11.618x
Texture2D<R16F>.Load linear: 97.947ms 2.950x
Texture2D<R16F>.Load random: 97.385ms 2.967x
Texture2D<RG16F>.Load uniform: 24.324ms 11.881x
Texture2D<RG16F>.Load linear: 98.257ms 2.941x
Texture2D<RG16F>.Load random: 97.672ms 2.959x
Texture2D<RGBA16F>.Load uniform: 24.408ms 11.840x
Texture2D<RGBA16F>.Load linear: 101.515ms 2.847x
Texture2D<RGBA16F>.Load random: 195.229ms 1.480x
Texture2D<R32F>.Load uniform: 24.677ms 11.710x
Texture2D<R32F>.Load linear: 97.829ms 2.954x
Texture2D<R32F>.Load random: 97.614ms 2.960x
Texture2D<RG32F>.Load uniform: 24.859ms 11.625x
Texture2D<RG32F>.Load linear: 97.809ms 2.955x
Texture2D<RG32F>.Load random: 194.397ms 1.487x
Texture2D<RGBA32F>.Load uniform: 24.660ms 11.719x
Texture2D<RGBA32F>.Load linear: 243.432ms 1.187x
Texture2D<RGBA32F>.Load random: 195.579ms 1.478x
 
NOTE: Intel result not directly comparable with other GPUs. I had to reduce workload size to avoid TDR.
```

**Typed loads:** All typed loads have same identical performance. Dimensions (1d/2d/4d) and channel widths (8b/16b/32b) don't affect performance. Intel compiler has a fast path for uniform address loads. It improves performance by up to 6x. Linear typed loads do not coalesce. Best bytes per cycle rate can be achieved by widest RGBA32 loads.

**Raw (ByteAddressBuffer) loads:** Intel raw buffer loads are significantly faster compared to similar typed loads. 1d raw load is 5x faster than any typed load. 2d linear raw load is 2.5x faster than typed loads. 4d linear raw load is 40% faster than typed loads. 2d/4d random raw loads are around 2x slower compared to linear ones (could be coalescing or something else). 3d raw load performance matches 4d. Alignment doesn't seem to matter. Uniform address raw loads also use the same compiler fast path as typed loads (6x gain).

**Structured buffer loads:** Performance is identical to similar raw buffer loads.

**Cbuffer loads:** 22x faster than normal load for fully uniform address. Linear/random access performance identical to typed buffer loads. Raw/structured buffers are up to 2x faster if you have linear/random access pattern.

**Texture loads:** All formats perform similarly, except the widest RGBA32 (half speed linear). Uniform address texture loads are 4x faster than linear. There's certainly something fishy going on, as Texture2D loads are generally 2x+ faster than same format buffer loads. Maybe I am hitting some bank conflict case or Intel is swizzling the buffer layout.

**Suggestions:** When using typed buffers, prefer widest loads (RGBA32). Raw buffers are significantly faster than typed buffers. 

**Uniform address optimization:** Uniform address loads are very fast (both raw and typed). Intel has confirmed that their compiler uses a wave shuffle trick to speed up uniform loads inside loops. See "Uniform Address Load Investigation" chapter for more info. 

# Branch with 2 samples + independent ALU workload benchmark

### NVIDIA GeForce GTX 770
```markdown
0X Coherent branch baseline: 26.318ms 1.000x
0X Coherent branch long: 19.209ms 1.370x
0X Coherent branch short: 19.276ms 1.365x
0X Divergent branch baseline: 26.264ms 1.002x
0X Divergent branch long: 24.007ms 1.096x
0X Divergent branch short: 24.098ms 1.092x
4X Coherent branch baseline: 26.792ms 1.000x
4X Coherent branch long: 19.841ms 1.350x
4X Coherent branch short: 19.944ms 1.343x
4X Divergent branch baseline: 26.595ms 1.007x
4X Divergent branch long: 23.828ms 1.124x
4X Divergent branch short: 23.985ms 1.117x
8X Coherent branch baseline: 26.393ms 1.000x
8X Coherent branch long: 20.241ms 1.304x
8X Coherent branch short: 20.155ms 1.309x
8X Divergent branch baseline: 26.651ms 0.990x
8X Divergent branch long: 24.115ms 1.094x
8X Divergent branch short: 24.038ms 1.098x
12X Coherent branch baseline: 26.288ms 1.000x
12X Coherent branch long: 20.106ms 1.307x
12X Coherent branch short: 20.379ms 1.290x
12X Divergent branch baseline: 26.352ms 0.998x
12X Divergent branch long: 24.315ms 1.081x
12X Divergent branch short: 24.225ms 1.085x
16X Coherent branch baseline: 26.682ms 1.000x
16X Coherent branch long: 20.287ms 1.315x
16X Coherent branch short: 20.356ms 1.311x
16X Divergent branch baseline: 26.303ms 1.014x
16X Divergent branch long: 24.390ms 1.094x
16X Divergent branch short: 24.572ms 1.086x
20X Coherent branch baseline: 26.444ms 1.000x
20X Coherent branch long: 20.519ms 1.289x
20X Coherent branch short: 20.584ms 1.285x
20X Divergent branch baseline: 26.275ms 1.006x
20X Divergent branch long: 25.602ms 1.033x
20X Divergent branch short: 23.944ms 1.104x
24X Coherent branch baseline: 26.358ms 1.000x
24X Coherent branch long: 20.809ms 1.267x
24X Coherent branch short: 21.027ms 1.254x
24X Divergent branch baseline: 26.181ms 1.007x
24X Divergent branch long: 27.519ms 0.958x
24X Divergent branch short: 24.041ms 1.096x
28X Coherent branch baseline: 26.313ms 1.000x
28X Coherent branch long: 21.159ms 1.244x
28X Coherent branch short: 21.599ms 1.218x
28X Divergent branch baseline: 26.265ms 1.002x
28X Divergent branch long: 29.789ms 0.883x
28X Divergent branch short: 24.053ms 1.094x
32X Coherent branch baseline: 26.365ms 1.000x
32X Coherent branch long: 21.709ms 1.214x
32X Coherent branch short: 22.248ms 1.185x
32X Divergent branch baseline: 26.327ms 1.001x
32X Divergent branch long: 31.909ms 0.826x
32X Divergent branch short: 24.547ms 1.074x
36X Coherent branch baseline: 26.410ms 1.000x
36X Coherent branch long: 22.280ms 1.185x
36X Coherent branch short: 22.885ms 1.154x
36X Divergent branch baseline: 26.323ms 1.003x
36X Divergent branch long: 33.986ms 0.777x
36X Divergent branch short: 25.314ms 1.043x
40X Coherent branch baseline: 26.399ms 1.000x
40X Coherent branch long: 22.784ms 1.159x
40X Coherent branch short: 23.572ms 1.120x
40X Divergent branch baseline: 26.272ms 1.005x
40X Divergent branch long: 36.025ms 0.733x
40X Divergent branch short: 26.675ms 0.990x
44X Coherent branch baseline: 26.408ms 1.000x
44X Coherent branch long: 23.326ms 1.132x
44X Coherent branch short: 24.406ms 1.082x
44X Divergent branch baseline: 26.275ms 1.005x
44X Divergent branch long: 38.063ms 0.694x
44X Divergent branch short: 27.953ms 0.945x
48X Coherent branch baseline: 26.401ms 1.000x
48X Coherent branch long: 24.143ms 1.094x
48X Coherent branch short: 25.270ms 1.045x
48X Divergent branch baseline: 26.277ms 1.005x
48X Divergent branch long: 40.459ms 0.653x
48X Divergent branch short: 28.947ms 0.912x
52X Coherent branch baseline: 26.400ms 1.000x
52X Coherent branch long: 25.001ms 1.056x
52X Coherent branch short: 26.127ms 1.010x
52X Divergent branch baseline: 26.299ms 1.004x
52X Divergent branch long: 42.604ms 0.620x
52X Divergent branch short: 29.293ms 0.901x
56X Coherent branch baseline: 26.427ms 1.000x
56X Coherent branch long: 25.864ms 1.022x
56X Coherent branch short: 26.904ms 0.982x
56X Divergent branch baseline: 26.319ms 1.004x
56X Divergent branch long: 45.010ms 0.587x
56X Divergent branch short: 28.903ms 0.914x
60X Coherent branch baseline: 26.367ms 1.000x
60X Coherent branch long: 26.778ms 0.985x
60X Coherent branch short: 27.770ms 0.949x
60X Divergent branch baseline: 26.298ms 1.003x
60X Divergent branch long: 47.292ms 0.558x
60X Divergent branch short: 28.724ms 0.918x
64X Coherent branch baseline: 26.273ms 1.000x
64X Coherent branch long: 27.806ms 0.945x
64X Coherent branch short: 28.895ms 0.909x
64X Divergent branch baseline: 26.209ms 1.002x
64X Divergent branch long: 49.676ms 0.529x
64X Divergent branch short: 29.809ms 0.881x
68X Coherent branch baseline: 26.297ms 1.000x
68X Coherent branch long: 28.871ms 0.911x
68X Coherent branch short: 30.042ms 0.875x
68X Divergent branch baseline: 26.319ms 0.999x
68X Divergent branch long: 51.600ms 0.510x
68X Divergent branch short: 31.036ms 0.847x
72X Coherent branch baseline: 27.472ms 1.000x
72X Coherent branch long: 30.004ms 0.916x
72X Coherent branch short: 31.324ms 0.877x
72X Divergent branch baseline: 27.512ms 0.999x
72X Divergent branch long: 54.296ms 0.506x
72X Divergent branch short: 32.277ms 0.851x
76X Coherent branch baseline: 28.637ms 1.000x
76X Coherent branch long: 31.135ms 0.920x
76X Coherent branch short: 32.602ms 0.878x
76X Divergent branch baseline: 28.624ms 1.000x
76X Divergent branch long: 56.591ms 0.506x
76X Divergent branch short: 33.500ms 0.855x
80X Coherent branch baseline: 29.869ms 1.000x
80X Coherent branch long: 32.322ms 0.924x
80X Coherent branch short: 33.887ms 0.881x
80X Divergent branch baseline: 29.864ms 1.000x
80X Divergent branch long: 58.899ms 0.507x
80X Divergent branch short: 34.735ms 0.860x
84X Coherent branch baseline: 31.230ms 1.000x
84X Coherent branch long: 33.563ms 0.930x
84X Coherent branch short: 35.180ms 0.888x
84X Divergent branch baseline: 31.220ms 1.000x
84X Divergent branch long: 61.431ms 0.508x
84X Divergent branch short: 35.992ms 0.868x
88X Coherent branch baseline: 32.424ms 1.000x
88X Coherent branch long: 34.794ms 0.932x
88X Coherent branch short: 36.482ms 0.889x
88X Divergent branch baseline: 32.449ms 0.999x
88X Divergent branch long: 63.774ms 0.508x
88X Divergent branch short: 37.313ms 0.869x
92X Coherent branch baseline: 33.782ms 1.000x
92X Coherent branch long: 36.005ms 0.938x
92X Coherent branch short: 37.762ms 0.895x
92X Divergent branch baseline: 33.778ms 1.000x
92X Divergent branch long: 66.431ms 0.509x
92X Divergent branch short: 38.607ms 0.875x
96X Coherent branch baseline: 34.924ms 1.000x
96X Coherent branch long: 37.268ms 0.937x
96X Coherent branch short: 39.015ms 0.895x
96X Divergent branch baseline: 34.930ms 1.000x
96X Divergent branch long: 68.651ms 0.509x
96X Divergent branch short: 39.917ms 0.875x
100X Coherent branch baseline: 36.279ms 1.000x
100X Coherent branch long: 38.464ms 0.943x
100X Coherent branch short: 40.288ms 0.900x
100X Divergent branch baseline: 36.270ms 1.000x
100X Divergent branch long: 70.750ms 0.513x
100X Divergent branch short: 41.214ms 0.880x
104X Coherent branch baseline: 37.558ms 1.000x
104X Coherent branch long: 39.736ms 0.945x
104X Coherent branch short: 41.497ms 0.905x
104X Divergent branch baseline: 37.561ms 1.000x
104X Divergent branch long: 73.335ms 0.512x
104X Divergent branch short: 42.479ms 0.884x
108X Coherent branch baseline: 38.818ms 1.000x
108X Coherent branch long: 41.012ms 0.947x
108X Coherent branch short: 42.824ms 0.906x
108X Divergent branch baseline: 38.824ms 1.000x
108X Divergent branch long: 76.028ms 0.511x
108X Divergent branch short: 43.719ms 0.888x
112X Coherent branch baseline: 40.179ms 1.000x
112X Coherent branch long: 42.178ms 0.953x
112X Coherent branch short: 44.133ms 0.910x
112X Divergent branch baseline: 40.185ms 1.000x
112X Divergent branch long: 77.956ms 0.515x
112X Divergent branch short: 44.902ms 0.895x
116X Coherent branch baseline: 41.314ms 1.000x
116X Coherent branch long: 43.502ms 0.950x
116X Coherent branch short: 45.474ms 0.909x
116X Divergent branch baseline: 41.332ms 1.000x
116X Divergent branch long: 81.014ms 0.510x
116X Divergent branch short: 45.982ms 0.898x
120X Coherent branch baseline: 42.664ms 1.000x
120X Coherent branch long: 44.750ms 0.953x
120X Coherent branch short: 46.691ms 0.914x
120X Divergent branch baseline: 42.648ms 1.000x
120X Divergent branch long: 82.450ms 0.517x
120X Divergent branch short: 47.266ms 0.903x
124X Coherent branch baseline: 43.975ms 1.000x
124X Coherent branch long: 46.040ms 0.955x
124X Coherent branch short: 48.039ms 0.915x
124X Divergent branch baseline: 43.953ms 1.001x
124X Divergent branch long: 85.529ms 0.514x
124X Divergent branch short: 48.538ms 0.906x
128X Coherent branch baseline: 45.478ms 1.000x
128X Coherent branch long: 47.235ms 0.963x
128X Coherent branch short: 49.364ms 0.921x
128X Divergent branch baseline: 45.496ms 1.000x
128X Divergent branch long: 88.196ms 0.516x
128X Divergent branch short: 49.815ms 0.913x
```

* 72X starts to slow down the baseline
* After 20X Divergent branch long blows up
* Coherent branch long is always faster than Coherent branch short
* Divergent branch long is always slower thatn Divergent branch short
* Divergent branch short is almost the same as the baseline

### Intel Gen9 (HD 630 / i7 6700K)
```markdown
0X Coherent branch baseline: 95.884ms 1.000x
0X Coherent branch long: 67.906ms 1.412x
0X Coherent branch short: 66.874ms 1.434x
0X Divergent branch baseline: 87.102ms 1.101x
0X Divergent branch long: 97.985ms 0.979x
0X Divergent branch short: 91.838ms 1.044x
8X Coherent branch baseline: 86.106ms 1.000x
8X Coherent branch long: 68.631ms 1.255x
8X Coherent branch short: 76.284ms 1.129x
8X Divergent branch baseline: 93.944ms 0.917x
8X Divergent branch long: 87.145ms 0.988x
8X Divergent branch short: 93.708ms 0.919x
16X Coherent branch baseline: 92.081ms 1.000x
16X Coherent branch long: 71.506ms 1.288x
16X Coherent branch short: 70.035ms 1.315x
16X Divergent branch baseline: 90.125ms 1.022x
16X Divergent branch long: 94.059ms 0.979x
16X Divergent branch short: 90.341ms 1.019x
24X Coherent branch baseline: 92.903ms 1.000x
24X Coherent branch long: 75.832ms 1.225x
24X Coherent branch short: 73.111ms 1.271x
24X Divergent branch baseline: 94.935ms 0.979x
24X Divergent branch long: 92.934ms 1.000x
24X Divergent branch short: 92.574ms 1.004x
32X Coherent branch baseline: 94.784ms 1.000x
32X Coherent branch long: 67.525ms 1.404x
32X Coherent branch short: 70.970ms 1.336x
32X Divergent branch baseline: 89.981ms 1.053x
32X Divergent branch long: 96.876ms 0.978x
32X Divergent branch short: 93.454ms 1.014x
40X Coherent branch baseline: 89.957ms 1.000x
40X Coherent branch long: 70.196ms 1.282x
40X Coherent branch short: 67.837ms 1.326x
40X Divergent branch baseline: 93.629ms 0.961x
40X Divergent branch long: 89.371ms 1.007x
40X Divergent branch short: 94.511ms 0.952x
48X Coherent branch baseline: 96.918ms 1.000x
48X Coherent branch long: 68.388ms 1.417x
48X Coherent branch short: 69.896ms 1.387x
48X Divergent branch baseline: 93.376ms 1.038x
48X Divergent branch long: 96.296ms 1.006x
48X Divergent branch short: 90.259ms 1.074x
56X Coherent branch baseline: 95.070ms 1.000x
56X Coherent branch long: 74.325ms 1.279x
56X Coherent branch short: 74.685ms 1.273x
56X Divergent branch baseline: 94.223ms 1.009x
56X Divergent branch long: 105.795ms 0.899x
56X Divergent branch short: 93.089ms 1.021x
64X Coherent branch baseline: 89.277ms 1.000x
64X Coherent branch long: 71.419ms 1.250x
64X Coherent branch short: 71.830ms 1.243x
64X Divergent branch baseline: 95.267ms 0.937x
64X Divergent branch long: 119.080ms 0.750x
64X Divergent branch short: 95.171ms 0.938x
72X Coherent branch baseline: 92.232ms 1.000x
72X Coherent branch long: 78.753ms 1.171x
72X Coherent branch short: 80.208ms 1.150x
72X Divergent branch baseline: 90.969ms 1.014x
72X Divergent branch long: 126.631ms 0.728x
72X Divergent branch short: 89.133ms 1.035x
80X Coherent branch baseline: 90.027ms 1.000x
80X Coherent branch long: 80.125ms 1.124x
80X Coherent branch short: 82.503ms 1.091x
80X Divergent branch baseline: 96.368ms 0.934x
80X Divergent branch long: 138.997ms 0.648x
80X Divergent branch short: 89.785ms 1.003x
88X Coherent branch baseline: 92.398ms 1.000x
88X Coherent branch long: 79.283ms 1.165x
88X Coherent branch short: 84.620ms 1.092x
88X Divergent branch baseline: 99.743ms 0.926x
88X Divergent branch long: 151.399ms 0.610x
88X Divergent branch short: 93.616ms 0.987x
96X Coherent branch baseline: 91.753ms 1.000x
96X Coherent branch long: 82.419ms 1.113x
96X Coherent branch short: 81.816ms 1.121x
96X Divergent branch baseline: 98.073ms 0.936x
96X Divergent branch long: 166.351ms 0.552x
96X Divergent branch short: 96.874ms 0.947x
104X Coherent branch baseline: 90.740ms 1.000x
104X Coherent branch long: 85.646ms 1.059x
104X Coherent branch short: 91.458ms 0.992x
104X Divergent branch baseline: 97.022ms 0.935x
104X Divergent branch long: 170.800ms 0.531x
104X Divergent branch short: 101.488ms 0.894x
112X Coherent branch baseline: 90.125ms 1.000x
112X Coherent branch long: 93.796ms 0.961x
112X Coherent branch short: 92.986ms 0.969x
112X Divergent branch baseline: 90.024ms 1.001x
112X Divergent branch long: 190.517ms 0.473x
112X Divergent branch short: 101.119ms 0.891x
120X Coherent branch baseline: 91.374ms 1.000x
120X Coherent branch long: 98.964ms 0.923x
120X Coherent branch short: 92.648ms 0.986x
120X Divergent branch baseline: 95.411ms 0.958x
120X Divergent branch long: 201.996ms 0.452x
120X Divergent branch short: 101.146ms 0.903x
128X Coherent branch baseline: 95.215ms 1.000x
128X Coherent branch long: 104.423ms 0.912x
128X Coherent branch short: 99.867ms 0.953x
128X Divergent branch baseline: 99.782ms 0.954x
128X Divergent branch long: 213.200ms 0.447x
128X Divergent branch short: 112.398ms 0.847x
136X Coherent branch baseline: 97.463ms 1.000x
136X Coherent branch long: 107.451ms 0.907x
136X Coherent branch short: 103.539ms 0.941x
136X Divergent branch baseline: 97.500ms 1.000x
136X Divergent branch long: 231.513ms 0.421x
136X Divergent branch short: 106.989ms 0.911x
144X Coherent branch baseline: 105.036ms 1.000x
144X Coherent branch long: 117.894ms 0.891x
144X Coherent branch short: 100.370ms 1.046x
144X Divergent branch baseline: 98.343ms 1.068x
144X Divergent branch long: 241.916ms 0.434x
144X Divergent branch short: 116.798ms 0.899x
152X Coherent branch baseline: 112.307ms 1.000x
152X Coherent branch long: 116.730ms 0.962x
152X Coherent branch short: 114.441ms 0.981x
152X Divergent branch baseline: 111.552ms 1.007x
152X Divergent branch long: 252.075ms 0.446x
152X Divergent branch short: 118.253ms 0.950x
160X Coherent branch baseline: 117.538ms 1.000x
160X Coherent branch long: 124.383ms 0.945x
160X Coherent branch short: 117.636ms 0.999x
160X Divergent branch baseline: 114.494ms 1.027x
160X Divergent branch long: 268.620ms 0.438x
160X Divergent branch short: 122.380ms 0.960x
168X Coherent branch baseline: 113.685ms 1.000x
168X Coherent branch long: 132.730ms 0.857x
168X Coherent branch short: 122.659ms 0.927x
168X Divergent branch baseline: 120.517ms 0.943x
168X Divergent branch long: 280.049ms 0.406x
168X Divergent branch short: 127.183ms 0.894x
176X Coherent branch baseline: 126.390ms 1.000x
176X Coherent branch long: 128.652ms 0.982x
176X Coherent branch short: 130.558ms 0.968x
176X Divergent branch baseline: 119.258ms 1.060x
176X Divergent branch long: 301.794ms 0.419x
176X Divergent branch short: 132.429ms 0.954x
184X Coherent branch baseline: 134.882ms 1.000x
184X Coherent branch long: 141.134ms 0.956x
184X Coherent branch short: 128.838ms 1.047x
184X Divergent branch baseline: 127.537ms 1.058x
184X Divergent branch long: 308.576ms 0.437x
184X Divergent branch short: 142.095ms 0.949x
192X Coherent branch baseline: 133.612ms 1.000x
192X Coherent branch long: 143.528ms 0.931x
192X Coherent branch short: 141.848ms 0.942x
192X Divergent branch baseline: 136.251ms 0.981x
192X Divergent branch long: 313.380ms 0.426x
192X Divergent branch short: 141.584ms 0.944x
200X Coherent branch baseline: 142.891ms 1.000x
200X Coherent branch long: 149.795ms 0.954x
200X Coherent branch short: 144.693ms 0.988x
200X Divergent branch baseline: 135.988ms 1.051x
200X Divergent branch long: 331.469ms 0.431x
200X Divergent branch short: 146.600ms 0.975x
208X Coherent branch baseline: 146.082ms 1.000x
208X Coherent branch long: 161.584ms 0.904x
208X Coherent branch short: 151.168ms 0.966x
208X Divergent branch baseline: 140.465ms 1.040x
208X Divergent branch long: 342.970ms 0.426x
208X Divergent branch short: 155.150ms 0.942x
216X Coherent branch baseline: 146.258ms 1.000x
216X Coherent branch long: 165.700ms 0.883x
216X Coherent branch short: 154.826ms 0.945x
216X Divergent branch baseline: 148.106ms 0.988x
216X Divergent branch long: 361.852ms 0.404x
216X Divergent branch short: 162.214ms 0.902x
224X Coherent branch baseline: 154.300ms 1.000x
224X Coherent branch long: 163.994ms 0.941x
224X Coherent branch short: 159.309ms 0.969x
224X Divergent branch baseline: 156.236ms 0.988x
224X Divergent branch long: 364.883ms 0.423x
224X Divergent branch short: 168.363ms 0.916x
232X Coherent branch baseline: 160.544ms 1.000x
232X Coherent branch long: 182.370ms 0.880x
232X Coherent branch short: 157.124ms 1.022x
232X Divergent branch baseline: 156.272ms 1.027x
232X Divergent branch long: 382.136ms 0.420x
232X Divergent branch short: 170.397ms 0.942x
240X Coherent branch baseline: 169.030ms 1.000x
240X Coherent branch long: 182.726ms 0.925x
240X Coherent branch short: 163.967ms 1.031x
240X Divergent branch baseline: 162.108ms 1.043x
240X Divergent branch long: 395.592ms 0.427x
240X Divergent branch short: 175.645ms 0.962x
248X Coherent branch baseline: 173.679ms 1.000x
248X Coherent branch long: 180.350ms 0.963x
248X Coherent branch short: 171.743ms 1.011x
248X Divergent branch baseline: 167.109ms 1.039x
248X Divergent branch long: 414.666ms 0.419x
248X Divergent branch short: 182.510ms 0.952x
256X Coherent branch baseline: 179.713ms 1.000x
256X Coherent branch long: 187.313ms 0.959x
256X Coherent branch short: 179.552ms 1.001x
256X Divergent branch baseline: 180.879ms 0.994x
256X Divergent branch long: 420.496ms 0.427x
256X Divergent branch short: 189.113ms 0.950x

```
* 144X starts to slow down the baseline

### NVIDIA GeForce GTX 1050

```markdown
0X Coherent branch baseline: 47.788ms 1.000x
0X Coherent branch long: 32.794ms 1.457x
0X Coherent branch short: 34.335ms 1.392x
0X Divergent branch baseline: 42.955ms 1.113x
0X Divergent branch long: 39.428ms 1.212x
0X Divergent branch short: 40.560ms 1.178x
8X Coherent branch baseline: 50.149ms 1.000x
8X Coherent branch long: 29.928ms 1.676x
8X Coherent branch short: 31.086ms 1.613x
8X Divergent branch baseline: 41.783ms 1.200x
8X Divergent branch long: 40.635ms 1.234x
8X Divergent branch short: 41.487ms 1.209x
16X Coherent branch baseline: 48.178ms 1.000x
16X Coherent branch long: 30.459ms 1.582x
16X Coherent branch short: 33.073ms 1.457x
16X Divergent branch baseline: 42.595ms 1.131x
16X Divergent branch long: 38.941ms 1.237x
16X Divergent branch short: 39.762ms 1.212x
24X Coherent branch baseline: 49.401ms 1.000x
24X Coherent branch long: 31.906ms 1.548x
24X Coherent branch short: 31.136ms 1.587x
24X Divergent branch baseline: 42.493ms 1.163x
24X Divergent branch long: 39.964ms 1.236x
24X Divergent branch short: 40.397ms 1.223x
32X Coherent branch baseline: 49.492ms 1.000x
32X Coherent branch long: 31.207ms 1.586x
32X Coherent branch short: 31.842ms 1.554x
32X Divergent branch baseline: 42.922ms 1.153x
32X Divergent branch long: 41.751ms 1.185x
32X Divergent branch short: 39.370ms 1.257x
40X Coherent branch baseline: 47.435ms 1.000x
40X Coherent branch long: 32.245ms 1.471x
40X Coherent branch short: 32.660ms 1.452x
40X Divergent branch baseline: 41.762ms 1.136x
40X Divergent branch long: 45.815ms 1.035x
40X Divergent branch short: 38.928ms 1.219x
48X Coherent branch baseline: 47.820ms 1.000x
48X Coherent branch long: 33.318ms 1.435x
48X Coherent branch short: 33.706ms 1.419x
48X Divergent branch baseline: 42.399ms 1.128x
48X Divergent branch long: 50.613ms 0.945x
48X Divergent branch short: 38.892ms 1.230x
56X Coherent branch baseline: 47.522ms 1.000x
56X Coherent branch long: 34.725ms 1.369x
56X Coherent branch short: 35.782ms 1.328x
56X Divergent branch baseline: 41.863ms 1.135x
56X Divergent branch long: 55.825ms 0.851x
56X Divergent branch short: 39.019ms 1.218x
64X Coherent branch baseline: 47.752ms 1.000x
64X Coherent branch long: 36.477ms 1.309x
64X Coherent branch short: 37.117ms 1.287x
64X Divergent branch baseline: 42.586ms 1.121x
64X Divergent branch long: 61.334ms 0.779x
64X Divergent branch short: 39.536ms 1.208x
72X Coherent branch baseline: 47.263ms 1.000x
72X Coherent branch long: 39.098ms 1.209x
72X Coherent branch short: 39.420ms 1.199x
72X Divergent branch baseline: 41.804ms 1.131x
72X Divergent branch long: 66.467ms 0.711x
72X Divergent branch short: 42.374ms 1.115x
80X Coherent branch baseline: 47.189ms 1.000x
80X Coherent branch long: 41.522ms 1.136x
80X Coherent branch short: 42.663ms 1.106x
80X Divergent branch baseline: 41.994ms 1.124x
80X Divergent branch long: 72.135ms 0.654x
80X Divergent branch short: 44.155ms 1.069x
88X Coherent branch baseline: 47.478ms 1.000x
88X Coherent branch long: 43.632ms 1.088x
88X Coherent branch short: 44.576ms 1.065x
88X Divergent branch baseline: 42.081ms 1.128x
88X Divergent branch long: 77.273ms 0.614x
88X Divergent branch short: 47.405ms 1.002x
96X Coherent branch baseline: 47.961ms 1.000x
96X Coherent branch long: 46.322ms 1.035x
96X Coherent branch short: 47.274ms 1.015x
96X Divergent branch baseline: 42.492ms 1.129x
96X Divergent branch long: 83.103ms 0.577x
96X Divergent branch short: 49.425ms 0.970x
104X Coherent branch baseline: 47.451ms 1.000x
104X Coherent branch long: 49.029ms 0.968x
104X Coherent branch short: 50.224ms 0.945x
104X Divergent branch baseline: 44.676ms 1.062x
104X Divergent branch long: 88.759ms 0.535x
104X Divergent branch short: 52.223ms 0.909x
112X Coherent branch baseline: 47.926ms 1.000x
112X Coherent branch long: 52.105ms 0.920x
112X Coherent branch short: 53.077ms 0.903x
112X Divergent branch baseline: 47.065ms 1.018x
112X Divergent branch long: 93.785ms 0.511x
112X Divergent branch short: 55.049ms 0.871x
120X Coherent branch baseline: 50.713ms 1.000x
120X Coherent branch long: 54.564ms 0.929x
120X Coherent branch short: 55.848ms 0.908x
120X Divergent branch baseline: 50.200ms 1.010x
120X Divergent branch long: 99.971ms 0.507x
120X Divergent branch short: 57.843ms 0.877x
128X Coherent branch baseline: 52.745ms 1.000x
128X Coherent branch long: 57.168ms 0.923x
128X Coherent branch short: 58.705ms 0.898x
128X Divergent branch baseline: 52.439ms 1.006x
128X Divergent branch long: 104.256ms 0.506x
128X Divergent branch short: 62.555ms 0.843x
136X Coherent branch baseline: 55.377ms 1.000x
136X Coherent branch long: 60.483ms 0.916x
136X Coherent branch short: 61.728ms 0.897x
136X Divergent branch baseline: 55.239ms 1.003x
136X Divergent branch long: 109.767ms 0.504x
136X Divergent branch short: 63.460ms 0.873x
144X Coherent branch baseline: 58.892ms 1.000x
144X Coherent branch long: 62.812ms 0.938x
144X Coherent branch short: 64.751ms 0.910x
144X Divergent branch baseline: 58.702ms 1.003x
144X Divergent branch long: 115.669ms 0.509x
144X Divergent branch short: 66.748ms 0.882x
152X Coherent branch baseline: 60.819ms 1.000x
152X Coherent branch long: 65.411ms 0.930x
152X Coherent branch short: 67.362ms 0.903x
152X Divergent branch baseline: 60.723ms 1.002x
152X Divergent branch long: 121.315ms 0.501x
152X Divergent branch short: 69.353ms 0.877x
160X Coherent branch baseline: 64.201ms 1.000x
160X Coherent branch long: 68.321ms 0.940x
160X Coherent branch short: 70.757ms 0.907x
160X Divergent branch baseline: 63.148ms 1.017x
160X Divergent branch long: 126.889ms 0.506x
160X Divergent branch short: 72.180ms 0.889x
168X Coherent branch baseline: 67.169ms 1.000x
168X Coherent branch long: 71.181ms 0.944x
168X Coherent branch short: 73.398ms 0.915x
168X Divergent branch baseline: 66.598ms 1.009x
168X Divergent branch long: 132.902ms 0.505x
168X Divergent branch short: 74.917ms 0.897x
176X Coherent branch baseline: 69.587ms 1.000x
176X Coherent branch long: 73.708ms 0.944x
176X Coherent branch short: 76.403ms 0.911x
176X Divergent branch baseline: 69.751ms 0.998x
176X Divergent branch long: 137.740ms 0.505x
176X Divergent branch short: 77.948ms 0.893x
184X Coherent branch baseline: 71.673ms 1.000x
184X Coherent branch long: 76.479ms 0.937x
184X Coherent branch short: 78.963ms 0.908x
184X Divergent branch baseline: 71.655ms 1.000x
184X Divergent branch long: 144.084ms 0.497x
184X Divergent branch short: 80.968ms 0.885x
192X Coherent branch baseline: 75.003ms 1.000x
192X Coherent branch long: 79.427ms 0.944x
192X Coherent branch short: 82.721ms 0.907x
192X Divergent branch baseline: 74.208ms 1.011x
192X Divergent branch long: 149.077ms 0.503x
192X Divergent branch short: 83.840ms 0.895x
200X Coherent branch baseline: 77.623ms 1.000x
200X Coherent branch long: 82.776ms 0.938x
200X Coherent branch short: 84.881ms 0.914x
200X Divergent branch baseline: 77.457ms 1.002x
200X Divergent branch long: 155.185ms 0.500x
200X Divergent branch short: 87.770ms 0.884x
208X Coherent branch baseline: 80.756ms 1.000x
208X Coherent branch long: 85.414ms 0.945x
208X Coherent branch short: 87.327ms 0.925x
208X Divergent branch baseline: 80.877ms 0.999x
208X Divergent branch long: 161.083ms 0.501x
208X Divergent branch short: 89.305ms 0.904x
216X Coherent branch baseline: 83.212ms 1.000x
216X Coherent branch long: 87.959ms 0.946x
216X Coherent branch short: 90.412ms 0.920x
216X Divergent branch baseline: 82.722ms 1.006x
216X Divergent branch long: 165.517ms 0.503x
216X Divergent branch short: 92.362ms 0.901x
224X Coherent branch baseline: 85.688ms 1.000x
224X Coherent branch long: 91.275ms 0.939x
224X Coherent branch short: 93.817ms 0.913x
224X Divergent branch baseline: 85.356ms 1.004x
224X Divergent branch long: 171.250ms 0.500x
224X Divergent branch short: 96.328ms 0.890x
232X Coherent branch baseline: 90.029ms 1.000x
232X Coherent branch long: 93.781ms 0.960x
232X Coherent branch short: 96.470ms 0.933x
232X Divergent branch baseline: 88.025ms 1.023x
232X Divergent branch long: 178.466ms 0.504x
232X Divergent branch short: 98.312ms 0.916x
240X Coherent branch baseline: 91.662ms 1.000x
240X Coherent branch long: 96.193ms 0.953x
240X Coherent branch short: 100.463ms 0.912x
240X Divergent branch baseline: 92.404ms 0.992x
240X Divergent branch long: 183.066ms 0.501x
240X Divergent branch short: 101.228ms 0.906x
248X Coherent branch baseline: 95.311ms 1.000x
248X Coherent branch long: 99.083ms 0.962x
248X Coherent branch short: 103.743ms 0.919x
248X Divergent branch baseline: 93.701ms 1.017x
248X Divergent branch long: 188.698ms 0.505x
248X Divergent branch short: 105.440ms 0.904x
256X Coherent branch baseline: 97.260ms 1.000x
256X Coherent branch long: 101.813ms 0.955x
256X Coherent branch short: 104.577ms 0.930x
256X Divergent branch baseline: 98.745ms 0.985x
256X Divergent branch long: 193.324ms 0.503x
256X Divergent branch short: 106.407ms 0.914x
```

## Contact

Send private message to @SebAaltonen at Twitter. We can discuss via company emails later.

## License

PerfTest is released under the MIT license. See [LICENSE.md](LICENSE.md) for full text.
