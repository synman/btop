/*
 * Layout of the libi915_perf structures btop reads, copied from igt-gpu-tools 2.0
 * lib/i915/perf.h and lib/igt_list.h (MIT). btop loads libi915_perf.so.1.5 at runtime
 * with dlopen, so only the struct layouts and function signatures are needed here.
 * Update this file together with the igt-gpu-tools version it is checked against.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

struct igt_list_head {
	struct igt_list_head *prev;
	struct igt_list_head *next;
};

struct intel_perf_accumulator {
#define INTEL_PERF_MAX_RAW_OA_COUNTERS 64
	uint64_t deltas[INTEL_PERF_MAX_RAW_OA_COUNTERS];
};

struct intel_perf;
struct intel_perf_metric_set;

struct intel_perf_logical_counter {
	const struct intel_perf_metric_set *metric_set;
	const char *name;
	const char *symbol_name;
	const char *desc;
	const char *group;
	bool (*availability)(const struct intel_perf *perf);
	int storage;
	int type;
	int unit;
	union {
		uint64_t (*max_uint64)(const struct intel_perf *perf, const struct intel_perf_metric_set *metric_set, uint64_t *deltas);
		double (*max_float)(const struct intel_perf *perf, const struct intel_perf_metric_set *metric_set, uint64_t *deltas);
	};
	union {
		uint64_t (*read_uint64)(const struct intel_perf *perf, const struct intel_perf_metric_set *metric_set, uint64_t *deltas);
		double (*read_float)(const struct intel_perf *perf, const struct intel_perf_metric_set *metric_set, uint64_t *deltas);
	};
	struct igt_list_head link;
};

struct intel_perf_register_prog {
	uint32_t reg;
	uint32_t val;
};

struct intel_perf_metric_set {
	const char *name;
	const char *symbol_name;
	const char *hw_config_guid;

	struct intel_perf_logical_counter *counters;
	int n_counters;

	uint64_t perf_oa_metrics_set;
	int perf_oa_format;
	int perf_raw_size;

	int gpu_time_offset;
	int gpu_clock_offset;
	int a_offset;
	int b_offset;
	int c_offset;
	int perfcnt_offset;

	const struct intel_perf_register_prog *b_counter_regs;
	uint32_t n_b_counter_regs;

	const struct intel_perf_register_prog *mux_regs;
	uint32_t n_mux_regs;

	const struct intel_perf_register_prog *flex_regs;
	uint32_t n_flex_regs;

	struct igt_list_head link;
};

/* Only the leading members are read; devinfo follows and is never touched. */
struct intel_perf {
	const char *name;
	void *root_group;
	struct igt_list_head metric_sets;
};

struct drm_i915_perf_record_header;
