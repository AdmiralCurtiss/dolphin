// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylanders/SkylanderCrypto.h"

#include <string>

#include <Common/BitUtils.h>
#include <Common/Swap.h>

namespace IOS::HLE::USB::SkylanderCrypto
{
u16 ComputeCRC16(u16 init_value, std::span<const u8> data)
{
  static constexpr std::array<u16, 256> CRC_CCITT_TABLE{
      0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A,
      0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294,
      0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462,
      0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509,
      0xE5EE, 0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695,
      0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5,
      0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948,
      0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
      0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
      0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B,
      0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F,
      0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB,
      0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046,
      0x6067, 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290,
      0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E,
      0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
      0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691,
      0x16B0, 0x6657, 0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
      0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D,
      0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16,
      0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8,
      0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E,
      0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93,
      0x3EB2, 0x0ED1, 0x1EF0};

  u16 crc = init_value;

  for (size_t i = 0; i < data.size(); ++i)
  {
    const u16 tmp = (crc >> 8) ^ data[i];
    crc = (crc << 8) ^ CRC_CCITT_TABLE[tmp];
  }

  return crc;
}
// CRC-64 algorithm that is limited to 48 bits every iteration
u64 ComputeCRC48(std::span<const u8> data)
{
  const u64 polynomial = 0x42f0e1eba9ea3693;
  const u64 initial_register_value = 2ULL * 2ULL * 3ULL * 1103ULL * 12868356821ULL;

  u64 crc = initial_register_value;
  for (size_t i = 0; i < data.size(); ++i)
  {
    crc ^= (static_cast<u64>(data[i]) << 40);
    for (size_t j = 0; j < 8; ++j)
    {
      if (Common::ExtractBit(crc, 47))
      {
        crc = (crc << 1) ^ polynomial;
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc & 0x0000FFFFFFFFFFFF;
}
u64 CalculateKeyA(u8 sector, std::span<const u8, 0x4> nuid)
{
  if (sector == 0)
  {
    return 73ULL * 2017ULL * 560381651ULL;
  }

  std::array<u8, 5> data = {nuid[0], nuid[1], nuid[2], nuid[3], sector};

  u64 big_endian_crc = ComputeCRC48(data);
  u64 little_endian_crc = Common::swap64(big_endian_crc) >> 16;

  return little_endian_crc;
}
void ComputeChecksum(ChecksumType type, const u8* data_start, u8* output)
{
  u16 crc = 0xFFFF;

  switch (type)
  {
  case ChecksumType::Type0:
  {
    std::array<u8, 0x1E> input = {};
    memcpy(input.data(), data_start, 0x1E);

    crc = ComputeCRC16(crc, input);
    break;
  }
  case ChecksumType::Type3:
  {
    std::array<u8, 0x110> input = {};
    memcpy(input.data(), data_start, 0x20);
    memcpy(input.data() + 0x20, data_start + 0x30, 0x10);

    crc = ComputeCRC16(crc, input);
    break;
  }
  case ChecksumType::Type2:
  {
    std::array<u8, 0x30> input = {};
    memcpy(input.data(), data_start, 0x20);
    memcpy(input.data() + 0x20, data_start + 0x30, 0x10);

    crc = ComputeCRC16(crc, input);
    break;
  }
  case ChecksumType::Type1:
  {
    std::array<u8, 0x10> input = {};
    memcpy(input.data(), data_start, 0x10);
    input[0xE] = 0x05;
    input[0xF] = 0x00;

    crc = ComputeCRC16(crc, input);
    break;
  }
  case ChecksumType::Type6:
  {
    std::array<u8, 0x40> input = {};
    memcpy(input.data(), data_start, 0x20);

    memcpy(input.data() + 0x20, data_start + 0x30, 0x20);

    input[0x0] = 0x06;
    input[0x1] = 0x01;

    crc = ComputeCRC16(crc, input);
    break;
  }
  default:
    break;
  }
  memcpy(output, &crc, 2);
}
std::array<u8, 11> ComputeToyCode(u64 code)
{
  if (code == 0)
  {
    static constexpr std::array<u8, 11> invalid_code_result{
        static_cast<u8>('N'), static_cast<u8>('/'), static_cast<u8>('A')};
    return invalid_code_result;
  }

  std::array<u8, 10> code_bytes;
  for (size_t i = 0; i < code_bytes.size(); ++i)
  {
    code_bytes[i] = static_cast<u8>(code % 29);
    code /= 29;
  }

  static constexpr char lookup_table[] = "23456789BCDFGHJKLMNPQRSTVWXYZ";
  std::array<u8, 10> code_chars;
  for (size_t i = 0; i < code_bytes.size(); ++i)
  {
    code_chars[i] = static_cast<u8>(lookup_table[code_bytes[9 - i]]);
  }

  std::array<u8, 11> result;
  std::memcpy(&result[0], &code_chars[0], 5);
  result[5] = static_cast<u8>('-');
  std::memcpy(&result[6], &code_chars[5], 5);

  return result;
}
}  // namespace IOS::HLE::USB::SkylanderCrypto
