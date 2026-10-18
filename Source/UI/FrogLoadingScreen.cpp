// SPDX-License-Identifier: GPL-2.0-or-later
#include "Base/Types.h"
#include "FrogLoadingScreen.h"
#include "FrogTheme.h"
#include "UIContext.h"
#include <pspkernel.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
std::unique_ptr<CUIContext> context;
std::string filename;
bool active = false, reading = false, starting = false;
u64 lastDraw = 0;
unsigned lastPercent = ~0u;

void Text(int x, int y, std::string text, c32 colour, int width = 444)
{
    constexpr float scale = .85f;
    if (context->GetTextWidth(text.c_str()) * scale > width) {
        do {
            if (text.empty()) break;
            auto pos = text.size() - 1;
            while (pos && (static_cast<unsigned char>(text[pos]) & 0xc0) == 0x80) --pos;
            text.erase(pos);
        } while (context->GetTextWidth((text + "...").c_str()) * scale > width);
        text += "...";
    }
    context->DrawTextScale(x, y, scale, text, colour, c32(0, 0, 0, 0));
}

void Draw(unsigned percent, const char *status, const std::string &detail, bool force)
{
    if (!active) return;
    const u64 now = sceKernelGetSystemTimeWide();
    if (!force && (percent == lastPercent || now - lastDraw < 50000)) return;
    lastDraw = now; lastPercent = percent;
    context->BeginRender();
    context->SetFontStyle(CUIContext::FS_REGULAR);
    context->ClearBackground(FrogTheme::Background);
    Text(6, 13, "Loading game", FrogTheme::Info);
    // Same pixel mark and palette as the FrogN64 home screen.
    context->DrawRect(202, 49, 18, 20, FrogTheme::Info);
    context->DrawRect(260, 49, 18, 20, FrogTheme::Info);
    context->DrawRect(202, 65, 76, 36, FrogTheme::Info);
    context->DrawRect(194, 77, 92, 18, FrogTheme::Info);
    context->DrawRect(208, 55, 7, 9, FrogTheme::Background);
    context->DrawRect(266, 55, 7, 9, FrogTheme::Background);
    context->DrawRect(220, 86, 40, 3, FrogTheme::Background);
    Text(207, 121, "FrogN64", FrogTheme::Selected, 100);
    Text(18, 148, filename, FrogTheme::Selected);
    Text(18, 169, status, FrogTheme::Info, 365);
    char label[16]; std::snprintf(label, sizeof(label), "%u%%", percent);
    Text(421, 169, label, FrogTheme::Info, 53);
    context->DrawRect(18, 181, 444, 12, FrogTheme::Info);
    context->DrawRect(20, 183, 440, 8, FrogTheme::Scroll);
    const unsigned fill = 440 * percent / 100;
    if (fill) context->DrawRect(20, 183, fill, 8, FrogTheme::Selected);
    Text(18, 214, detail, FrogTheme::Help);
    Text(12, 264, "Please wait", FrogTheme::Help);
    Text(366, 264, "FrogN64  0.1", FrogTheme::Help, 108);
    context->EndRender();
}
}

void FrogLoadingScreen::Begin(const std::filesystem::path &rom)
{
    if (!context) context.reset(CUIContext::Create());
    active = context != nullptr;
    reading = starting = false; filename = rom.filename().string();
    Draw(0, "Preparing game", "", true);
}

void FrogLoadingScreen::Reading(std::uint32_t bytes, std::uint32_t total)
{
    reading = true;
    const unsigned percent = total ? std::min<std::uint64_t>(100, std::uint64_t(bytes) * 100 / total) : 0;
    char detail[64];
    std::snprintf(detail, sizeof(detail), "%.1f / %.1f MiB", bytes / 1048576.f, total / 1048576.f);
    Draw(percent, "Reading ROM", detail, bytes == 0 || bytes == total);
}

void FrogLoadingScreen::Preparing(unsigned completed, unsigned total)
{
    // A full buffer already reports byte progress. Keep it full during setup;
    // cached ROMs instead report completed initialization steps.
    if (reading && starting) return;
    const unsigned percent = reading ? 100 : (total ? std::min(100u, completed * 100 / total) : 0);
    Draw(percent, "Starting game", "", !starting || completed == total);
    starting = true;
}

void FrogLoadingScreen::End() { active = false; }
void FrogLoadingScreen::Shutdown() { End(); context.reset(); }
