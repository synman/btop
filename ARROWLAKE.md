# btop 1.4.7+arrowlake

This branch is upstream btop v1.4.7 plus a series of code commits, and this notes file. It makes the Intel
Arrow Lake integrated GPU show up properly, and it fixes the layout when two GPUs are shown.
Everything else is upstream. Other i915 GPUs take the same code paths, but only Arrow Lake was tested.
For btop itself, read [README.md](README.md).

The version string reads `1.4.7+arrowlake.14`. The `+arrowlake.N` part is semver build metadata.
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
| NPU row | Not shown | `NPU` meter with clock against maximum, from the `intel_vpu` sysfs counters. `NPU mem:` sits in the empty slot under `Total:`. Both read `offline` while the driver reloads |
| MEM row | Not shown | `MEM` meter of the GPU's own memory traffic (TX writes plus RX reads) against 70.4 GB/s, the ceiling of the two DDR5-4400 channels on the Ultra. It reads `n/a` while no OA stream is open. It does not include CPU or NPU traffic |
| Refresh interval | `- 2000ms +` is static | The `2000ms` text swaps foreground and background on every update, so it flashes once per interval and shows the display is still updating |
| `+` / `-` keys | With the process tree on, they always expand or collapse a tree node, so the refresh interval keys did nothing | They expand or collapse only while a process is selected. With none selected they change the refresh interval, as the `- 2000ms +` buttons do |

Intel xe GPUs (discrete Arc, Battlemage), in `src/linux/btop_collect.cpp`. Each Intel card is found by PCI slot and
gets its own GPU box after the i915 one. The vendored `src/linux/intel_gpu_top/` files are untouched, because they only know i915.

| Panel item | This branch, for an xe device |
|---|---|
| GPU name | `Intel Arc Pro B70` for id `8086:e223`, else the igt name table, else `Intel Battlemage (Gen20)` or `Intel GPU (xe)` |
| GPU % | The busiest engine of any class, compute (ccs) included, from the xe PMU `engine-active-ticks` over `engine-total-ticks` |
| ENC / DEC | The busiest video decode engine (class 2); the video engines do both jobs, as on the iGPU |
| Clock | `tile0/gt0/freq0/act_freq` in sysfs |
| Memory | VRAM used and total from the xe device query on the render node |
| Power | Card energy counter `energy1_input` in hwmon, as a change over time; the scale is `power1_cap` |
| Temperature | The `pkg` sensor in hwmon, scaled to its `crit` limit |
| FAN, VRAM temperature | One `FAN` row: the fan speed in rpm, then the `VRAM` temperature graph and degrees in the same cells as the GPU row's temperature graph. A sensor with no reading leaves its field blank. `fan1_input` sometimes fails with `EAGAIN` or reads 0 between real values, so one such sample after a real value is held over; a second in a row is shown, so a stopped fan reads 0 one sample late. One blank row under the stats keeps the box as tall as the iGPU and RTX boxes |
| TX / RX, MEM, NPU | Not shown, there is no source for them on xe |

Layout, in `src/btop_draw.cpp`:

- GPU boxes sit side by side when every box keeps a full 41-column stats box, its two borders and a graph of at
  least 8 cells, which is 51 columns per box. At 192 columns that holds for two and for three boxes. Each box then
  takes one row of height instead of one row per GPU. The stats box stays 41 columns and the graph takes the
  remainder (21 cells for three boxes at 192). Two boxes on a terminal 102 to 163 columns wide now share a row too,
  where the old rule asked for 82 columns each and stacked them.
- The minimum terminal height counts boxes in one row once, by their tallest, not as a stack, so three GPU boxes
  no longer report "Terminal size too small" at 192x49. A terminal under 153 columns with three boxes still stacks
  them and needs 60 rows.
- A GPU name that does not fit beside the clock is cut at the last word that fits, not mid-word.
- For three boxes in one row add `gpu2` (the B70) to `shown_boxes`. Without it the B70 gets a summary line in the CPU
  box instead, and that box and every box below it move down one row.
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
  `cap_perfmon` and `kernel.perf_event_paranoid` set to 2 or lower. Debian 13 ships 3. When
  the level is too high, the load reads 0% and btop's debug log says
  `Intel GPU: Failed to initialize PMU`. To set the level and keep it across reboots:

  ```sh
  echo kernel.perf_event_paranoid=2 | sudo tee /etc/sysctl.d/60-perf-event-paranoid.conf
  sudo sysctl --system
  ```
