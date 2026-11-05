/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#pragma once

#include "Core/PCI/Bridge/PCIBridge.h"
#include "Core/PCI/DmaEngine.h"
#include "Core/PCI/InterruptLine.h"
#include "Core/PCI/PCIDevice.h"
#include "Core/PCI/Worker.h"
#include "Core/RAM/RAM.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef GFX_ENABLED
  #include <SDL3/SDL.h>
  #include <SDL3/SDL_audio.h>
#endif

namespace Xe::PCIDev {

  // Xenon audio output controller (on-chip PCI 9.0, PCI ID 1414:580C, BAR0 phys 0xEA001600).
  //
  // Two DMA engines, I2S (analog/HDMI PCM) and S/PDIF, each walking a ring of 32 descriptors. Registers and descriptors
  // are little-endian, 32-bit accesses. A descriptor is { u32 physical address, u32 0x80000000 (IOC) | byte count },
  // and the kernel never leaves a slot empty: idle slots point at a silence buffer, so a running engine plays
  // continuously. Both channels run at 48 kHz with 4 bytes per frame, so a slot lasts bytes / 192000 s, and I2S carries
  // s16 LE stereo.
  //
  // STATUS [4:0] is the slot the engine plays next (read only). [12:8] is the stop slot: the ISR writes (hwSlot - 1) &
  // 31 on every interrupt. The engine plays the stop slot, then halts on it (HW slot stays put, HALTED + UNDERRUN)
  // until the stop slot moves (AC'97 "last valid index" semantics, at stream open both are 0). It advances the HW slot
  // and then raises DESC_DONE after every descriptor, all of them have IOC.
  //
  // CONTROL bits 2/3/4 (HALTED / DESC_DONE / UNDERRUN) are write-1-to-clear, and the ISR acknowledges by writing back
  // the value it read. Bit 24 is RUN, bit 25 RESET (self-clearing), bits 26-28 enable bits 2-4 onto the interrupt line,
  // and bit 0 IDLE reads set while the engine is stopped (Close spins on it after clearing RUN). CONFIG (+0x0C) is
  // shared by both channels, and its bit 12 makes S/PDIF follow the I2S PCM while no S/PDIF stream runs.
  //
  // The interrupt is IRQL 0x44 (PRIO_AUDIO), level sensitive, routed to hardware thread 4 by the kernel. Output is only
  // heard once the kernel enables it through SMC command 0x8D (SMC toggles the audio clamp line via GPIO?).

  static constexpr u32 AUDIO_DMA_NUM_DESCRIPTORS = 0x20;
  static constexpr u32 AUDIO_SAMPLE_RATE = 48000;
  static constexpr u32 AUDIO_BYTES_PER_FRAME = 4;

  // Descriptor as stored in guest RAM (little-endian).
  struct AudioPRDEntry {
    u32 bufferAddress;
    u32 controlSize; // bit 31 IOC, [15:0] byte count
  };

  // Register offsets from BAR0
  enum eAudioReg : u32 {
    AUDIO_I2S_RING_BASE = 0x00,
    AUDIO_I2S_STATUS = 0x04,
    AUDIO_I2S_CONTROL = 0x08,
    AUDIO_CONFIG = 0x0C,
    AUDIO_SPDIF_RING_BASE = 0x10,
    AUDIO_SPDIF_STATUS = 0x14,
    AUDIO_SPDIF_CONTROL = 0x18,
    AUDIO_SPDIF_CHANNEL_STATUS = 0x1C,
  };

  enum eAudioControlBits : u32 {
    AUDIO_CTRL_IDLE = 0x00000001,      // RO: engine stopped
    AUDIO_CTRL_HALTED = 0x00000004,    // W1C: halted on the stop slot
    AUDIO_CTRL_DESC_DONE = 0x00000008, // W1C: descriptor(s) completed
    AUDIO_CTRL_UNDERRUN = 0x00000010,  // W1C: ran out of descriptors
    AUDIO_CTRL_RUN = 0x01000000,
    AUDIO_CTRL_RESET = 0x02000000, // self-clearing
    AUDIO_CTRL_STATUS_W1C = AUDIO_CTRL_HALTED | AUDIO_CTRL_DESC_DONE | AUDIO_CTRL_UNDERRUN,
  };

