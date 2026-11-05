/***************************************************************/
/* Copyright 2026 Xenon Emulator Project. All rights reserved. */
/***************************************************************/

#include "AudioController.h"

#include "Base/Config.h"
#include "Base/Logging/Log.h"
#include "Base/Thread.h"

#include <algorithm>

// Every register access, slot and engine state change is logged with this on (a lot of output):
// #define AUDIOCTRLR_DEBUG

#ifdef AUDIOCTRLR_DEBUG
  #define AUDIOCTRLR_DEBUGP(x, ...) LOG_DEBUG(AudioController, "[AUDIO] " x, ##__VA_ARGS__)
#else
  #define AUDIOCTRLR_DEBUGP(x, ...)
#endif

// Channel indices
static constexpr int CH_I2S = 0;
static constexpr int CH_SPDIF = 1;

// A slot that ends further than this in the past (host stall, debugger) restarts the channel's clock at "now"
// instead of retiring a burst of overdue slots back to back.
static constexpr auto kMaxLag = std::chrono::milliseconds(100);
// Host-side queue cap: past this much buffered audio a slot is dropped rather than queued (the emulated clock
// running ahead of the host device's).
static constexpr u32 kMaxQueuedBytes = Xe::PCIDev::AUDIO_SAMPLE_RATE * Xe::PCIDev::AUDIO_BYTES_PER_FRAME / 5; // 200 ms

Xe::PCIDev::AUDIOCTRLR::AUDIOCTRLR(const std::string &deviceName, u64 size, PCIBridge *parentPCIBridge, RAM *ram)
    : PCIDevice(deviceName, size)
    , interruptLine(parentPCIBridge, PRIO_AUDIO)
    , dma(ram) {
  // Set PCI Properties (Vendor 0x1414, Device 0x580C)
  pciConfigSpace.configSpaceHeader.id.hexData = 0x580C1414;
  pciConfigSpace.configSpaceHeader.commandStatus.hexData = 0x02880006;
  pciConfigSpace.configSpaceHeader.classRev.hexData = 0x04010001;
  pciConfigSpace.configSpaceHeader.subsystem.hexData = 0x75011039;
  pciConfigSpace.configSpaceHeader.interrupt.hexData = 0x0B340100;
  // Set our PCI Dev Sizes
  pciDevSizes[0] = DevSize; // BAR0

#ifdef GFX_ENABLED
  InitSDLAudio();
#endif

  dmaThreadRunning = true;
  dmaThread.Start("Audio DMA", [this] { WorkerLoop(); });
}

Xe::PCIDev::AUDIOCTRLR::~AUDIOCTRLR() {
  {
    std::lock_guard<std::mutex> lock(channelMutex);
    dmaThreadRunning = false;
  }
  dmaCV.notify_all();
  dmaThread.Join();

#ifdef GFX_ENABLED
  ShutdownSDLAudio();
#endif
}

// SDL3 Audio Backend

#ifdef GFX_ENABLED
// The playback device called name, the default one when it's empty or no longer there.
static SDL_AudioDeviceID FindPlaybackDevice(const std::string &name) {
  if (name.empty()) { return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK; }
  int count = 0;
  SDL_AudioDeviceID *ids = SDL_GetAudioPlaybackDevices(&count);
  SDL_AudioDeviceID found = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  for (int i = 0; ids && i < count; ++i) {
    const char *deviceName = SDL_GetAudioDeviceName(ids[i]);
    if (deviceName && name == deviceName) {
      found = ids[i];
      break;
    }
  }
  SDL_free(ids);
  if (found == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
    LOG_WARNING(AudioController, "Audio device '{}' not found, using the default one", name);
  }
  return found;
}

// Config::audio's volume as a gain: squared, so the slider's steps sound even.
static float HostGain() {
  if (Config::audio.mute) { return 0.f; }
  const float volume = static_cast<float>(std::clamp<s32>(Config::audio.volume, 0, 100)) / 100.f;
  return volume * volume;
}

