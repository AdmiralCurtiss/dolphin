// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Win32/Win32Keyboard.h"

#include <algorithm>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"

#include "Core/Core.h"
#include "Core/Host.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::Win32Keyboard
{
static bool s_keyboard_initialized = false;

void PopulateDevices()
{
  if (s_keyboard_initialized)
    return;
  s_keyboard_initialized = true;
  g_controller_interface.AddDevice(std::make_shared<ciface::Win32Keyboard::Keyboard>());
}

static const struct
{
  const BYTE code;
  const char* const name;
} named_keys[] = {
    {VK_LBUTTON, "Click 0"},
    {VK_RBUTTON, "Click 1"},
    {VK_MBUTTON, "Click 2"},
    {VK_XBUTTON1, "Click 3"},
    {VK_XBUTTON2, "Click 4"},

    {VK_BACK, "BACK"},
    {VK_TAB, "TAB"},
    {VK_CLEAR, "CLEAR"},
    {VK_RETURN, "RETURN"},

    {VK_SHIFT, "Shift"},
    {VK_CONTROL, "Ctrl"},
    {VK_MENU, "Alt"},
    {VK_PAUSE, "PAUSE"},
    {VK_CAPITAL, "CAPITAL"},
    {VK_KANA, "KANA"},
    {VK_IME_ON, "IME_ON"},
    {VK_JUNJA, "JUNJA"},
    {VK_FINAL, "FINAL"},
    {VK_KANJI, "KANJI"},
    {VK_IME_OFF, "IME_OFF"},
    {VK_ESCAPE, "ESCAPE"},
    {VK_CONVERT, "CONVERT"},
    {VK_NONCONVERT, "NONCONVERT"},
    {VK_ACCEPT, "ACCEPT"},
    {VK_MODECHANGE, "MODECHANGE"},
    {VK_SPACE, "SPACE"},
    {VK_PRIOR, "PRIOR"},
    {VK_NEXT, "NEXT"},
    {VK_END, "END"},
    {VK_HOME, "HOME"},
    {VK_LEFT, "LEFT"},
    {VK_UP, "UP"},
    {VK_RIGHT, "RIGHT"},
    {VK_DOWN, "DOWN"},
    {VK_SELECT, "SELECT"},
    {VK_PRINT, "PRINT"},
    {VK_EXECUTE, "EXECUTE"},
    {VK_SNAPSHOT, "SYSRQ"},
    {VK_INSERT, "INSERT"},
    {VK_DELETE, "DELETE"},
    {VK_HELP, "HELP"},

    {'0', "0"},
    {'1', "1"},
    {'2', "2"},
    {'3', "3"},
    {'4', "4"},
    {'5', "5"},
    {'6', "6"},
    {'7', "7"},
    {'8', "8"},
    {'9', "9"},
    {'A', "A"},
    {'B', "B"},
    {'C', "C"},
    {'D', "D"},
    {'E', "E"},
    {'F', "F"},
    {'G', "G"},
    {'H', "H"},
    {'I', "I"},
    {'J', "J"},
    {'K', "K"},
    {'L', "L"},
    {'M', "M"},
    {'N', "N"},
    {'O', "O"},
    {'P', "P"},
    {'Q', "Q"},
    {'R', "R"},
    {'S', "S"},
    {'T', "T"},
    {'U', "U"},
    {'V', "V"},
    {'W', "W"},
    {'X', "X"},
    {'Y', "Y"},
    {'Z', "Z"},

    {VK_LWIN, "LWIN"},
    {VK_RWIN, "RWIN"},
    {VK_APPS, "APPS"},
    {VK_SLEEP, "SLEEP"},
    {VK_NUMPAD0, "NUMPAD0"},
    {VK_NUMPAD1, "NUMPAD1"},
    {VK_NUMPAD2, "NUMPAD2"},
    {VK_NUMPAD3, "NUMPAD3"},
    {VK_NUMPAD4, "NUMPAD4"},
    {VK_NUMPAD5, "NUMPAD5"},
    {VK_NUMPAD6, "NUMPAD6"},
    {VK_NUMPAD7, "NUMPAD7"},
    {VK_NUMPAD8, "NUMPAD8"},
    {VK_NUMPAD9, "NUMPAD9"},
    {VK_MULTIPLY, "MULTIPLY"},
    {VK_ADD, "ADD"},
    {VK_SEPARATOR, "SEPARATOR"},
    {VK_SUBTRACT, "SUBTRACT"},
    {VK_DECIMAL, "DECIMAL"},
    {VK_DIVIDE, "DIVIDE"},
    {VK_F1, "F1"},
    {VK_F2, "F2"},
    {VK_F3, "F3"},
    {VK_F4, "F4"},
    {VK_F5, "F5"},
    {VK_F6, "F6"},
    {VK_F7, "F7"},
    {VK_F8, "F8"},
    {VK_F9, "F9"},
    {VK_F10, "F10"},
    {VK_F11, "F11"},
    {VK_F12, "F12"},
    {VK_F13, "F13"},
    {VK_F14, "F14"},
    {VK_F15, "F15"},
    {VK_F16, "F16"},
    {VK_F17, "F17"},
    {VK_F18, "F18"},
    {VK_F19, "F19"},
    {VK_F20, "F20"},
    {VK_F21, "F21"},
    {VK_F22, "F22"},
    {VK_F23, "F23"},
    {VK_F24, "F24"},
    {VK_NUMLOCK, "NUMLOCK"},
    {VK_SCROLL, "SCROLL"},
    {VK_LSHIFT, "LSHIFT"},
    {VK_RSHIFT, "RSHIFT"},
    {VK_LCONTROL, "LCONTROL"},
    {VK_RCONTROL, "RCONTROL"},
    {VK_LMENU, "LMENU"},
    {VK_RMENU, "RMENU"},
    {VK_BROWSER_BACK, "WEBBACK"},
    {VK_BROWSER_FORWARD, "WEBFORWARD"},
    {VK_BROWSER_REFRESH, "WEBREFRESH"},
    {VK_BROWSER_STOP, "WEBSTOP"},
    {VK_BROWSER_SEARCH, "WEBSEARCH"},
    {VK_BROWSER_FAVORITES, "WEBFAVORITES"},
    {VK_BROWSER_HOME, "WEBHOME"},
    {VK_VOLUME_MUTE, "MUTE"},
    {VK_VOLUME_DOWN, "VOLUMEDOWN"},
    {VK_VOLUME_UP, "VOLUMEUP"},
    {VK_MEDIA_NEXT_TRACK, "NEXTTRACK"},
    {VK_MEDIA_PREV_TRACK, "PREVTRACK"},
    {VK_MEDIA_STOP, "MEDIASTOP"},
    {VK_MEDIA_PLAY_PAUSE, "PLAYPAUSE"},
    {VK_LAUNCH_MAIL, "MAIL"},
    {VK_LAUNCH_MEDIA_SELECT, "MEDIASELECT"},
    {VK_LAUNCH_APP1, "APP1"},
    {VK_LAUNCH_APP2, "APP2"},
    {VK_OEM_1, "SEMICOLON"},
    {VK_OEM_PLUS, "EQUALS"},
    {VK_OEM_COMMA, "COMMA"},
    {VK_OEM_MINUS, "MINUS"},
    {VK_OEM_PERIOD, "PERIOD"},
    {VK_OEM_2, "SLASH"},
    {VK_OEM_3, "GRAVE"},
    {VK_OEM_4, "LBRACKET"},
    {VK_OEM_5, "BACKSLASH"},
    {VK_OEM_6, "RBRACKET"},
    {VK_OEM_7, "APOSTROPHE"},
    {VK_OEM_8, "OEM_8"},
    {VK_OEM_102, "OEM_102"},
    {VK_PROCESSKEY, "PROCESSKEY"},
    {VK_PACKET, "PACKET"},
    {VK_ATTN, "ATTN"},
    {VK_CRSEL, "CRSEL"},
    {VK_EXSEL, "EXSEL"},
    {VK_EREOF, "EREOF"},
    {VK_PLAY, "PLAY"},
    {VK_ZOOM, "ZOOM"},
    {VK_NONAME, "NONAME"},
    {VK_PA1, "PA1"},
    {VK_OEM_CLEAR, "OEM_CLEAR"},
};

Keyboard::~Keyboard()
{
  s_keyboard_initialized = false;
}

Keyboard::Keyboard() : m_last_update(GetTickCount()), m_state_in()
{
  for (u8 i = 0; i < sizeof(named_keys) / sizeof(*named_keys); ++i)
  {
    Key* key = new Key(i, m_state_in.keyboard[named_keys[i].code]);
    m_inputs.push_back(key);
    AddInput(key);
  }
}

Core::DeviceRemoval Keyboard::UpdateInput()
{
  for (Key* key : m_inputs)
  {
    if (GetAsyncKeyState(named_keys[key->m_index].code) & 0x8000)
    {
      key->m_key = BYTE(1);
    }
    else
    {
      key->m_key = BYTE(0);
    }
  }

  return Core::DeviceRemoval::Keep;
}

std::string Keyboard::GetName() const
{
  return "Keyboard";
}

std::string Keyboard::GetSource() const
{
  return "Win32";
}

int Keyboard::GetSortPriority() const
{
  return 0;
}

bool Keyboard::IsVirtualDevice() const
{
  return true;
}

std::string Keyboard::Key::GetName() const
{
  return named_keys[m_index].name;
}

ControlState Keyboard::Key::GetState() const
{
  return (m_key != 0);
}
}  // namespace ciface::Win32Keyboard
