// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// PatchEngine
// Supports simple memory patches, and has a partial Action Replay implementation
// in ActionReplay.cpp/h.

#include "Core/PatchEngine.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/Debug/MemoryPatches.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ActionReplay.h"
#include "Core/CheatCodes.h"
#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

namespace PatchEngine
{
constexpr std::array<const char*, 3> s_patch_type_strings{{
    "byte",
    "word",
    "dword",
}};

static std::vector<Patch> s_on_frame;
static std::vector<std::size_t> s_on_frame_memory;
static std::mutex s_on_frame_memory_mutex;
static std::map<u32, int> s_speed_hacks;

const char* PatchTypeAsString(PatchType type)
{
  return s_patch_type_strings.at(static_cast<int>(type));
}

std::optional<PatchEntry> DeserializeLine(std::string line)
{
  std::string::size_type loc = line.find('=');
  if (loc != std::string::npos)
    line[loc] = ':';

  const std::vector<std::string> items = SplitString(line, ':');
  PatchEntry entry;

  if (items.size() < 3)
    return std::nullopt;

  if (!TryParse(items[0], &entry.address))
    return std::nullopt;
  if (!TryParse(items[2], &entry.value))
    return std::nullopt;

  if (items.size() >= 4)
  {
    if (!TryParse(items[3], &entry.comparand))
      return std::nullopt;
    entry.conditional = true;
  }

  const auto iter = std::find(s_patch_type_strings.begin(), s_patch_type_strings.end(), items[1]);
  if (iter == s_patch_type_strings.end())
    return std::nullopt;
  entry.type = static_cast<PatchType>(std::distance(s_patch_type_strings.begin(), iter));

  return entry;
}

std::string SerializeLine(const PatchEntry& entry)
{
  if (entry.conditional)
  {
    return fmt::format("0x{:08X}:{}:0x{:08X}:0x{:08X}", entry.address,
                       PatchEngine::PatchTypeAsString(entry.type), entry.value, entry.comparand);
  }
  else
  {
    return fmt::format("0x{:08X}:{}:0x{:08X}", entry.address,
                       PatchEngine::PatchTypeAsString(entry.type), entry.value);
  }
}

void LoadPatchSection(const std::string& section, std::vector<Patch>* patches,
                      const IniFile& globalIni, const IniFile& localIni)
{
  const IniFile* inis[2] = {&globalIni, &localIni};

  for (const IniFile* ini : inis)
  {
    std::vector<std::string> lines;
    Patch currentPatch;
    ini->GetLines(section, &lines);

    for (std::string& line : lines)
    {
      if (line.empty())
        continue;

      if (line[0] == '$')
      {
        // Take care of the previous code
        if (!currentPatch.name.empty())
        {
          patches->push_back(currentPatch);
        }
        currentPatch.entries.clear();

        // Set name and whether the patch is user defined
        currentPatch.name = line.substr(1, line.size() - 1);
        currentPatch.user_defined = (ini == &localIni);
      }
      else
      {
        if (std::optional<PatchEntry> entry = DeserializeLine(line))
          currentPatch.entries.push_back(*entry);
      }
    }

    if (!currentPatch.name.empty() && !currentPatch.entries.empty())
    {
      patches->push_back(currentPatch);
    }

    ReadEnabledAndDisabled(*ini, section, patches);

    if (ini == &globalIni)
    {
      for (Patch& patch : *patches)
        patch.default_enabled = patch.enabled;
    }
  }
}

void SavePatchSection(IniFile* local_ini, const std::vector<Patch>& patches)
{
  std::vector<std::string> lines;
  std::vector<std::string> lines_enabled;
  std::vector<std::string> lines_disabled;

  for (const auto& patch : patches)
  {
    if (patch.enabled != patch.default_enabled)
      (patch.enabled ? lines_enabled : lines_disabled).emplace_back('$' + patch.name);

    if (!patch.user_defined)
      continue;

    lines.emplace_back('$' + patch.name);

    for (const PatchEntry& entry : patch.entries)
      lines.emplace_back(SerializeLine(entry));
  }

  local_ini->SetLines("OnFrame_Enabled", lines_enabled);
  local_ini->SetLines("OnFrame_Disabled", lines_disabled);
  local_ini->SetLines("OnFrame", lines);
}

static void LoadSpeedhacks(const std::string& section, IniFile& ini)
{
  std::vector<std::string> keys;
  ini.GetKeys(section, &keys);
  for (const std::string& key : keys)
  {
    std::string value;
    ini.GetOrCreateSection(section)->Get(key, &value, "BOGUS");
    if (value != "BOGUS")
    {
      u32 address;
      u32 cycles;
      bool success = true;
      success &= TryParse(key, &address);
      success &= TryParse(value, &cycles);
      if (success)
      {
        s_speed_hacks[address] = static_cast<int>(cycles);
      }
    }
  }
}

int GetSpeedhackCycles(const u32 addr)
{
  const auto iter = s_speed_hacks.find(addr);
  if (iter == s_speed_hacks.end())
    return 0;

  return iter->second;
}

void LoadPatches()
{
  IniFile merged = SConfig::GetInstance().LoadGameIni();
  IniFile globalIni = SConfig::GetInstance().LoadDefaultGameIni();
  IniFile localIni = SConfig::GetInstance().LoadLocalGameIni();

  LoadPatchSection("OnFrame", &s_on_frame, globalIni, localIni);

  // Check if I'm syncing Codes
  if (Config::Get(Config::SESSION_CODE_SYNC_OVERRIDE))
  {
    Gecko::SetSyncedCodesAsActive();
    ActionReplay::SetSyncedCodesAsActive();
  }
  else
  {
    Gecko::SetActiveCodes(Gecko::LoadCodes(globalIni, localIni));
    ActionReplay::LoadAndApplyCodes(globalIni, localIni);
  }

  LoadSpeedhacks("Speedhacks", merged);
}

static void ApplyPatches(const std::vector<Patch>& patches)
{
  for (const Patch& patch : patches)
  {
    if (patch.enabled)
    {
      for (const PatchEntry& entry : patch.entries)
      {
        u32 addr = entry.address;
        u32 value = entry.value;
        u32 comparand = entry.comparand;
        switch (entry.type)
        {
        case PatchType::Patch8Bit:
          if (!entry.conditional || PowerPC::HostRead_U8(addr) == static_cast<u8>(comparand))
            PowerPC::HostWrite_U8(static_cast<u8>(value), addr);
          break;
        case PatchType::Patch16Bit:
          if (!entry.conditional || PowerPC::HostRead_U16(addr) == static_cast<u16>(comparand))
            PowerPC::HostWrite_U16(static_cast<u16>(value), addr);
          break;
        case PatchType::Patch32Bit:
          if (!entry.conditional || PowerPC::HostRead_U32(addr) == comparand)
            PowerPC::HostWrite_U32(value, addr);
          break;
        default:
          // unknown patchtype
          break;
        }
      }
    }
  }
}

static void ApplyMemoryPatches(std::span<const std::size_t> memory_patch_indices)
{
  std::lock_guard lock(s_on_frame_memory_mutex);
  for (std::size_t index : memory_patch_indices)
  {
    PowerPC::debug_interface.ApplyExistingPatch(index);
  }
}

// Requires MSR.DR, MSR.IR
// There's no perfect way to do this, it's just a heuristic.
// We require at least 2 stack frames, if the stack is shallower than that then it won't work.
static bool IsStackSane()
{
  DEBUG_ASSERT(PowerPC::ppcState.msr.DR && PowerPC::ppcState.msr.IR);

  // Check the stack pointer
  u32 SP = PowerPC::ppcState.gpr[1];
  if (!PowerPC::HostIsRAMAddress(SP))
    return false;

  // Read the frame pointer from the stack (find 2nd frame from top), assert that it makes sense
  u32 next_SP = PowerPC::HostRead_U32(SP);
  if (next_SP <= SP || !PowerPC::HostIsRAMAddress(next_SP) ||
      !PowerPC::HostIsRAMAddress(next_SP + 4))
    return false;

  // Check the link register makes sense (that it points to a valid IBAT address)
  const u32 address = PowerPC::HostRead_U32(next_SP + 4);
  return PowerPC::HostIsInstructionRAMAddress(address) &&
         0 != PowerPC::HostRead_Instruction(address);
}

void AddMemoryPatch(std::size_t index)
{
  std::lock_guard lock(s_on_frame_memory_mutex);
  s_on_frame_memory.push_back(index);
}

void RemoveMemoryPatch(std::size_t index)
{
  std::lock_guard lock(s_on_frame_memory_mutex);
  s_on_frame_memory.erase(std::remove(s_on_frame_memory.begin(), s_on_frame_memory.end(), index),
                          s_on_frame_memory.end());
}

bool ApplyFramePatches()
{
  // Because we're using the VI Interrupt to time this instead of patching the game with a
  // callback hook we can end up catching the game in an exception vector.
  // We deal with this by returning false so that SystemTimers will reschedule us in a few cycles
  // where we can try again after the CPU hopefully returns back to the normal instruction flow.
  if (!PowerPC::ppcState.msr.DR || !PowerPC::ppcState.msr.IR || !IsStackSane())
  {
    DEBUG_LOG_FMT(ACTIONREPLAY,
                  "Need to retry later. CPU configuration is currently incorrect. PC = {:#010x}, "
                  "MSR = {:#010x}",
                  PowerPC::ppcState.pc, PowerPC::ppcState.msr.Hex);
    return false;
  }

  ApplyPatches(s_on_frame);
  ApplyMemoryPatches(s_on_frame_memory);

  // Run the Gecko code handler
  Gecko::RunCodeHandler();
  ActionReplay::RunAllActive();

  return true;
}

void Shutdown()
{
  s_on_frame.clear();
  s_speed_hacks.clear();
  ActionReplay::ApplyCodes({});
  Gecko::Shutdown();
}

void Reload()
{
  Shutdown();
  LoadPatches();
}

}  // namespace PatchEngine