void Xe::PCIDev::AUDIOCTRLR::InitSDLAudio() {
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_Init(SDL_INIT_AUDIO)) {
    LOG_ERROR(Xenon, "Failed to initialize SDL Audio subsystem: {}", SDL_GetError());
  }

  SDL_AudioSpec spec = {};
  spec.format = SDL_AUDIO_S16LE;
  spec.channels = 2;
  spec.freq = AUDIO_SAMPLE_RATE;

  sdlDeviceName = Config::audio.device;
  sdlAudioStream = SDL_OpenAudioDeviceStream(FindPlaybackDevice(sdlDeviceName), &spec, nullptr, nullptr);

  if (!sdlAudioStream) {
    LOG_ERROR(AudioController, "Failed to open SDL audio device: {}", SDL_GetError());
    sdlAudioInitialized = false;
    return;
  }

  SDL_SetAudioStreamGain(sdlAudioStream, HostGain());
  SDL_ResumeAudioStreamDevice(sdlAudioStream);
  sdlAudioInitialized = true;
  LOG_INFO(AudioController, "SDL3 audio backend initialized (48kHz, S16LE, Stereo) on {}",
           sdlDeviceName.empty() ? "the default device" : sdlDeviceName);
}

void Xe::PCIDev::AUDIOCTRLR::ShutdownSDLAudio() {
  if (sdlAudioStream) {
    SDL_DestroyAudioStream(sdlAudioStream);
    sdlAudioStream = nullptr;
  }
  sdlAudioInitialized = false;
}

void Xe::PCIDev::AUDIOCTRLR::FeedSDLAudio(const u8 *pcmData, u32 size) {
  if (!sdlAudioInitialized || !sdlAudioStream || !size) { return; }

  if (SDL_GetAudioStreamQueued(sdlAudioStream) > static_cast<int>(kMaxQueuedBytes)) {
    AUDIOCTRLR_DEBUGP("Host audio queue full, dropping {} bytes", size);
    return;
  }
  if (!SDL_PutAudioStreamData(sdlAudioStream, pcmData, static_cast<int>(size))) {
    LOG_ERROR(AudioController, "SDL_PutAudioStreamData failed: {}", SDL_GetError());
  }
}
#endif

// Descriptors

Xe::PCIDev::AudioPRDEntry Xe::PCIDev::AUDIOCTRLR::ReadPRDEntry(u32 ringBase, u32 index) {
  AudioPRDEntry entry = {};
  const u64 entryAddr = static_cast<u64>(ringBase) + index * sizeof(AudioPRDEntry);
  if (entryAddr > UINT32_MAX) { return entry; }

  // Little-endian in guest RAM, like the registers. Stays zero when the entry is outside RAM.
  dma.Read(static_cast<u32>(entryAddr), &entry, sizeof(entry));
  return entry;
}

// Interrupt line

// Level sensitive: asserted while any channel has a status bit set whose enable (bit 24 + n) is set. The line drops
// when the ISR writes CONTROL back to acknowledge.
void Xe::PCIDev::AUDIOCTRLR::UpdateInterruptLine(bool acknowledged) {
  interruptLine.Set(channels[CH_I2S].InterruptPending() || channels[CH_SPDIF].InterruptPending(), acknowledged);
}

// DMA engines

