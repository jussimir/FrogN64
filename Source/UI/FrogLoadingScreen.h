// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <cstdint>

namespace FrogLoadingScreen
{
void Begin(const std::filesystem::path &rom);
void Reading(std::uint32_t bytes, std::uint32_t total);
void Preparing(unsigned completed, unsigned total);
void End();
void Shutdown();
}
