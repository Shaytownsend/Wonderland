# BLDS Player

A macOS AU/VST3 plugin for Ableton Live 12 that plays DirectOut's **BLDS™**
redundancy trigger signal **bit-exactly**, looped with no gaps, at whatever
sample rate the Live set runs.

The plugin does not synthesize or approximate BLDS. It only plays the official
files made with DirectOut **BLDS Generator v1.1**, embedded into the plugin at
build time. Output samples equal the file's samples exactly: no resampling,
interpolation, dither, fades, smoothing, normalisation or gain.

| Build variant      | Type                        | AU code             | Use                       |
|--------------------|-----------------------------|---------------------|---------------------------|
| **BLDS Player**    | Instrument, 0 in / 2 out    | `aumu Blds Lnce`    | Normal use, on a MIDI track |
| **BLDS Player FX** | Audio effect, 2 in / 2 out (input ignored) | `aufx Blde Lnce` | For hosts or tracks that need an effect |

## 1. Generate the six BLDS files

1. Get **BLDS Generator v1.1** from DirectOut and open it.
2. Make one file at each of these sample rates: **44.1, 48, 88.2, 96, 176.4 and 192 kHz**.
   Leave the generator's own output format alone (24-bit mono WAV). Don't
   open, trim, normalise or re-export the files in any other program.
3. Rename them exactly like this and put them in `blds_files/` next to `CMakeLists.txt`:

   ```
   blds_files/BLDS_44100.wav
   blds_files/BLDS_48000.wav
   blds_files/BLDS_88200.wav
   blds_files/BLDS_96000.wav
   blds_files/BLDS_176400.wav
   blds_files/BLDS_192000.wav
   ```

You can leave a rate out; the plugin then shows **"No BLDS file for <rate> Hz"**
and outputs digital silence at that rate. Every file is checked when the plugin
loads: it must be RIFF/WAVE, integer PCM, 24-bit, mono, longer than 0 samples, and
its header sample rate must match the rate in its filename. A file that fails is
rejected, and the UI shows the reason (for example `Invalid file: header says 44100 Hz but filename says 48000 Hz`).
The bit-exact test also fails the build if an embedded file is invalid.

## 2. Build and install

Requirements: macOS 12+, Xcode command line tools, CMake 3.22+, internet access
for the first build (fetches JUCE 8.0.15 and pluginval).

```sh
./build_and_install.sh                 # full run (10-minute test renders, pluginval level 10)
./build_and_install.sh --skip-pluginval
./build_and_install.sh --test-seconds 60   # shorter test renders
```

The script:

1. Configures and builds Release as a universal binary (arm64 + x86_64).
2. Runs `BLDSPlayerTests` and `BLDSPlayerFXTests`, and stops if either fails.
3. Checks that every bundle is universal, then ad-hoc codesigns them.
4. Copies them to `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3`.
5. Runs `auval -v aumu Blds Lnce` and `auval -v aufx Blde Lnce`.
6. Runs pluginval at strictness level 10 on all four bundles.

Re-run the script after you add or replace files in `blds_files/`. The files are compiled into the binary.

### What the tests check

For every embedded rate, through the real plugin processor, with no host:

- 10 minutes of audio with random block sizes (1–4096, fixed seed), then 10
  minutes each at block sizes 32, 64, 128, 256, 512, 1024 and 2048, plus 1-sample
  blocks. Every output sample's float bit pattern must equal the source sample,
  across every loop boundary.
- The unused channel must be exactly `0.0f`. Left-only and Both modes are also checked.
- A global `operator new` hook fails the test if `processBlock` allocates anything.
- On one plugin instance, the sample rate changes repeatedly mid-run, including to an
  unsupported rate (silence plus the error flag) and back. After each change,
  output must restart at sample 0 of the new file and stay bit-exact.
- Bypass gives exact silence, and the loop keeps running underneath.
- The bytes embedded in the binary are compared with the files on disk, and the
  decoded samples are checked against a separate reference decoder.
- Every 24-bit value (2^24 of them) converts to float exactly. WAV parser accept/reject cases.
- Random 24-bit fixtures at all six rates. These are test data, not BLDS, and
  are never embedded. They exercise the engine even when no official files are present.

The tests exit non-zero on any failure and finish with a PASS/FAIL table.

## 3. Ableton Live 12 setup (do this identically on both Macs)

1. Create a new **MIDI track** and put **BLDS Player** (Plug-Ins › Lnce) on it.
   It needs no MIDI clip and no input, and plays whether the transport is running or not.
2. In the plugin, set **Output Channel = Right only**. The status line must be
   green: **"BLDS OK — <rate> Hz"**.
3. Set the track's **Audio To** to your interface, output pair **"31/32"**. The
   right channel then lands on hardware channel **32** untouched, with no
   stereo-to-mono summing.
4. Set track volume to exactly **0.0 dB** and pan to **centre**. The mixer then
   passes the signal at unity gain.
5. Put **no other devices** on this track. Don't add Utility, a limiter on the
   Main track, etc. in this signal's path.
6. Make sure **nothing else** is routed to output 32: no other tracks, sends,
   cue/preview output, or metronome.
7. Make the set's sample rate match one you generated (Live › Settings › Audio).
   The plugin follows Live's rate automatically.

Both machines need the same plugin build, the same files, and the same routing.

## 4. Confirm detection in DirectOut GlobCon

1. Open **GlobCon** and select the device that receives channel 32 from each Mac.
2. Go to the **Show** page. In the **Trigger** column, the BLDS trigger for the
   input carrying channel 32 should show as **detected/active** for each Mac's feed.
3. Test the switchover: stop BLDS on one Mac by bypassing or removing the plugin.
   GlobCon should report BLDS lost on that input and act as configured.

> If BLDS is still not detected while **FastSRC** is active on the Dante port, that is DirectOut/ACE behaviour, not a plugin fault. Test with FastSRC off to confirm.

## How it works

- **Build time:** CMake embeds each `.wav` byte-for-byte using the assembler's
  `.incbin` directive. No converter tool, so nothing can alter the data.
- **Load time (never on the audio thread):** each embedded file is decoded and
  validated. Each 24-bit sample `s` becomes `float(s) / 8388608.0f`, which is
  exact because every 24-bit integer fits in float32. Tables are allocated once
  and live as long as the plugin.
- **prepareToPlay:** the host rate is rounded to the nearest Hz and matched
  exactly against 44100…192000. The chosen table is published through a
  `std::atomic` pointer. A table change restarts playback at sample 0.
- **processBlock:** has no allocation, locks, I/O, logging, strings, exceptions or
  system calls. It keeps a 64-bit position and wraps it exactly at the file length.
  It ignores the transport and playhead, so playback runs free.
  `reset()` deliberately doesn't move the loop.
- Latency is 0 and never changes. Tail is infinite, so hosts never suspend the plugin.
- Host bypass outputs exact silence and never passes the input through.
- The UI reads atomics at 4 Hz and repaints only when something changes.

## Files

```
CMakeLists.txt            JUCE via FetchContent (8.0.15), both variants, tests
build_and_install.sh      build → test → sign → install → auval → pluginval
blds_files/               put the six official BLDS_<rate>.wav files here
Source/BldsWav.*          strict 24-bit mono PCM WAV decoder
Source/BldsEmbedded.h     access to the files embedded verbatim with .incbin
Source/BldsLibrary.*      six rate slots, decoded from the embedded files at construction
Source/PluginProcessor.*  real-time player, Output Channel parameter, state
Source/PluginEditor.*     fixed-size status UI
Tests/BldsTests.cpp       command-line bit-exact verification
```
