// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/RiivolutionPatcher.h"

#include <pugixml.hpp>

#include "Common/IOFile.h"
#include "DiscIO/DirectoryBlob.h"

namespace DiscIO::Riivolution
{
std::optional<Disc> ParseFile(const std::string& filename, const std::string& game_id, u16 revision,
                              u8 disc_number)
{
  ::File::IOFile f(filename, "rb");
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

  const auto patches = wiidisc.children("patch");
  for (const auto& patch_node : patches)
  {
    Patch& patch = disc.m_patches.emplace_back();
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

// 'before' and 'after' should be two copies of the same source
// 'split_at' needs to be between the start and end of the source, may not match either boundary
static void SplitAt(BuilderContentSource* before, BuilderContentSource* after, u64 split_at)
{
  const u64 start = before->m_offset;
  const u64 size = before->m_size;
  const u64 end = start + size;

  // The source before the split point just needs its length reduced.
  before->m_size = split_at - start;

  // The source after the split needs its length reduced and its start point adjusted.
  after->m_offset += before->m_size;
  after->m_size = end - split_at;
  if (std::holds_alternative<ContentFile>(after->m_source))
    std::get<ContentFile>(after->m_source).m_offset += before->m_size;
  else if (std::holds_alternative<const u8*>(after->m_source))
    std::get<const u8*>(after->m_source) += before->m_size;
  else if (std::holds_alternative<ContentVolume>(after->m_source))
    std::get<ContentVolume>(after->m_source).m_offset += before->m_size;
}

static void ApplyPatchToFile(const Patch& patch, const File& file_patch,
                             DiscIO::FSTBuilderNode* file_node)
{
  std::string external_filename = patch.m_root + "/" + file_patch.m_external;
  ::File::IOFile f(external_filename, "rb");
  if (!f)
    return;

  auto& content = std::get<std::vector<BuilderContentSource>>(file_node->m_content);

  const u64 external_filesize = f.GetSize();

  const u64 patch_start = file_patch.m_offset;
  const u64 patch_size = file_patch.m_length == 0 ? external_filesize : file_patch.m_length;
  const u64 patch_end = patch_start + patch_size;

  const u64 target_filesize =
      file_patch.m_resize ? patch_end : std::max(file_node->m_size, patch_end);

  // If the patch is past the end of the existing file no existing content needs to be touched, just
  // extend the file.
  if (patch_start >= file_node->m_size)
  {
    if (patch_start > file_node->m_size)
    {
      // Insert an padding area between the old file and the patch data.
      content.emplace_back(BuilderContentSource{file_node->m_size, patch_start - file_node->m_size,
                                                ContentFixedByte{0}});
    }

    // Insert the actual patch data, possibly with zero-padding.
    if (patch_size > external_filesize)
    {
      content.emplace_back(BuilderContentSource{patch_start, external_filesize,
                                                ContentFile{0, std::move(external_filename)}});
      content.emplace_back(BuilderContentSource{
          patch_start + external_filesize, patch_size - external_filesize, ContentFixedByte{0}});
    }
    else
    {
      content.emplace_back(BuilderContentSource{patch_start, patch_size,
                                                ContentFile{0, std::move(external_filename)}});
    }
  }
  else
  {
    // Patch is at the start or somewhere in the middle of the existing file. At least one source
    // needs to be modified or removed, and a new source with the patch data inserted instead.
    // To make this easier, we first split up existing sources at the patch start and patch end
    // offsets, then discard all overlapping sources and insert the patch sources there.
    for (size_t i = 0; i < content.size(); ++i)
    {
      const u64 source_start = content[i].m_offset;
      const u64 source_end = source_start + content[i].m_size;
      if (patch_start > source_start && patch_start < source_end)
      {
        content.insert(content.begin() + i + 1, content[i]);
        SplitAt(&content[i], &content[i + 1], patch_start);
        continue;
      }
      if (patch_end > source_start && patch_end < source_end)
      {
        content.insert(content.begin() + i + 1, content[i]);
        SplitAt(&content[i], &content[i + 1], patch_end);
      }
    }

    // Now discard the overlapping areas and remember where they were.
    size_t insert_where = 0;
    for (size_t i = 0; i < content.size(); ++i)
    {
      if (patch_start == content[i].m_offset)
      {
        insert_where = i;
        while (i < content.size() && patch_end >= content[i].m_offset + content[i].m_size)
          content.erase(content.begin() + i);
      }
    }

    // And finally, insert the patch data.
    if (patch_size > external_filesize)
    {
      content.emplace(content.begin() + insert_where,
                      BuilderContentSource{patch_start + external_filesize,
                                           patch_size - external_filesize, ContentFixedByte{0}});
      content.emplace(content.begin() + insert_where,
                      BuilderContentSource{patch_start, external_filesize,
                                           ContentFile{0, std::move(external_filename)}});
    }
    else
    {
      content.emplace(content.begin() + insert_where,
                      BuilderContentSource{patch_start, patch_size,
                                           ContentFile{0, std::move(external_filename)}});
    }
  }

  // Update the filesize of the file.
  file_node->m_size = target_filesize;
}

void ApplyPatchToDOL(const Patch& patch, DiscIO::FSTBuilderNode* dol_node)
{
  const auto& files = patch.m_file_patches;
  auto main_dol_patch =
      std::find_if(files.begin(), files.end(),
                   [](const DiscIO::Riivolution::File& f) { return f.m_disc == "main.dol"; });
  if (main_dol_patch != files.end())
    ApplyPatchToFile(patch, *main_dol_patch, dol_node);
}

void ApplyPatchToFST(const Patch& patch, std::vector<DiscIO::FSTBuilderNode>* fst)
{
}
}  // namespace DiscIO::Riivolution
