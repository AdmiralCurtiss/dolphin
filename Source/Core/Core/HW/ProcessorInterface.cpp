// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/ProcessorInterface.h"

#include <cstdio>
#include <memory>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"

namespace ProcessorInterface
{
constexpr u32 FLIPPER_REV_A = 0x046500B0;
constexpr u32 FLIPPER_REV_B = 0x146500B1;
constexpr u32 FLIPPER_REV_C = 0x246500B1;

void ProcessorInterfaceState::DoState(PointerWrap& p)
{
  p.Do(m_InterruptMask);
  p.Do(m_InterruptCause);
  p.Do(Fifo_CPUBase);
  p.Do(Fifo_CPUEnd);
  p.Do(Fifo_CPUWritePointer);
  p.Do(m_ResetCode);
}

void ProcessorInterfaceState::Init()
{
  m_InterruptMask = 0;
  m_InterruptCause = 0;

  Fifo_CPUBase = 0;
  Fifo_CPUEnd = 0;
  Fifo_CPUWritePointer = 0;

  m_ResetCode = 0;  // Cold reset
  m_InterruptCause = INT_CAUSE_RST_BUTTON | INT_CAUSE_VI;

  toggleResetButton = CoreTiming::RegisterEvent("ToggleResetButton", ToggleResetButtonCallback);
  iosNotifyResetButton =
      CoreTiming::RegisterEvent("IOSNotifyResetButton", IOSNotifyResetButtonCallback);
  iosNotifyPowerButton =
      CoreTiming::RegisterEvent("IOSNotifyPowerButton", IOSNotifyPowerButtonCallback);
}

void ProcessorInterfaceState::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(base | PI_INTERRUPT_CAUSE, MMIO::DirectRead<u32>(&m_InterruptCause),
                 MMIO::ComplexWrite<u32>([this](u32, u32 val) {
                   m_InterruptCause &= ~val;
                   UpdateException();
                 }));

  mmio->Register(base | PI_INTERRUPT_MASK, MMIO::DirectRead<u32>(&m_InterruptMask),
                 MMIO::ComplexWrite<u32>([this](u32, u32 val) {
                   m_InterruptMask = val;
                   UpdateException();
                 }));

  mmio->Register(base | PI_FIFO_BASE, MMIO::DirectRead<u32>(&Fifo_CPUBase),
                 MMIO::DirectWrite<u32>(&Fifo_CPUBase, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_END, MMIO::DirectRead<u32>(&Fifo_CPUEnd),
                 MMIO::DirectWrite<u32>(&Fifo_CPUEnd, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_WPTR, MMIO::DirectRead<u32>(&Fifo_CPUWritePointer),
                 MMIO::DirectWrite<u32>(&Fifo_CPUWritePointer, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_RESET, MMIO::InvalidRead<u32>(),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   // Used by GXAbortFrame
                   INFO_LOG_FMT(PROCESSORINTERFACE, "Wrote PI_FIFO_RESET: {:08x}", val);
                   if ((val & 1) != 0)
                   {
                     GPFifo::ResetGatherPipe();

                     // Call Fifo::ResetVideoBuffer() from the video thread. Since that function
                     // resets various pointers used by the video thread, we can't call it directly
                     // from the CPU thread, so queue a task to do it instead. In single-core mode,
                     // AsyncRequests is in passthrough mode, so this will be safely and immediately
                     // called on the CPU thread.

                     // NOTE: GPFifo::ResetGatherPipe() only affects
                     // CPU state, so we can call it directly

                     AsyncRequests::Event ev = {};
                     ev.type = AsyncRequests::Event::FIFO_RESET;
                     AsyncRequests::GetInstance()->PushEvent(ev);
                   }
                 }));

  mmio->Register(base | PI_RESET_CODE, MMIO::ComplexRead<u32>([this](u32) {
                   DEBUG_LOG_FMT(PROCESSORINTERFACE, "Read PI_RESET_CODE: {:08x}", m_ResetCode);
                   return m_ResetCode;
                 }),
                 MMIO::ComplexWrite<u32>([this](u32, u32 val) {
                   m_ResetCode = val;
                   INFO_LOG_FMT(PROCESSORINTERFACE, "Wrote PI_RESET_CODE: {:08x}", m_ResetCode);
                   if (!SConfig::GetInstance().bWii && ~m_ResetCode & 0x4)
                   {
                     DVDInterface::ResetDrive(true);
                   }
                 }));

  mmio->Register(base | PI_FLIPPER_REV, MMIO::Constant<u32>(FLIPPER_REV_C),
                 MMIO::InvalidWrite<u32>());

  // 16 bit reads are based on 32 bit reads.
  for (u32 i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToLarger<u16>(mmio, base | i, 16),
                   MMIO::InvalidWrite<u16>());
    mmio->Register(base | (i + 2), MMIO::ReadToLarger<u16>(mmio, base | i, 0),
                   MMIO::InvalidWrite<u16>());
  }
}

