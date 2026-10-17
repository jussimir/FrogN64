// SPDX-License-Identifier: GPL-2.0-or-later
#include "buffer_cases.h"
#include <thread>
#include <string>

void ThreadYield() { std::this_thread::yield(); }

static int full()
{
    CAudioBuffer buffer(17);
    std::vector<Sample> input(100000);
    for (size_t i=0; i<input.size(); ++i) input[i] = {static_cast<s16>(1+i%30000),static_cast<s16>(-1-static_cast<int>(i%30000))};
    std::thread producer([&] { buffer.AddSamples(input.data(), input.size(), 44100, 44100); });
    size_t count=0;
    bool good=true;
    while (count < input.size())
    {
        Sample output[8];
        const auto n=buffer.Drain(output, std::min<size_t>(8, input.size()-count));
        for (u32 i=0; i<n; ++i) good &= equal(output[i],input[count+i]);
        count+=n;
        if (!n) std::this_thread::yield();
    }
    producer.join();
    return good ? 0 : fail("Concurrent producer/consumer lost sample order.");
}

int main(int argc, char **argv)
{
    const std::string test=argc>1 ? argv[1] : "fifo";
    if (test=="fifo") return fifo();
    if (test=="silence") return silence();
    if (test=="interpolation") return interpolation();
    if (test=="packets") return packet_boundaries();
    if (test=="wrap") return wrap();
    if (test=="full") return full();
    return 2;
}
