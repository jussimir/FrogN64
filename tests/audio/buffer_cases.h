// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "HLEAudio/AudioBuffer.h"
#include <cstdio>
#include <vector>
#include <algorithm>

static bool equal(Sample a, Sample b) { return a.L == b.L && a.R == b.R; }
static int fail(const char *message) { std::fprintf(stderr, "%s\n", message); return 1; }

static int fifo()
{
    CAudioBuffer buffer(32);
    const Sample input[] = {{1001,-1001},{2002,-2002},{3003,-3003},{4004,-4004}};
    buffer.AddSamples(input, 4, 44100, 44100);
    Sample output[4]{};
    const auto count = buffer.Drain(output, 4);
    if (count != 4) return fail("Equal-rate input lost samples.");
    for (int i=0; i<4; ++i)
        if (!equal(input[i], output[i])) return fail("FIFO sample order or stereo channels changed.");
    return 0;
}

static int silence()
{
    CAudioBuffer buffer(8);
    Sample output[4] = {{1,2},{3,4},{5,6},{7,8}};
    if (buffer.Drain(output, 4)) return fail("Empty ring returned samples.");
    for (auto sample : output) if (!equal(sample, {0,0})) return fail("Underrun did not produce silence.");
    const Sample input{32767,-32768};
    buffer.AddSamples(&input, 1, 44100, 44100);
    if (buffer.Drain(output, 4) != 1 || !equal(output[0], input)) return fail("Single-sample packet was lost.");
    for (int i=1; i<4; ++i) if (!equal(output[i], {0,0})) return fail("Tail padding is not silence.");
    buffer.AddSamples(nullptr, 0, 44100, 44100);
    buffer.AddSamples(&input, 1, 0, 44100);
    buffer.AddSamples(&input, 1, 44100, 0);
    return buffer.GetNumBufferedSamples() ? fail("Invalid rate/input changed the queue.") : 0;
}

static int interpolation()
{
    CAudioBuffer buffer(32);
    const Sample input[]={{0,0},{1000,-1000},{2000,-2000}};
    const Sample expected[]={{0,0},{500,-500},{1000,-1000},{1500,-1500},{2000,-2000}};
    buffer.AddSamples(input, 3, 22050, 44100);
    Sample output[8]{};
    if (buffer.Drain(output, 8) != 5) return fail("Upsample count differs from the known ramp.");
    for (int i=0; i<5; ++i) if (!equal(output[i], expected[i])) return fail("Known ramp interpolation differs.");
    const Sample lower[]={{0,0},{100,100},{200,200},{300,300},{400,400},{500,500},{600,600},{700,700}};
    buffer.AddSamples(lower, 8, 48000, 24000);
    if (buffer.Drain(output, 8) != 4) return fail("Downsample count differs.");
    for (int i=0; i<4; ++i) if (!equal(output[i], lower[i*2])) return fail("Downsample positions differ.");
    return 0;
}

static int packet_boundaries()
{
    std::vector<Sample> input(2048);
    for (size_t i=0; i<input.size(); ++i) input[i] = {static_cast<s16>(i*13-12000),static_cast<s16>(12000-i*11)};
    for (u32 rate : {u32(32000),u32(32006),u32(44100),u32(48000)})
    {
        CAudioBuffer whole(4096), split(4096);
        whole.AddSamples(input.data(), input.size(), rate, 44100);
        for (size_t offset=0; offset<input.size();)
        {
            const auto size=std::min<size_t>(1+offset%37, input.size()-offset);
            split.AddSamples(input.data()+offset, size, rate, 44100);
            offset+=size;
        }
        std::vector<Sample> a(4096),b(4096);
        const u32 n=whole.Drain(a.data(), a.size()), m=split.Drain(b.data(), b.size());
        const u32 expected=(static_cast<u64>(input.size()-1)*44100)/rate+1;
        if (n != m || n != expected) return fail("Rate or packet boundaries lost/duplicated samples.");
        for (u32 i=0; i<n; ++i) if (!equal(a[i],b[i])) return fail("Packet boundaries changed the waveform.");
    }
    return 0;
}

static int wrap()
{
    CAudioBuffer buffer(17);
    unsigned sequence=1;
    for (int round=0; round<1000; ++round)
    {
        Sample input[13],output[13];
        for (auto &sample : input) { sample={static_cast<s16>(sequence),static_cast<s16>(-sequence)}; ++sequence; }
        buffer.AddSamples(input, 13, 44100, 44100);
        if (buffer.Drain(output, 13) != 13) return fail("Wrap changed the number of samples.");
        for (int i=0; i<13; ++i) if (!equal(input[i],output[i])) return fail("Wrap reordered samples.");
    }
    return 0;
}