void ProcessorInterfaceState::UpdateException()
{
  if ((m_InterruptCause & m_InterruptMask) != 0)
    PowerPC::ppcState.Exceptions |= EXCEPTION_EXTERNAL_INT;
  else
    PowerPC::ppcState.Exceptions &= ~EXCEPTION_EXTERNAL_INT;
}

static const char* Debug_GetInterruptName(u32 cause_mask)
{
  switch (cause_mask)
  {
  case INT_CAUSE_PI:
    return "INT_CAUSE_PI";
  case INT_CAUSE_DI:
    return "INT_CAUSE_DI";
  case INT_CAUSE_RSW:
    return "INT_CAUSE_RSW";
  case INT_CAUSE_SI:
    return "INT_CAUSE_SI";
  case INT_CAUSE_EXI:
    return "INT_CAUSE_EXI";
  case INT_CAUSE_AI:
    return "INT_CAUSE_AI";
  case INT_CAUSE_DSP:
    return "INT_CAUSE_DSP";
  case INT_CAUSE_MEMORY:
    return "INT_CAUSE_MEMORY";
  case INT_CAUSE_VI:
    return "INT_CAUSE_VI";
  case INT_CAUSE_PE_TOKEN:
    return "INT_CAUSE_PE_TOKEN";
  case INT_CAUSE_PE_FINISH:
    return "INT_CAUSE_PE_FINISH";
  case INT_CAUSE_CP:
    return "INT_CAUSE_CP";
  case INT_CAUSE_DEBUG:
    return "INT_CAUSE_DEBUG";
  case INT_CAUSE_WII_IPC:
    return "INT_CAUSE_WII_IPC";
  case INT_CAUSE_HSP:
    return "INT_CAUSE_HSP";
  case INT_CAUSE_RST_BUTTON:
    return "INT_CAUSE_RST_BUTTON";
  default:
    return "!!! ERROR-unknown Interrupt !!!";
  }
}

void ProcessorInterfaceState::SetInterrupt(u32 cause_mask, bool set)
{
  DEBUG_ASSERT_MSG(POWERPC, Core::IsCPUThread(), "SetInterrupt from wrong thread");

  if (set && !(m_InterruptCause & cause_mask))
  {
    DEBUG_LOG_FMT(PROCESSORINTERFACE, "Setting Interrupt {} (set)",
                  Debug_GetInterruptName(cause_mask));
  }

  if (!set && (m_InterruptCause & cause_mask))
  {
    DEBUG_LOG_FMT(PROCESSORINTERFACE, "Setting Interrupt {} (clear)",
                  Debug_GetInterruptName(cause_mask));
  }

  if (set)
    m_InterruptCause |= cause_mask;
  else
    m_InterruptCause &= ~cause_mask;  // is there any reason to have this possibility?
  // F|RES: i think the hw devices reset the interrupt in the PI to 0
  // if the interrupt cause is eliminated. that isn't done by software (afaik)
  UpdateException();
}

void ProcessorInterfaceState::SetResetButton(Core::System& system, bool set)
{
  SetInterrupt(INT_CAUSE_RST_BUTTON, !set);
}

void ProcessorInterfaceState::ToggleResetButtonCallback(Core::System& system, u64 userdata,
                                                        s64 cyclesLate)
{
  system.GetProcessorInterfaceState().SetResetButton(system, !!userdata);
}

void ProcessorInterfaceState::IOSNotifyResetButtonCallback(Core::System& system, u64 userdata,
                                                           s64 cyclesLate)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return;

  auto stm = ios->GetDeviceByName("/dev/stm/eventhook");
  if (stm)
    std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->ResetButton();
}

void ProcessorInterfaceState::IOSNotifyPowerButtonCallback(Core::System& system, u64 userdata,
                                                           s64 cyclesLate)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return;

  auto stm = ios->GetDeviceByName("/dev/stm/eventhook");
  if (stm)
    std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->PowerButton();
}

void ProcessorInterfaceState::ResetButton_Tap()
{
  if (!Core::IsRunning())
    return;
  CoreTiming::ScheduleEvent(0, toggleResetButton, true, CoreTiming::FromThread::ANY);
  CoreTiming::ScheduleEvent(0, iosNotifyResetButton, 0, CoreTiming::FromThread::ANY);
  CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond() / 2, toggleResetButton, false,
                            CoreTiming::FromThread::ANY);
}

void ProcessorInterfaceState::PowerButton_Tap()
{
  if (!Core::IsRunning())
    return;
  CoreTiming::ScheduleEvent(0, iosNotifyPowerButton, 0, CoreTiming::FromThread::ANY);
}

}  // namespace ProcessorInterface
