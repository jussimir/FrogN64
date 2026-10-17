// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
class CUIContext;
class CMainMenuScreen;
class CPauseScreen;

std::unique_ptr<CMainMenuScreen> CreateFrogMainMenu(CUIContext *context);
std::unique_ptr<CPauseScreen> CreateFrogPauseMenu(CUIContext *context);
