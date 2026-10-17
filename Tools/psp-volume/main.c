// SPDX-License-Identifier: GPL-2.0-or-later
// One-shot PSPLink helper: adjusts firmware volume without game-memory pokes.
#include <pspkernel.h>
#include <pspimpose_driver.h>
#include <pspiofilemgr_kernel.h>
#include <stdio.h>
#include <string.h>

PSP_MODULE_INFO("FrogN64Volume", PSP_MODULE_KERNEL, 1, 0);

int module_start(SceSize args, void *argp)
{
    const char *cursor = argp, *end = cursor + args;
    const char *values[3];
    int count = 0;
    while (cursor < end && count < 3) {
        const char *nul = memchr(cursor, 0, end - cursor);
        if (!nul) return -1;
        values[count++] = cursor;
        cursor = nul + 1;
    }
    if (count != 3 || !*values[1]) return -1;
    int level = 0;
    for (const char *s = values[1]; *s; ++s) {
        if (*s < '0' || *s > '9') return -1;
        level = level * 10 + *s - '0';
        if (level > 30) return -1;
    }
    // Open the result first, so a bad output path cannot hide a volume change.
    SceUID fd = sceIoOpen(values[2], PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (fd < 0) return fd;
    int before = sceImposeGetParam(PSP_IMPOSE_MAIN_VOLUME);
    int mute_before = sceImposeGetParam(PSP_IMPOSE_MUTE);
    int result = sceImposeSetParam(PSP_IMPOSE_MAIN_VOLUME, level);
    int unmute = result < 0 ? result : sceImposeSetParam(PSP_IMPOSE_MUTE, 0);
    int after = sceImposeGetParam(PSP_IMPOSE_MAIN_VOLUME);
    int mute_after = sceImposeGetParam(PSP_IMPOSE_MUTE);
    char report[320];
    int len = snprintf(report, sizeof(report),
        "requested=%d\nbefore=%d\nmute_before=%d\nset_result=%d\nunmute_result=%d\nafter=%d\nmute_after=%d\n",
        level, before, mute_before, result, unmute, after, mute_after);
    sceIoWrite(fd, report, len);
    sceIoClose(fd);
    return 0;
}

int module_stop(SceSize args, void *argp) { (void)args; (void)argp; return 0; }
