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

static bool CheckRegion(const pugi::xml_object_range<pugi::xml_named_node_iterator>& xml_regions,
                        std::string_view game_region)
{
  if (xml_regions.begin() == xml_regions.end())
    return true;

  for (const auto& region : xml_regions)
  {
    if (region.attribute("type").as_string() == game_region)
      return true;
  }
  return false;
}

std::optional<Disc> ParseString(std::string_view xml, const std::string& game_id, u16 revision,
                                u8 disc_number)
{
  if (game_id.size() != 6)
    return std::nullopt;

  const std::string_view game_id_full = std::string_view(game_id);
  const std::string_view game_id_no_region = game_id_full.substr(0, 3);
  const std::string_view game_region = game_id_full.substr(3, 1);
  const std::string_view game_developer = game_id_full.substr(4, 2);

  const auto replace_variables = [&](std::string_view sv) -> std::string {
    return std::string(sv);
  };

  const auto read_hex_string = [](std::string_view sv) -> std::vector<u8> {
    return std::vector<u8>();
  };

  pugi::xml_document doc;
  const auto parse_result = doc.load_buffer(xml.data(), xml.size());
  if (!parse_result)
    return std::nullopt;

  const auto wiidisc = doc.child("wiidisc");
  if (!wiidisc)
    return std::nullopt;

  Disc disc;
  disc.m_version = wiidisc.attribute("version").as_int(-1);
  if (disc.m_version != 1)
    return std::nullopt;
  disc.m_root = replace_variables(wiidisc.attribute("root").as_string());

  const auto id = wiidisc.child("id");
  if (id)
  {
    // filter against given game data and cancel if anything mismatches
    for (const auto& attribute : id.attributes())
    {
      const std::string_view attribute_name(attribute.name());
      if (attribute_name == "game")
      {
        if (!game_id_full.starts_with(attribute.as_string()))
          return std::nullopt;
      }
      else if (attribute_name == "developer")
      {
        if (game_developer != attribute.as_string())
          return std::nullopt;
      }
      else if (attribute_name == "disc")
      {
        if (disc_number != attribute.as_int(-1))
          return std::nullopt;
      }
      else if (attribute_name == "version")
      {
        if (revision != attribute.as_int(-1))
          return std::nullopt;
      }
    }

    if (!CheckRegion(id.children("region"), game_region))
      return std::nullopt;
  }

  const auto patches = doc.children("patch");
  for (const auto& patch_node : patches)
  {
    Patch patch;
    patch.m_id = patch_node.attribute("id").as_string();
    patch.m_root = replace_variables(patch_node.attribute("root").as_string());

    for (const auto& patch_subnode : patch_node.children())
    {
      const std::string_view patch_name(patch_subnode.name());
      if (patch_name == "file")
      {
        auto& file = patch.m_file_patches.emplace_back();
        file.m_disc = replace_variables(patch_subnode.attribute("disc").as_string());
        file.m_external = replace_variables(patch_subnode.attribute("external").as_string());
        file.m_resize = patch_subnode.attribute("resize").as_bool(true);
        file.m_create = patch_subnode.attribute("create").as_bool(false);
        file.m_offset = patch_subnode.attribute("offset").as_uint(0);
        file.m_fileoffset = patch_subnode.attribute("fileoffset").as_uint(0);
        file.m_length = patch_subnode.attribute("length").as_uint(0);
      }
      else if (patch_name == "folder")
      {
        auto& folder = patch.m_folder_patches.emplace_back();
        folder.m_disc = replace_variables(patch_subnode.attribute("disc").as_string());
        folder.m_external = replace_variables(patch_subnode.attribute("external").as_string());
        folder.m_resize = patch_subnode.attribute("resize").as_bool(true);
        folder.m_create = patch_subnode.attribute("create").as_bool(false);
        folder.m_recursive = patch_subnode.attribute("recursive").as_bool(true);
        folder.m_length = patch_subnode.attribute("length").as_uint(0);
      }
      else if (patch_name == "savegame")
      {
        auto& savegame = patch.m_savegame_patches.emplace_back();
        savegame.m_external = replace_variables(patch_subnode.attribute("external").as_string());
        savegame.m_clone = patch_subnode.attribute("clone").as_bool(true);
      }
      else if (patch_name == "memory")
      {
        auto& memory = patch.m_memory_patches.emplace_back();
        memory.m_offset = patch_subnode.attribute("offset").as_uint(0);
        memory.m_value = read_hex_string(patch_subnode.attribute("value").as_string());
        memory.m_valuefile = replace_variables(patch_subnode.attribute("valuefile").as_string());
        memory.m_original = read_hex_string(patch_subnode.attribute("original").as_string());
        memory.m_ocarina = patch_subnode.attribute("ocarina").as_bool(false);
        memory.m_search = patch_subnode.attribute("search").as_bool(false);
        memory.m_align = patch_subnode.attribute("align").as_uint(1);
      }
    }
  }

  return disc;
}
}  // namespace DiscIO::Riivolution
