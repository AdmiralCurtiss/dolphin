// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Config
{
class ConfigLayerLoader;
enum class PerformanceProfileType;
}  // namespace Config

namespace ConfigLoaders
{
std::vector<std::string>
GetGameIniFilenames(const std::string& id, std::optional<u16> revision,
                    std::optional<Config::PerformanceProfileType> profile = std::nullopt);

std::unique_ptr<Config::ConfigLayerLoader> GenerateGlobalGameConfigLoader(const std::string& id,
                                                                          u16 revision);
std::unique_ptr<Config::ConfigLayerLoader>
GenerateProfileGameConfigLoader(const std::string& id, u16 revision,
                                Config::PerformanceProfileType profile);
std::unique_ptr<Config::ConfigLayerLoader> GenerateLocalGameConfigLoader(const std::string& id,
                                                                         u16 revision);
}  // namespace ConfigLoaders
