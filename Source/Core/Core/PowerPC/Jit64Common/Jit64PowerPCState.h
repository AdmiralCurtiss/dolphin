// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/PowerPC.h"

// We offset by 0x80 because the range of one byte memory offsets is
// -0x80..0x7f.
#define PPCSTATE(x) MDisp(RPPCSTATE, static_cast<int>(offsetof(PowerPC::PowerPCState, x)) - 0x80)
#define PPCSTATE_LR PPCSTATE(spr[SPR_LR])
#define PPCSTATE_CTR PPCSTATE(spr[SPR_CTR])
#define PPCSTATE_SRR0 PPCSTATE(spr[SPR_SRR0])
#define PPCSTATE_SRR1 PPCSTATE(spr[SPR_SRR1])
