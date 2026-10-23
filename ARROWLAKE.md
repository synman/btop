# btop 1.4.7+arrowlake

This branch is upstream btop v1.4.7 plus one commit. It makes the Intel Arrow Lake integrated GPU
show up properly, and it fixes the layout when two GPUs are shown. Everything else is upstream.
For btop itself, read [README.md](README.md).

The version string reads `1.4.7+arrowlake.7`. The `+arrowlake.N` part is semver build metadata.
It counts our builds and does not change upstream's version.

## What it changes

Intel GPU, in `src/linux/`:

| Panel item | Upstream 1.4.7 on Arrow Lake | This branch |
|---|---|---|
| GPU name | Arrow Lake ids sit in the Meteor Lake table | Its own `arrowlake` device entry |
| Power | Blank, because the perf `energy-gpu` event is missing | Read from the powercap RAPL `uncore` domain |
| Memory used | Not shown | Each i915 client's `drm-resident` memory from `/proc/<pid>/fdinfo`, counting a shared client once |
| Memory total | Not shown | Half of system RAM, the heap Mesa reports for the iGPU |
| ENC / DEC | Not shown | Load of the video engine, which does both jobs |
| TX / RX row | Not shown, since an iGPU has no PCIe link | GPU memory write and read bandwidth from the i915 OA unit |

Layout, in `src/btop_draw.cpp`:

- When every box would still be at least twice the minimum width, GPU boxes sit side by side.
  They then take one row of height instead of one row per GPU.
- The CPU box grows until every core fits. Before this, a 192x49 screen with two GPUs showed 18
  of 24 cores. The boxes below give up the rows but never shrink under their minimums.
- Smaller fixes: the rows each core column can hold, the x position of the GPU graph, and the
  position of the clock in the title.

## Build

Build it the same way as upstream, with GPU support on:

```sh
git clone -b arrowlake https://github.com/synman/btop.git
cd btop
make GPU_SUPPORT=true
sudo make install
sudo make setcap
```

`make setcap` grants `cap_perfmon` and `cap_dac_read_search`, which the Intel panel needs. Our
build ran on Debian 13 with `build-essential` (g++ 14) in a `debian:trixie-slim` container.

## Runtime requirements on Linux

- **Kernel perf access.** Intel GPU load comes from the i915 PMU. Without root, that needs
  `cap_perfmon` and `kernel.perf_event_paranoid` set to 2 or lower. Debian ships 3.
- **Power.** The RAPL `energy_uj` file can only be read by root. `cap_dac_read_search` covers it.
- **Memory bandwidth.** The TX/RX row needs `libi915_perf.so.1.5` from `intel-gpu-tools` 2.0.
  btop loads it with `dlopen` while running, so btop still works without it. You also need
  `dev.i915.perf_stream_paranoid` set to 1, its default.
  - Only one OA stream can run per GPU. A second btop, or `i915-perf-recorder`, waits and retries
    every 10 s, and its row stays hidden until then.
  - btop tries the render node first. If the user cannot open it, as in an ssh or kiosk session
    outside the `render` group, btop tries the card node through `video`.
- **Memory used.** This total counts only the processes whose `/proc/<pid>/fdinfo` btop can read.
  Run btop as the user that owns the GPU workload, or as root, to see all of it.
- **Temperature.** Not shown, because Linux exposes no i915 temperature sensor for this iGPU.

## Tested on

- Minisforum MS-02 Ultra with an Intel Core Ultra 9 285HX. The iGPU is Arrow Lake-S `8086:7d67`.
- A second GPU: NVIDIA RTX PRO 4000 Blackwell SFF.
- Debian 13, kernel 7.2.9, Mesa 25.0, `intel-gpu-tools` 2.0.
- Shown full screen on the Linux console under `kmscon` at 1920x1080. That gives a 192x49 terminal.

## Keeping it current

The code change is one commit on top of the v1.4.7 tag. This file is a second commit. To move
to a newer upstream release, rebase the branch onto the new tag and rebuild. Check two things after a rebase:

- whether upstream added an Arrow Lake id table entry of its own;
- whether the layout code in `btop_draw.cpp` moved.

When the change set or a requirement changes, update this file.
