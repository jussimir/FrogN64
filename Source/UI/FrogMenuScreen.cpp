/*
 * FrogN64 PSP frontend, Copyright (C) 2026 FrogN64 contributors.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Layout, navigation and palette adapted from FrogGBA's gui.c, by
 * tzubertowski (Prosty) and contributors. FrogGBA is based on gpSP/TempGBA:
 * Copyright (C) 2006 Exophase, (C) 2007 takka.
 * See licenses/FrogGBA.txt and licenses/FrogN64-UI.txt in the distribution.
 * N64 emulation and settings are provided by DaedalusX64.
 */
#include "Base/Types.h"
#include "FrogMenuScreen.h"
#include "FrogTheme.h"
#include "MainMenuScreen.h"
#include "PauseScreen.h"
#include "UIScreen.h"
#include "UIContext.h"
#include "UIComponent.h"
#include "GlobalSettingsComponent.h"
#include "SavestateSelectorComponent.h"
#include "RomPreferencesScreen.h"
#include "AdvancedOptionsScreen.h"
#include "CheatOptionsScreen.h"
#include "Core/CPU.h"
#include "Core/ROM.h"
#include "RomFile/RomSettings.h"
#include "Interface/SaveState.h"
#include "Interface/Preferences.h"
#include "System/SystemInit.h"
#include "Graphics/GraphicsContext.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>

extern bool gTakeScreenshot;
extern bool gTakeScreenshotSS;

