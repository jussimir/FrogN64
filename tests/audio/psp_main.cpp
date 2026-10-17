// SPDX-License-Identifier: GPL-2.0-or-later
#include <pspkernel.h>
#include <pspaudio.h>
#include "buffer_cases.h"
#include <cmath>
#include <new>
#include <cstdlib>
#include "SysPSP/Utility/CacheUtil.h"

PSP_MODULE_INFO("FrogN64AudioTests", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-256);

void ThreadYield() { sceKernelDelayThread(1); }

int main()
{
    const int failures = fifo() + silence() + interpolation() + packet_boundaries() + wrap();
    if (failures) {
        std::printf("FROGN64_AUDIO_TESTS_FAIL count=%d\n", failures);
        sceKernelExitGame();
        return 1;
    }
    std::vector<Sample> input(32000);
    for (size_t i=0; i<input.size(); ++i) {
        const double envelope = std::min({1.0, i/320.0, (input.size()-1-i)/320.0});
        const s16 value=static_cast<s16>(12000*envelope*std::sin(6.283185307179586*440*i/32000.0));
        input[i]={value,value};
    }
    void *storage=malloc_64(sizeof(CAudioBuffer));
    auto *cached=new(storage) CAudioBuffer(65536);
    dcache_wbinv_range_unaligned(cached, cached + 1);
    auto *buffer=make_uncached_ptr(cached);
    buffer->AddSamples(input.data(), input.size(), 32000, 44100);
    const u32 count=buffer->GetNumBufferedSamples();
    if (count != 44099) {
        std::printf("FROGN64_AUDIO_TESTS_FAIL tone_count=%lu\n", static_cast<unsigned long>(count));
        sceKernelExitGame();
        return 1;
    }
    int channel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, 512, PSP_AUDIO_FORMAT_STEREO);
    // The first blocking output returns while its buffer is still playing.
    // Alternate buffers so that RAM is not rewritten under the active descriptor.
    alignas(64) Sample output[2][512];
    unsigned current=0;
    if (channel<0) return 1;
    while (buffer->GetNumBufferedSamples()) {
        buffer->Drain(output[current], 512);
        if (sceAudioOutputBlocking(channel, PSP_AUDIO_VOLUME_MAX, output[current])<0) return 1;
        current ^= 1;
    }
    sceKernelDelayThread(50000);
    sceAudioChRelease(channel);
    cached->~CAudioBuffer();
    std::free(storage);
    std::printf("FROGN64_AUDIO_TESTS_PASS samples=%lu\n", static_cast<unsigned long>(count));
    sceKernelExitGame();
    return 0;
}
