// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct FSTBuilderNode;
}

namespace DiscIO::Riivolution
{
// Replaces, adds, or modifies a file on disc.
struct File
{
  // Path of the file on disc to modify.
  std::string m_disc;

  // Path of the file on SD card to use for modification.
  std::string m_external;

  // If true, the file on the disc is truncated if the external file end is before the disc file
  // end. If false, the bytes after the external file end stay as they were.
  bool m_resize = true;

  // If true, a new file is created if it does not already exist at the disc path. Otherwise this
  // modification is ignored if the file does not exist on disc.
  bool m_create = false;

  // Offset of where to start replacing bytes in the on-disc file.
  u32 m_offset = 0;

  // Amount of bytes to copy from the external file to the disc file.
  // TODO: If this is longer than the amount of bytes available in the external file, it's
  // zero-padded, I think?
  u32 m_length = 0;
};

// Adds or modifies a folder on disc.
struct Folder
{
  // Path of the folder on disc to modify.
  // TODO: Can be left empty, but I don't understand what happens in this case, need to check
  // that...
  std::string m_disc;

  // Path of the folder on SD card to use for modification.
  std::string m_external;

  // Like File::m_resize but for each file in the folder.
  bool m_resize = true;

  // Like File::m_create but for each file in the folder.
  bool m_create = false;

  // Whether to also traverse subdirectories. (TODO: of the disc folder? external folder? both?)
  bool m_recursive = true;

  // Like File::m_length but for each file in the folder.
  u32 m_length = 0;
};

// Redirects the save file from the Wii NAND to a folder on SD card.
struct Savegame
{
  // The folder on SD card to use for the save files. Is created if it does not exist.
  std::string m_external;

  // If this is set to true and the external folder is empty or does not exist, the existing save on
  // NAND is copied to the new folder on game boot.
  bool m_clone = true;
};

// Modifies the game RAM right before jumping into the game executable.
struct Memory
{
  // Memory address where this modification takes place.
  u32 m_offset = 0;

  // Bytes to write to that address.
  std::vector<u8> m_value;

  // Like m_value, but read the bytes from a file instead.
  std::string m_valuefile;

  // If set, the memory at that address will be checked before the value is written, and the
  // replacement value only written if the bytes there match this.
  std::vector<u8> m_original;

  // If true, this memory patch is an ocarina-style patch.
  // TODO: I'm unsure what this means exactly, need to check some examples...
  bool m_ocarina = false;

  // If true, the offset is not known, and instead we should search for the m_original bytes in
  // memory and replace them where found.
  // TODO: I'm also a bit unsure what happens here. Does it just search the entirety of memory for
  // matches? Does it stop after the first match?
  bool m_search = false;

  // For m_search. The byte stride between search points.
  u32 m_align = 1;
};

struct Patch
{
  // Internal name of this patch.
  std::string m_id;

  // Defines a SD card path that all other paths are relative to.
  // We need to manually set this somehow because we have no SD root, and should ignore the path
  // from the XML.
  std::string m_root;

  std::vector<File> m_file_patches;
  std::vector<Folder> m_folder_patches;
  std::vector<Savegame> m_savegame_patches;
  std::vector<Memory> m_memory_patches;
};

struct Disc
{
  // Riivolution version. Only '1' exists at time of writing.
  int m_version;

  // Default root for patches where no other root is specified.
  std::string m_root;

  std::vector<Patch> m_patches;
};

std::optional<Disc> ParseFile(const std::string& filename, const std::string& game_id, u16 revision,
                              u8 disc_number);
std::optional<Disc> ParseString(std::string_view xml, const std::string& game_id, u16 revision,
                                u8 disc_number);

void ApplyPatchToDOL(const Patch& patch, DiscIO::FSTBuilderNode* dol_node);
void ApplyPatchToFST(const Patch& patch, std::vector<DiscIO::FSTBuilderNode>* fst);
}  // namespace DiscIO::Riivolution
