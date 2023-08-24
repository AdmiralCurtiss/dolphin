// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>

#include <Common/CommonTypes.h>

namespace IOS::HLE::USB::SkylanderCrypto
{
enum class ChecksumType
{
  Type0,
  Type1,
  Type2,
  Type3,
  Type6
};
u16 ComputeCRC16(u16 init_value, std::span<const u8> data);
u64 ComputeCRC48(std::span<const u8> data);
u64 CalculateKeyA(u8 sector, std::span<const u8, 0x4> nuid);
void ComputeChecksum(ChecksumType type, const u8* data_start, u8* output);
void ComputeToyCode(u64 code, std::array<u8, 11>* output);
}  // namespace IOS::HLE::USB::SkylanderCrypto