- **Power.** The RAPL `energy_uj` file can only be read by root. `cap_dac_read_search` covers it.
- **Memory bandwidth.** The TX/RX row needs `libi915_perf.so.1.5` from `intel-gpu-tools` 2.0.
  On Debian 13, install it with `sudo apt install intel-gpu-tools`. btop loads it with `dlopen`
  while running, so btop still works without it. You also need `dev.i915.perf_stream_paranoid`
  set to 1, its default. The row stays hidden while any of these is missing:
  - the library;
  - the `ComputeBasic` metric set, on a GPU that lacks it;
  - an openable DRM node;
  - a free OA stream.
  - Only one OA stream can run per GPU. A second btop, or `i915-perf-recorder`, waits and retries
    every 10 s, and its row stays hidden until then.
  - btop tries the render node first. If the user cannot open it, as in an ssh or kiosk session
    outside the `render` group, btop tries the card node through `video`. Run `id` to see which
    of the two groups the user is in. With neither group, run btop as root or add the user to
    `video`.
  - To find out why the row is hidden, run btop with `--debug` and read the
    `Intel GPU:` lines in its log.
- **Memory used.** This total counts only the processes whose `/proc/<pid>/fdinfo` btop can read.
  The kernel allows that read only where btop could ptrace the process. So a GPU workload run by
  another user, such as a container's own uid, reads as 0. To see all of it, use one of these:
  - Run btop as the user that owns the GPU workload, or as root.
  - Grant btop `cap_sys_ptrace` beside the other two capabilities:

    ```sh
    sudo setcap "cap_perfmon,cap_dac_read_search,cap_sys_ptrace=+ep" /usr/local/bin/btop
    ```

    `make setcap` does not add it, because it lets btop attach to any process. Grant it only where
    you trust the btop binary and its users, such as a single-user kiosk.
- **Intel xe.** The xe PMU needs `cap_perfmon` and `kernel.perf_event_paranoid` of 2 or lower, the same as i915.
  The VRAM figures need an openable render node (group `render`) or card node (group `video`). hwmon and sysfs
  need nothing. An open PMU event and the hwmon reads keep a runtime-suspended card awake while btop runs. A card
  that stops answering reads shows as offline and is probed again every 5 s; a card that was absent when btop
  started is not picked up later.
- **NPU.** The NPU row and `NPU mem:` need the `intel_vpu` driver, which exposes
  `npu_busy_time_us`, `npu_current_frequency_mhz`, `npu_max_frequency_mhz` and
  `npu_memory_utilization` under `/sys/bus/pci/devices/<address>/`. Normal users can read them,
  so no capability is needed. The busy time is cumulative, so btop shows its change over time.
  Without those files the row is not drawn. While the driver reloads they vanish for about 3 s,
  and the row reads `offline` until they return.
- **Temperature.** Not shown, because Linux exposes no i915 temperature sensor for this iGPU.

## Tested on

- Minisforum MS-02 Ultra with an Intel Core Ultra 9 285HX. The iGPU is Arrow Lake-S `8086:7d67`.
- A second GPU: NVIDIA RTX PRO 4000 Blackwell SFF.
- Debian 13, kernel 7.2.9, Mesa 25.0, `intel-gpu-tools` 2.0.
- Shown full screen on the Linux console under `kmscon` at 1920x1080. That gives a 192x49 terminal.

## Keeping it current

The code change is eight commits on top of the v1.4.7 tag: Arrow Lake support, the NPU row, the MEM row, the refresh-interval flash, the `+`/`-` key fix, the Intel xe GPU box, three GPU boxes in one row with the xe FAN row, then the fan read hold.
This file and the README banner come in later commits that touch no code. To move to a newer
upstream release, rebase the branch onto the new tag and rebuild. Check two things after a rebase:

- whether upstream added an Arrow Lake id table entry of its own;
- whether the layout code in `btop_draw.cpp` moved.

When the change set or a requirement changes, update this file.