namespace
{
namespace fs = std::filesystem;
constexpr int VisibleRows = 16;
constexpr int MaxRecent = 10;
const char *DirectoryFile = "frogn64-directory.txt";
const char *RecentFile = "frogn64-recent.txt";

enum class Page { Home, Browser, Recent, Settings, States, About, Message };

std::string Lower(std::string text)
{
    for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool IsRom(const fs::path &path)
{
    const auto ext = Lower(path.extension().string());
    return ext == ".z64" || ext == ".n64" || ext == ".v64" || ext == ".rom" ||
           ext == ".bin" || ext == ".usa" || ext == ".jap" || ext == ".pal"
#ifdef DAEDALUS_COMPRESSED_ROM_SUPPORT
           || ext == ".zip"
#endif
           ;
}

// Replace only after a complete write; a failed write leaves the previous list intact.
void WriteLines(const char *name, const std::vector<std::string> &lines)
{
    const std::string temp = std::string(name) + ".tmp";
    std::ofstream file(temp, std::ios::trunc);
    for (const auto &line : lines) file << line << '\n';
    file.close();
    std::error_code ec;
    if (file) fs::rename(temp, name, ec);
    fs::remove(temp, ec);
}

std::vector<std::string> ReadLines(const char *name)
{
    std::vector<std::string> lines;
    std::ifstream file(name);
    std::string line;
    while (lines.size() < MaxRecent && std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

class FrogScreen : public CUIScreen
{
public:
    FrogScreen(CUIContext *context, bool paused) : CUIScreen(context), mPaused(paused)
    {
        mRecent = ReadLines(RecentFile);
        std::error_code ec;
        mAppDirectory = fs::current_path(ec);
        mDirectory = mAppDirectory / "Roms";
        const auto saved = ReadLines(DirectoryFile);
        if (!saved.empty() && fs::is_directory(saved.front(), ec)) mDirectory = saved.front();
        RefreshClock();
    }

    void RunMenu()
    {
        CUIScreen::Run();
        CGraphicsContext::Get()->SwitchToChosenDisplay();
        CGraphicsContext::Get()->ClearAllSurfaces();
    }
    bool IsFinished() const override { return mFinished; }

    void Update(float elapsed, const glm::vec2 &stick, u32 old, u32 buttons) override
    {
        mClockTimer += elapsed;
        if (mClockTimer >= 1.0f) { RefreshClock(); mClockTimer = 0; }
        const u32 pressed = buttons & ~old;
        if (mPage == Page::Settings || mPage == Page::States)
        {
            if (pressed & PSP_CTRL_CIRCLE) { Home(); return; }
            mComponent->Update(elapsed, stick, old, buttons);
            // The component callback only queues a path: never destroy it while in Update.
            if (!mPendingState.empty()) ApplyState();
            return;
        }
        if (pressed & PSP_CTRL_CIRCLE)
        {
            if (mPage == Page::Home) { if (mPaused) mFinished = true; }
            else if (mPage == Page::Browser) ParentDirectory();
            else Home();
            return;
        }
        if (mPage == Page::Message || mPage == Page::About)
        {
            if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) Home();
            return;
        }
        if (mPage == Page::Home && (pressed & PSP_CTRL_LTRIGGER) && !mPaused)
        { OpenBrowser(); return; }
        if (mPage == Page::Home && (pressed & PSP_CTRL_RTRIGGER) && mPaused)
        { OpenStates(false); return; }

        int direction = buttons & PSP_CTRL_UP ? -1 : buttons & PSP_CTRL_DOWN ? 1 :
                        stick.y < -0.55f ? -1 : stick.y > 0.55f ? 1 : 0;
        if (direction)
        {
            mRepeat -= elapsed;
            if (direction != mDirection || mRepeat <= 0)
            {
                Move(direction);
                mRepeat = direction != mDirection ? 0.32f : 0.085f;
            }
        }
        else mRepeat = 0;
        mDirection = direction;

        if (mPage == Page::Browser)
        {
            if (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))
            { mDirectoryColumn = !mDirectoryColumn; }
            if (pressed & PSP_CTRL_LTRIGGER) Move(-VisibleRows);
            if (pressed & PSP_CTRL_RTRIGGER) Move(VisibleRows);
            if (pressed & PSP_CTRL_SQUARE) { mDirectory = "ms0:/"; ReadDirectory(); }
            if (pressed & PSP_CTRL_SELECT) { mDirectory = mAppDirectory / "Roms"; ReadDirectory(); }
            if (pressed & PSP_CTRL_TRIANGLE) EditSelectedRom();
        }
        else if (mPage == Page::Recent && (pressed & PSP_CTRL_TRIANGLE)) EditSelectedRom();
        if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) Activate();
    }

    void Render() override
    {
        mpContext->SetFontStyle(CUIContext::FS_REGULAR);
        mpContext->ClearBackground(FrogTheme::Background);
        const char *title = mPage == Page::Home ? (mPaused ? "Game paused" : "Main menu") :
            mPage == Page::Browser ? "Load game" : mPage == Page::Recent ? "Recent games" :
            mPage == Page::Settings ? "Emulator settings" : mPage == Page::States ?
            (mSaving ? "Save state" : "Load state") : mPage == Page::About ? "About FrogN64" : "FrogN64";
        Text(6, 13, title, FrogTheme::Info, 248);
        Text(284, 13, mClock, FrogTheme::Info, 190);
        if (mPage == Page::Home) DrawHome();
        else if (mPage == Page::Browser) DrawBrowser();
        else if (mPage == Page::Recent) DrawRecent();
        else if (mPage == Page::Settings || mPage == Page::States) mComponent->Render();
        else if (mPage == Page::About) DrawAbout();
        else
        {
            Text(18, 62, "Unable to open", FrogTheme::Selected, 440);
            Text(18, 84, mMessage, FrogTheme::Info, 440);
            Text(18, 112, "Check the file and try again.", FrogTheme::Help, 440);
        }
        // Keep the FrogGBA help strip legible, including over legacy settings screens.
        mpContext->DrawRect(0, 250, 480, 22, FrogTheme::Background);
        const char *help = mPage == Page::Browser ? "X Open  O Up  [] Memory Stick" :
            mPage == Page::Recent ? "X Load  O Back  Triangle Settings" :
            mPage == Page::Settings ? "Left/Right Change  O Back" :
            mPage == Page::Home ? (mPaused ? "X Select  O Resume  R States" : "X Select  L Load game") :
            mPage == Page::States ? "X Select  O Back" : "X / O Back";
        Text(12, 264, help, FrogTheme::Help, 320);
        Text(366, 264, "FrogN64  0.1", FrogTheme::Help, 108);
    }

private:
    void Text(int x, int y, std::string text, c32 colour, int width)
    {
        constexpr float scale = 0.85f;
        if (mpContext->GetTextWidth(text.c_str()) * scale > width)
        {
            do
            {
                if (text.empty()) break;
                size_t last = text.size() - 1;
                while (last && (static_cast<unsigned char>(text[last]) & 0xc0) == 0x80) --last;
                text.erase(last);
            } while (mpContext->GetTextWidth((text + "...").c_str()) * scale > width);
            text += "...";
        }
        mpContext->DrawTextScale(x, y, scale, text, colour, c32(0, 0, 0, 0));
    }

    void RefreshClock()
    {
        ScePspDateTime now{};
        sceRtcGetCurrentClockLocalTime(&now);
        char clock[64];
        int battery = scePowerGetBatteryLifePercent();
        if (battery >= 0)
            snprintf(clock, sizeof(clock), "%02d/%02d %02d:%02d   %d%%", now.day, now.month, now.hour, now.minute, battery);
        else
            snprintf(clock, sizeof(clock), "%02d/%02d %02d:%02d   AC", now.day, now.month, now.hour, now.minute);
        mClock = clock;
    }

    void Home()
    {
        if (mPage == Page::Settings) CPreferences::Get()->Commit();
        mComponent.reset();
        mPage = Page::Home;
        mSelection = 0;
        mDirection = 0;
    }

    std::vector<const char *> HomeItems() const
    {
        if (mPaused) return {"Return to game", "Save state", "Load state", "Game settings",
            "Advanced settings", "Cheats", "Emulator settings", "Take screenshot", "Return to main menu", "About FrogN64"};
        return {"Load game", "Recent games", "Load state", "Emulator settings", "About FrogN64", "Exit to XMB"};
    }

    void DrawHome()
    {
        const auto items = HomeItems();
        for (size_t i = 0; i < items.size(); ++i)
            Text(18, 48 + i * 18, items[i], i == mSelection ? FrogTheme::Selected : FrogTheme::Text, 208);
        // Original vector mark; FrogGBA's screenshot region becomes a FrogN64 identity panel.
        mpContext->DrawRect(240, 44, 222, 154, FrogTheme::Scroll);
        const auto frog = FrogTheme::Info;
        mpContext->DrawRect(302, 82, 26, 28, frog);
        mpContext->DrawRect(374, 82, 26, 28, frog);
        mpContext->DrawRect(302, 102, 98, 48, frog);
        mpContext->DrawRect(290, 118, 122, 24, frog);
        mpContext->DrawRect(310, 90, 10, 12, FrogTheme::Background);
        mpContext->DrawRect(382, 90, 10, 12, FrogTheme::Background);
        mpContext->DrawRect(326, 132, 50, 4, FrogTheme::Background);
        Text(307, 176, "FrogN64", FrogTheme::Selected, 132);
        Text(240, 222, mPaused ? g_ROM.settings.GameName : "Nintendo 64 on PSP", FrogTheme::Info, 222);
        Text(240, 239, mPaused ? "SELECT opens this menu" : "Powered by DaedalusX64", FrogTheme::Help, 222);
    }

    void DrawScroll(size_t selected, size_t count, int x)
    {
        if (count <= VisibleRows) return;
        mpContext->DrawRect(x, 52, 3, 188, FrogTheme::Scroll);
        const int height = std::max(5, 188 * VisibleRows / static_cast<int>(count));
        const int top = 52 + (188 - height) * selected / (count - 1);
        mpContext->DrawRect(x, top, 3, height, FrogTheme::Info);
    }

    size_t FirstRow(size_t selected) const
    { return selected < VisibleRows ? 0 : selected - VisibleRows + 1; }

    void DrawBrowser()
    {
        Text(18, 33, mDirectory.string(), FrogTheme::Help, 444);
        mpContext->DrawLine(340, 40, 340, 242, FrogTheme::Scroll);
        for (size_t i = FirstRow(mFileSelection), row = 0; i < mFiles.size() && row < VisibleRows; ++i, ++row)
            Text(18, 58 + row * 12, mFiles[i].filename().string(),
                !mDirectoryColumn && i == mFileSelection ? FrogTheme::Selected : FrogTheme::Text, 309);
        for (size_t i = FirstRow(mDirSelection), row = 0; i < mDirs.size() && row < VisibleRows; ++i, ++row)
            Text(352, 58 + row * 12, i == 0 ? ".." : mDirs[i].filename().string(),
                mDirectoryColumn && i == mDirSelection ? FrogTheme::Selected : FrogTheme::Directory, 116);
        DrawScroll(mFileSelection, mFiles.size(), 6);
        DrawScroll(mDirSelection, mDirs.size(), 475);
        if (mFiles.empty())
        {
            Text(18, 83, "No N64 ROMs in this folder.", FrogTheme::Info, 310);
            Text(18, 107, "[] Memory Stick / SELECT Roms", FrogTheme::Help, 310);
        }
        if (!mDirectoryError.empty()) Text(18, 233, mDirectoryError, FrogTheme::Selected, 309);
    }

    void DrawRecent()
    {
        if (mRecent.empty())
        {
            Text(18, 61, "No recent games yet.", FrogTheme::Info, 440);
            Text(18, 85, "Load a game to add it to this list.", FrogTheme::Help, 440);
        }
        for (size_t i = 0; i < mRecent.size(); ++i)
            Text(18, 48 + i * 18, fs::path(mRecent[i]).filename().string(),
                 i == mSelection ? FrogTheme::Selected : FrogTheme::Text, 440);
    }

    void DrawAbout()
    {
        const char *lines[] = {"FrogN64 0.1 - PSP", "N64 emulation: DaedalusX64 team / StrmnNrmn",
            "UI based on FrogGBA by Prosty (tzubertowski)", "FrogGBA: gpSP / TempGBA and contributors", "",
            "github.com/jussimir/FrogN64", "github.com/tzubertowski/FrogGBA",
            "github.com/DaedalusX64/daedalus", "", "Free software: GNU GPL v2 or later.",
            "Licenses and credits: licenses/ and COPYING.txt"};
        for (size_t i = 0; i < sizeof(lines) / sizeof(*lines); ++i)
            Text(18, 46 + i * 17, lines[i], i == 0 ? FrogTheme::Selected : FrogTheme::Info, 444);
    }

    void Move(int amount)
    {
        size_t *selected = &mSelection;
        size_t count = mPage == Page::Home ? HomeItems().size() : mRecent.size();
        if (mPage == Page::Browser)
        {
            selected = mDirectoryColumn ? &mDirSelection : &mFileSelection;
            count = mDirectoryColumn ? mDirs.size() : mFiles.size();
        }
        if (count) *selected = static_cast<size_t>(std::clamp(static_cast<int>(*selected) + amount, 0, static_cast<int>(count) - 1));
    }

    void OpenBrowser() { mPage = Page::Browser; ReadDirectory(); }

    void ReadDirectory()
    {
        mFiles.clear(); mDirs.clear();
        mFileSelection = mDirSelection = 0;
        mDirectoryError.clear();
        auto parent = mDirectory.parent_path();
        mDirs.push_back(parent.empty() ? mDirectory : parent);
        std::error_code ec;
        fs::directory_iterator it(mDirectory, ec), end;
        while (!ec && it != end)
        {
            const auto &entry = *it;
            if (entry.is_directory(ec)) mDirs.push_back(entry.path());
            else if (!ec && entry.is_regular_file(ec) && IsRom(entry.path())) mFiles.push_back(entry.path());
            if (!ec) it.increment(ec);
        }
        if (ec) mDirectoryError = "Cannot read this folder. [] opens ms0:/";
        auto less = [](const fs::path &a, const fs::path &b) { return Lower(a.filename().string()) < Lower(b.filename().string()); };
        std::sort(mFiles.begin(), mFiles.end(), less);
        std::sort(mDirs.begin() + 1, mDirs.end(), less);
        mDirectoryColumn = mFiles.empty();
        if (!ec) WriteLines(DirectoryFile, {mDirectory.string()});
    }

    void ParentDirectory()
    {
        const auto parent = mDirectory.parent_path();
        if (parent.empty() || parent == mDirectory) Home();
        else { mDirectory = parent; ReadDirectory(); }
    }

    fs::path SelectedRom() const
    {
        if (mPage == Page::Recent && mSelection < mRecent.size()) return mRecent[mSelection];
        if (mPage == Page::Browser && !mDirectoryColumn && mFileSelection < mFiles.size()) return mFiles[mFileSelection];
        return {};
    }

    void EditSelectedRom()
    {
        const auto path = SelectedRom();
        RomID id; u32 size; ECicType cic;
        if (!path.empty() && ROM_GetRomDetailsByFilename(path, &id, &size, &cic))
        { CRomPreferencesScreen::Create(mpContext, id)->Run(); CPreferences::Get()->Commit(); }
    }

    bool LoadRom(const fs::path &path)
    {
        RomID id; u32 size; ECicType cic;
        if (!ROM_GetRomDetailsByFilename(path, &id, &size, &cic) || !System_Open(path))
        { mMessage = path.filename().string(); mPage = Page::Message; return false; }
        const auto text = path.string();
        mRecent.erase(std::remove(mRecent.begin(), mRecent.end(), text), mRecent.end());
        mRecent.insert(mRecent.begin(), text);
        if (mRecent.size() > MaxRecent) mRecent.resize(MaxRecent);
        WriteLines(RecentFile, mRecent);
        mFinished = true;
        return true;
    }

    void OpenStates(bool saving)
    {
        mPage = Page::States; mSaving = saving; mPendingState.clear();
        mComponent.reset(CSavestateSelectorComponent::Create(mpContext,
            saving ? CSavestateSelectorComponent::AT_SAVING : CSavestateSelectorComponent::AT_LOADING,
            [this](const char *path) { if (path) mPendingState = path; },
            mPaused ? fs::path(g_ROM.settings.GameName) : fs::path()));
    }

    void ApplyState()
    {
        const auto state = mPendingState;
        mPendingState.clear();
        if (mPaused)
        {
            const bool requested = mSaving ? CPU_RequestSaveState(state) : CPU_RequestLoadState(state);
            if (!requested) { mMessage = "Could not queue the state request."; mPage = Page::Message; return; }
            if (mSaving) { gTakeScreenshot = true; gTakeScreenshotSS = true; }
            mFinished = true;
        }
        else
        {
            const auto rom = SaveState_GetRom(state);
            if (!rom.empty() && LoadRom(rom))
            {
                if (!SaveState_LoadFromFile(state))
                { System_Close(); mFinished = false; mMessage = "Could not restore this state."; mPage = Page::Message; }
            }
            else { mMessage = "The ROM for this state is unavailable."; mPage = Page::Message; }
        }
    }

    void Activate()
    {
        if (mPage == Page::Browser)
        {
            if (mDirectoryColumn && mDirSelection < mDirs.size())
            { mDirectory = mDirs[mDirSelection]; ReadDirectory(); }
            else { const auto path = SelectedRom(); if (!path.empty()) LoadRom(path); }
            return;
        }
        if (mPage == Page::Recent)
        { const auto path = SelectedRom(); if (!path.empty()) LoadRom(path); return; }
        if (mPage != Page::Home) return;
        if (!mPaused)
        {
            switch (mSelection)
            {
            case 0: OpenBrowser(); break;
            case 1: mPage = Page::Recent; mSelection = 0; break;
            case 2: OpenStates(false); break;
            case 3: mPage = Page::Settings; mComponent.reset(CGlobalSettingsComponent::Create(mpContext)); break;
            case 4: mPage = Page::About; break;
            case 5: sceKernelExitGame(); break;
            }
        }
        else
        {
            switch (mSelection)
            {
            case 0: mFinished = true; break;
            case 1: OpenStates(true); break;
            case 2: OpenStates(false); break;
            case 3: CRomPreferencesScreen::Create(mpContext, g_ROM.mRomID)->Run(); break;
            case 4: CAdvancedOptionsScreen::Create(mpContext, g_ROM.mRomID)->Run(); break;
            case 5: CCheatOptionsScreen::Create(mpContext, g_ROM.mRomID)->Run(); break;
            case 6: mPage = Page::Settings; mComponent.reset(CGlobalSettingsComponent::Create(mpContext)); break;
            case 7: gTakeScreenshot = true; mFinished = true; break;
            case 8: CPU_Halt("Returning to FrogN64 menu"); mFinished = true; break;
            case 9: mPage = Page::About; break;
            }
        }
    }

    bool mPaused = false, mFinished = false, mDirectoryColumn = false, mSaving = false;
    Page mPage = Page::Home;
    size_t mSelection = 0, mFileSelection = 0, mDirSelection = 0;
    float mRepeat = 0, mClockTimer = 0;
    int mDirection = 0;
    fs::path mAppDirectory, mDirectory;
    std::vector<fs::path> mFiles, mDirs;
    std::vector<std::string> mRecent;
    std::unique_ptr<CUIComponent> mComponent;
    std::string mClock, mMessage, mDirectoryError, mPendingState;
};

class FrogMainMenu final : public CMainMenuScreen
{
public:
    explicit FrogMainMenu(CUIContext *context) : mScreen(context, false) {}
    void Run() override { mScreen.RunMenu(); }
private:
    FrogScreen mScreen;
};
class FrogPauseMenu final : public CPauseScreen
{
public:
    explicit FrogPauseMenu(CUIContext *context) : mScreen(context, true) {}
    void Run() override { mScreen.RunMenu(); }
private:
    FrogScreen mScreen;
};
}

std::unique_ptr<CMainMenuScreen> CreateFrogMainMenu(CUIContext *context)
{ return std::make_unique<FrogMainMenu>(context); }
std::unique_ptr<CPauseScreen> CreateFrogPauseMenu(CUIContext *context)
{ return std::make_unique<FrogPauseMenu>(context); }