  enum eAudioStatusBits : u32 {
    AUDIO_STATUS_HW_SLOT_MASK = 0x0000001F,
    AUDIO_STATUS_STOP_SLOT_SHIFT = 8,
    AUDIO_STATUS_STOP_SLOT_MASK = 0x00001F00,
  };

  // One DMA engine.
  struct AudioDMAChannel {
    u32 ringBase = 0;
    u32 hwSlot = 0;   // STATUS [4:0]
    u32 stopSlot = 0; // STATUS [12:8]
    u32 statusRW = 0; // other STATUS bits, kept as written
    u32 control = AUDIO_CTRL_IDLE;
    bool halted = false; // parked on the stop slot

    // Slot in progress: fetched when it starts, retired at slotEnd.
    bool inSlot = false;
    u32 slotAddress = 0;
    u32 slotBytes = 0;
    std::chrono::steady_clock::time_point slotEnd{};

    bool Running() const { return (control & AUDIO_CTRL_RUN) && ringBase != 0; }
    u32 Status() const {
      return (statusRW & ~(AUDIO_STATUS_HW_SLOT_MASK | AUDIO_STATUS_STOP_SLOT_MASK))
             | ((stopSlot << AUDIO_STATUS_STOP_SLOT_SHIFT) & AUDIO_STATUS_STOP_SLOT_MASK)
             | (hwSlot & AUDIO_STATUS_HW_SLOT_MASK);
    }
    // Status bits currently driving the interrupt line (CONTROL bit 24 + n enables status bit n).
    bool InterruptPending() const { return (control & AUDIO_CTRL_STATUS_W1C & (control >> 24)) != 0; }
  };

  class AUDIOCTRLR : public PCIDevice {
  public:
    // Size of the register window (BAR0).
    static constexpr u32 DevSize = 0x40;

    AUDIOCTRLR(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram);
    ~AUDIOCTRLR();

    void Read(u64 offset, u8 *data, u64 size) override;
    void Write(u64 offset, const u8 *data, u64 size) override;
    void PlatformReset() override;

    // SMC command 0x8D: gates what reaches the host device. The DMA engines run regardless.
    void SetAudioOutputEnabled(bool enabled);

    // The host side: mute and volume take effect right away, a new device reopens the output on it.
    void ApplyHostSettings();
    // The host's playback devices by name.
    static std::vector<std::string> HostDevices();

  private:
    // I2S = 0, S/PDIF = 1.
    AudioDMAChannel channels[2] = {};
    u32 config = 0;             // bit 12: S/PDIF follows the I2S PCM while no S/PDIF stream runs
    u32 spdifChannelStatus = 0; // IEC 60958 channel status. Bit 1 set = a bitstream (AC-3), not PCM
    std::mutex channelMutex;

    std::atomic_bool audioOutEnabled{false};

    Xe::PCI::InterruptLine interruptLine;
    Xe::PCI::DmaEngine dma;

    // SDL3 Audio output backend
#ifdef GFX_ENABLED
    SDL_AudioStream *sdlAudioStream = nullptr;
    bool sdlAudioInitialized = false;
    std::string sdlDeviceName; // the Config::audio.device the stream was opened on
    void InitSDLAudio();
    void ShutdownSDLAudio();
    void FeedSDLAudio(const u8 *pcmData, u32 size);
#endif

    // DMA engine thread: plays both channels in real time.
    Xe::PCI::Worker dmaThread;
    std::atomic_bool dmaThreadRunning{false};
    std::condition_variable dmaCV;
    void WorkerLoop();

    // Starts the channel's next slot (fetches its descriptor) at start. Caller holds channelMutex.
    void StartSlot(int chIdx, std::chrono::steady_clock::time_point start);
    // Retires the slot in progress: output, advance or halt, DESC_DONE. Caller holds channelMutex.
    void FinishSlot(int chIdx);
    // Sends a finished slot's samples to the host if this channel is the audible one. Caller holds channelMutex.
    void OutputSlot(int chIdx, u32 address, u32 bytes);

    AudioPRDEntry ReadPRDEntry(u32 ringBase, u32 index);

    void HandleControlWrite(int chIdx, u32 value);
    void HandleStatusWrite(int chIdx, u32 value);

    // Level-sensitive line: forward the edges of (pending & enabled) to the IIC.
    // acknowledged: the ISR just wrote CONTROL back.
    void UpdateInterruptLine(bool acknowledged = false);
  };

} // namespace Xe::PCIDev
