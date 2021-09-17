// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/RiivolutionPatcher.h"

#include <pugixml.hpp>

#include "Common/IOFile.h"

namespace DiscIO::Riivolution
{
std::optional<Disc> ParseFile(const std::string& filename, const std::string& game_id, u16 revision,
                              u8 disc_number)
{
  ::File::IOFile f(filename, "r");
  if (!f)
    return std::nullopt;

  std::vector<char> data;
  data.resize(f.GetSize());
  if (!f.ReadBytes(data.data(), data.size()))
    return std::nullopt;

  return ParseString(std::string_view(data.data(), data.data() + data.size()), game_id, revision,
                     disc_number);
}

std::optional<Disc> ParseString(std::string_view xml, const std::string& game_id, u16 revision,
                                u8 disc_number)
{
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size());

  return std::nullopt;
}
}  // namespace DiscIO::Riivolution