void Xe::PCIDev::AUDIOCTRLR::StartSlot(int chIdx, std::chrono::steady_clock::time_point start) {
  AudioDMAChannel &ch = channels[chIdx];
  // The descriptor is fetched when its slot starts: the driver may rewrite slots ahead of the engine until then.
  const AudioPRDEntry prd = ReadPRDEntry(ch.ringBase, ch.hwSlot);
  ch.slotAddress = prd.bufferAddress;
  ch.slotBytes = prd.controlSize & 0xFFFF;
  ch.inSlot = true;

  // 48 kHz, 4 bytes per frame on both channels. An empty descriptor still takes one frame so the engine can't spin.
  const u64 frames = std::max<u64>(ch.slotBytes / AUDIO_BYTES_PER_FRAME, 1);
  ch.slotEnd = start + std::chrono::nanoseconds(frames * 1'000'000'000ull / AUDIO_SAMPLE_RATE);

  OutputSlot(chIdx, ch.slotAddress, ch.slotBytes);
  AUDIOCTRLR_DEBUGP("{} slot {} start: {:#x} {} bytes", chIdx == CH_I2S ? "I2S" : "SPDIF", ch.hwSlot, ch.slotAddress,
                    ch.slotBytes);
}

void Xe::PCIDev::AUDIOCTRLR::FinishSlot(int chIdx) {
  AudioDMAChannel &ch = channels[chIdx];
  ch.inSlot = false;

  if (ch.hwSlot == ch.stopSlot) {
    // Played the last slot the driver handed over: park on it until the ISR moves the stop slot.
    ch.halted = true;
    ch.control |= AUDIO_CTRL_HALTED | AUDIO_CTRL_UNDERRUN | AUDIO_CTRL_DESC_DONE;
    AUDIOCTRLR_DEBUGP("{} halted on stop slot {}", chIdx == CH_I2S ? "I2S" : "SPDIF", ch.stopSlot);
  } else {
    // Advance first, then raise DESC_DONE: the ISR reads the HW slot once it sees the bit.
    ch.hwSlot = (ch.hwSlot + 1) % AUDIO_DMA_NUM_DESCRIPTORS;
    ch.control |= AUDIO_CTRL_DESC_DONE; // every descriptor the kernel writes has IOC
  }
  UpdateInterruptLine();
}

void Xe::PCIDev::AUDIOCTRLR::OutputSlot(int chIdx, u32 address, u32 bytes) {
  // The host device plays the I2S PCM (s16 LE stereo). S/PDIF carries either the same mix (CONFIG bit 12).
  if (chIdx != CH_I2S || !bytes || !audioOutEnabled.load(std::memory_order_relaxed)) { return; }
  if (!dma.Contains(address, bytes)) { return; }
#ifdef GFX_ENABLED
  FeedSDLAudio(dma.ReadPointer(address, bytes), bytes);
#endif
}

void Xe::PCIDev::AUDIOCTRLR::WorkerLoop() {
  using Clock = std::chrono::steady_clock;

  std::unique_lock<std::mutex> lock(channelMutex);
  while (dmaThreadRunning) {
    const auto now = Clock::now();
    auto wake = Clock::time_point::max();

    for (int chIdx : {CH_I2S, CH_SPDIF}) {
      AudioDMAChannel &ch = channels[chIdx];
      if (!ch.Running()) { continue; }

      // Retire every slot that has ended. The next one starts where the last ended, so the rate stays exact.
      while (ch.inSlot && now >= ch.slotEnd) {
        const auto end = ch.slotEnd;
        FinishSlot(chIdx);
        if (ch.halted) { break; }
        StartSlot(chIdx, now - end > kMaxLag ? now : end);
      }
      if (!ch.inSlot && !ch.halted) { StartSlot(chIdx, now); }
      if (ch.inSlot) { wake = std::min(wake, ch.slotEnd); }
    }

    if (wake == Clock::time_point::max()) {
      dmaCV.wait(lock); // nothing running: wait for RUN or a stop-slot update
    } else {
      dmaCV.wait_until(lock, wake);
    }
  }
}

// PCI Register Access

void Xe::PCIDev::AUDIOCTRLR::Read(u64 offset, u8 *data, u64 size) {
  const u32 regOffset = static_cast<u32>(offset);
  u32 dataOut = 0;
  {
    std::lock_guard<std::mutex> lock(channelMutex);
    switch (regOffset) {
      case AUDIO_I2S_RING_BASE: dataOut = channels[CH_I2S].ringBase; break;
      case AUDIO_I2S_STATUS: dataOut = channels[CH_I2S].Status(); break;
      case AUDIO_I2S_CONTROL: dataOut = channels[CH_I2S].control; break;
      case AUDIO_CONFIG: dataOut = config; break;
      case AUDIO_SPDIF_RING_BASE: dataOut = channels[CH_SPDIF].ringBase; break;
      case AUDIO_SPDIF_STATUS: dataOut = channels[CH_SPDIF].Status(); break;
      case AUDIO_SPDIF_CONTROL: dataOut = channels[CH_SPDIF].control; break;
      case AUDIO_SPDIF_CHANNEL_STATUS: dataOut = spdifChannelStatus; break;
      default: AUDIOCTRLR_DEBUGP("Unhandled read at offset {:#x}", regOffset); break;
    }
  }

  memset(data, 0, size);
  memcpy(data, &dataOut, std::min<u64>(size, sizeof(dataOut)));
  AUDIOCTRLR_DEBUGP("Read reg {:#x} = {:#x}", regOffset, dataOut);
}

void Xe::PCIDev::AUDIOCTRLR::Write(u64 offset, const u8 *data, u64 size) {
  const u32 regOffset = static_cast<u32>(offset);
  u32 dataIn = 0;
  memcpy(&dataIn, data, std::min<u64>(size, sizeof(dataIn)));
  AUDIOCTRLR_DEBUGP("Write reg {:#x} = {:#x}, size {}", regOffset, dataIn, size);

  std::lock_guard<std::mutex> lock(channelMutex);
  switch (regOffset) {
    case AUDIO_I2S_RING_BASE: channels[CH_I2S].ringBase = dataIn; break;
    case AUDIO_I2S_STATUS: HandleStatusWrite(CH_I2S, dataIn); break;
    case AUDIO_I2S_CONTROL: HandleControlWrite(CH_I2S, dataIn); break;
    case AUDIO_CONFIG: config = dataIn; break;
    case AUDIO_SPDIF_RING_BASE: channels[CH_SPDIF].ringBase = dataIn; break;
    case AUDIO_SPDIF_STATUS: HandleStatusWrite(CH_SPDIF, dataIn); break;
    case AUDIO_SPDIF_CONTROL: HandleControlWrite(CH_SPDIF, dataIn); break;
    case AUDIO_SPDIF_CHANNEL_STATUS: spdifChannelStatus = dataIn; break;
    default: AUDIOCTRLR_DEBUGP("Unhandled write at offset {:#x}, data {:#x}", regOffset, dataIn); break;
  }
  dmaCV.notify_all();
}

// Caller holds channelMutex.
void Xe::PCIDev::AUDIOCTRLR::HandleControlWrite(int chIdx, u32 value) {
  AudioDMAChannel &ch = channels[chIdx];
  const bool wasRunning = (ch.control & AUDIO_CTRL_RUN) != 0;

  // Status bits are write-1-to-clear (the ISR writes back what it read). IDLE is read only, RESET self-clearing.
  const u32 status = ch.control & (AUDIO_CTRL_STATUS_W1C | AUDIO_CTRL_IDLE) & ~(value & AUDIO_CTRL_STATUS_W1C);
  ch.control = status | (value & ~(AUDIO_CTRL_STATUS_W1C | AUDIO_CTRL_IDLE | AUDIO_CTRL_RESET));

  if (value & AUDIO_CTRL_RESET) {
    AUDIOCTRLR_DEBUGP("{} reset", chIdx == CH_I2S ? "I2S" : "SPDIF");
    ch.hwSlot = 0;
    ch.stopSlot = 0;
    ch.halted = false;
    ch.inSlot = false;
    ch.control &= ~AUDIO_CTRL_STATUS_W1C;
  }

  const bool running = (ch.control & AUDIO_CTRL_RUN) != 0;
  if (running && !wasRunning) {
    AUDIOCTRLR_DEBUGP("{} run, ring {:#x}", chIdx == CH_I2S ? "I2S" : "SPDIF", ch.ringBase);
    ch.control &= ~AUDIO_CTRL_IDLE;
    ch.halted = false;
    ch.inSlot = false;
  } else if (!running) {
    // Stop at once: the slot in progress is abandoned and the engine reads idle.
    ch.control |= AUDIO_CTRL_IDLE;
    ch.inSlot = false;
    ch.halted = false;
  }
  // The ISR acks by writing back the status it read. A bit the engine set after that read (the stop slot's
  // HALTED|UNDERRUN racing a DESC_DONE) survives the ack, so a real level line stays high and the IIC takes it again
  // after EOI. Only edges are forwarded here, so re-raise it explicitly: otherwise the ISR never runs again and the
  // render thread spins submitting a packet on the slot it's waiting for starving its CPU.
  UpdateInterruptLine(true);
}

// Caller holds channelMutex.
void Xe::PCIDev::AUDIOCTRLR::HandleStatusWrite(int chIdx, u32 value) {
  AudioDMAChannel &ch = channels[chIdx];
  // Only the stop slot is writable. The ISR writes the HW slot it read back into [4:0], which is ignored.
  ch.stopSlot = (value & AUDIO_STATUS_STOP_SLOT_MASK) >> AUDIO_STATUS_STOP_SLOT_SHIFT;
  ch.statusRW = value;
  // Parked on the old stop slot: resume on that slot once the driver moves the stop slot. The ISR has just
  // refilled it with silence and treats the engine as sitting at the slot it read.
  if (ch.halted && ch.stopSlot != ch.hwSlot) {
    AUDIOCTRLR_DEBUGP("{} resume at slot {}, stop {}", chIdx == CH_I2S ? "I2S" : "SPDIF", ch.hwSlot, ch.stopSlot);
    ch.halted = false;
  }
}

// The console's reset line: both DMA engines stop, the slot in progress is abandoned, registers and the interrupt line
// go back to idle, and what's still queued for the host device is dropped so nothing plays on after the reset. The
// SMC's audio enable is the guest's to set again.
void Xe::PCIDev::AUDIOCTRLR::PlatformReset() {
  {
    std::lock_guard<std::mutex> lock(channelMutex);
    channels[CH_I2S] = {};
    channels[CH_SPDIF] = {};
    config = 0;
    spdifChannelStatus = 0;
    audioOutEnabled.store(false);
    UpdateInterruptLine();
#ifdef GFX_ENABLED
    if (sdlAudioStream) { SDL_ClearAudioStream(sdlAudioStream); }
#endif
  }
  dmaCV.notify_all();
}

void Xe::PCIDev::AUDIOCTRLR::SetAudioOutputEnabled(bool enabled) {
  audioOutEnabled.store(enabled);
  LOG_INFO(AudioController, "Audio output {}", enabled ? "enabled" : "disabled");
}

void Xe::PCIDev::AUDIOCTRLR::ApplyHostSettings() {
#ifdef GFX_ENABLED
  // The DMA thread feeds the stream with channelMutex held.
  std::lock_guard<std::mutex> lock(channelMutex);
  if (Config::audio.device != sdlDeviceName) {
    ShutdownSDLAudio();
    InitSDLAudio();
  } else if (sdlAudioStream) {
    SDL_SetAudioStreamGain(sdlAudioStream, HostGain());
  }
#endif
}

std::vector<std::string> Xe::PCIDev::AUDIOCTRLR::HostDevices() {
  std::vector<std::string> names;
#ifdef GFX_ENABLED
  if (!SDL_WasInit(SDL_INIT_AUDIO)) { return names; }
  int count = 0;
  SDL_AudioDeviceID *ids = SDL_GetAudioPlaybackDevices(&count);
  for (int i = 0; ids && i < count; ++i) {
    if (const char *name = SDL_GetAudioDeviceName(ids[i])) { names.emplace_back(name); }
  }
  SDL_free(ids);
#endif
  return names;
}
