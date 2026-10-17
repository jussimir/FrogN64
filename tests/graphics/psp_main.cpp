// SPDX-License-Identifier: GPL-2.0-or-later
#include <pspkernel.h>
#include "projection_cases.h"
PSP_MODULE_INFO("FrogN64ProjectionTests", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-256);
int main()
{
    const int failures=projection_cases();
    std::printf("FROGN64_PROJECTION_TESTS_%s\n", failures ? "FAIL" : "PASS");
    std::fflush(stdout);
    // Headless inspects the marker before its emulated deadline closes the run.
    sceKernelDelayThread(30000000);
    sceKernelExitGame();
    return failures ? 1 : 0;
}
