// The facts the picture's helpers depend on, read from Windows (cheap, no disk writes).
#pragma once
#include "../core/device_tier.h"

// running on battery (a laptop unplugged): the helpers that spend power stay off
bool on_battery();
// the adapter the picture is drawn on (the first one) is NVIDIA's
bool primary_gpu_nvidia();
// what this PC can comfortably do (core/device_tier.h): the first adapter's kind (the one the
// engine draws on: Windows lists the person's preferred graphics first) and the processor's
// threads. Read once, a few milliseconds; thread-safe.
DeviceTier read_device_tier();
// NVIDIA's own switch for RTX Video Super Resolution (NVIDIA app / control panel): 1 on, 0 off,
// -1 unknown (not found: then it is treated as off, so the picture never gets worse)
int nvidia_vsr_switch();

// While a file plays, Windows must not slow Cue down when its window is not in front (a mini
// player beside other work, a film on a second screen): on recent Windows a process whose
// window is not the foreground one can be moved to efficiency cores at low clocks (EcoQoS),
// which a 4K film's decoding and the sound's queue can hear. Back to Windows' choice at home.
void playback_priority(bool playing);
