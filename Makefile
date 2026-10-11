.PHONY: build build-native native-tests acceptance-build check test test-native test-import auction-check auction-check-all auction-gpu-check

PYTHON ?= python3
-include .bend2-local.mk

build:
	$(PYTHON) dev/bend2-build.py --target tests --backend javascript
	$(PYTHON) dev/bend2-build.py --target production --backend native

build-native:
	$(PYTHON) dev/bend2-build.py --backend native

native-tests:
	$(PYTHON) -P dev/bend2-native-tests.py

acceptance-build: build
	$(MAKE) native-tests

check:
	$(PYTHON) dev/bend2-build.py --check

test: test-native

test-native: acceptance-build
	BEND_TEST_BACKEND=native BEND_NATIVE_TEST_ROOT="$(CURDIR)" $(PYTHON) dev/bend2-test.py --no-build

test-import:
	$(PYTHON) dev/bend2-test.py --suite import

.PHONY: prelude-compatibility-test
prelude-compatibility-test:
	$(PYTHON) dev/bend2-build.py --target production --backend native
	$(PYTHON) -P dev/prelude-compatibility-gates.py

.PHONY: prelude-category-compatibility-test
prelude-category-compatibility-test:
	$(PYTHON) dev/bend2-build.py --target tests --backend javascript
	$(PYTHON) dev/bend2-build.py --target production --backend native
	$(PYTHON) -P dev/prelude-category-compatibility.py --out "$$(mktemp -d)/evidence"

.PHONY: prelude-functor-compatibility-test
prelude-functor-compatibility-test:
	$(PYTHON) dev/bend2-build.py --target tests --backend javascript
	$(PYTHON) dev/bend2-build.py --target production --backend native
	$(PYTHON) -P dev/prelude-functor-compatibility.py --out "$$(mktemp -d)/evidence"

.PHONY: prelude-nattrans-compatibility-test
prelude-nattrans-compatibility-test:
	$(PYTHON) dev/bend2-build.py --target tests --backend javascript
	$(PYTHON) dev/bend2-build.py --target production --backend native
	$(PYTHON) -P dev/prelude-nattrans-compatibility.py --out "$$(mktemp -d)/evidence"

LEFT_KAN_TIMEOUT ?= 900
.PHONY: prelude-left-kan-compatibility-test
prelude-left-kan-compatibility-test:
	$(PYTHON) dev/bend2-build.py --target tests --backend javascript
	$(PYTHON) dev/bend2-build.py --target production --backend native
	$(PYTHON) -P dev/prelude-left-kan-compatibility.py --out "$$(mktemp -d)/evidence" --timeout $(LEFT_KAN_TIMEOUT)

.PHONY: reference-check reference-test require-ocaml-reference

reference-check: bend2-reference-check

reference-test: bend2-reference-test

require-ocaml-reference:
	@test -n "$(strip $(REFERENCE))" || { printf '%s\n' 'Set REFERENCE to an OCaml checkout for live comparisons.' >&2; exit 2; }

auction-check: build
	$(PYTHON) -P dev/gpu-auction-check.py

auction-check-all: build
	$(PYTHON) -P dev/gpu-auction-check.py --category --schemas

auction-gpu-check: build
	$(PYTHON) -P dev/gpu-auction-check.py --gpu

.PHONY: bend2-build bend2-check bend2-test

-include .bend2-local.mk

bend2-build:
	$(PYTHON) dev/bend2-build.py --target tests --backend javascript
	$(PYTHON) dev/bend2-build.py --target production --backend native

bend2-check:
	$(PYTHON) dev/bend2-build.py --check

bend2-test:
	$(PYTHON) dev/bend2-test.py --suite extras

# Live OCaml comparison and evidence: dev/BEND2-REFERENCE.md
.PHONY: bend2-reference-check bend2-reference-test bend2-diff

bend2-reference-check: require-ocaml-reference
	$(PYTHON) -P dev/bend2-reference-check.py --reference "$(REFERENCE)" $(BEND2_REFERENCE_ARGS)

bend2-reference-test: require-ocaml-reference
	$(PYTHON) -P dev/bend2-reference-test.py --reference "$(REFERENCE)"
	$(PYTHON) -P dev/bend2-equality-runtime-test.py --reference "$(REFERENCE)"
	$(PYTHON) -P dev/bend2-composition-runtime-test.py --reference "$(REFERENCE)"
	$(PYTHON) -P dev/bend2-reuse-runtime-test.py --reference "$(REFERENCE)"

bend2-diff: bend2-reference-check bend2-build
	$(PYTHON) -P dev/bend2-json-check.py
	$(PYTHON) -P dev/bend2-export-check.py
	$(PYTHON) -P dev/bend2-translate-check.py
	$(PYTHON) -P dev/bend2-pipeline-check.py
	$(PYTHON) -P dev/bend2-parser-check.py
	$(PYTHON) -P dev/bend2-surface-check.py --driver _bend2/test/surface_check_driver.exe
	$(PYTHON) -P dev/bend2-erase-check.py
	$(PYTHON) -P dev/bend2-cli-check.py
	$(PYTHON) -P bend2/tests/cli_core_check.py
	$(PYTHON) -P dev/bend2-equality-runtime-check.py
	$(PYTHON) -P dev/bend2-composition-runtime-check.py
	$(PYTHON) -P dev/bend2-reuse-runtime-check.py

# C port compiled with TinyCC: sources under c/, outputs under _tcc/ (see MIGRATION notes in W/PLAN.md)
TCC ?= tcc
C_RT_SOURCES := $(wildcard c/rt/*.c)
C_RT_HEADERS := $(wildcard c/rt/*.h)
C_KERNEL_SOURCES := $(wildcard c/kernel/*.c)
C_KERNEL_HEADERS := $(wildcard c/kernel/*.h)
# Kernel test drivers: one binary per bend2/tests/<driver>.bend ported to c/tests/<driver>.c.
# Checks print PASS or die; probes print lines that c-test compares with c/tests/<driver>.expected.
C_KERNEL_CHECKS := kernel_budget_callback_check kernel_budget_callbacks kernel_budget_dag kernel_check kernel_conversion kernel_declarations kernel_eval kernel_global_lookup kernel_head_ty_invalid_level kernel_numbers kernel_projection_motive kernel_quantity levels_budget levels_checker levels_exact levels_regression runtime_slice surface_equality wasm_circuit wasm_order
# Checks that take the manifest arguments (dev/bend2/test-manifest.json) on the command line.
C_KERNEL_ARG_CHECKS := kernel_global_index kernel_usage_native
C_KERNEL_PROBES := kernel_quantity_native_probe kernel_term_native_probe kernel_pp_native_probe kernel_linear_check_probe kernel_spec_count
C_KERNEL_DRIVERS := $(C_KERNEL_CHECKS) $(C_KERNEL_ARG_CHECKS) $(C_KERNEL_PROBES)
C_TEST_HEADERS := $(wildcard c/tests/*.h)
C_KERNEL_BINARIES := $(addprefix _tcc/,$(C_KERNEL_DRIVERS))
C_KERNEL_FAULT_CHECKS := kernel_allocation_check kernel_lookup_allocation kernel_erasure_allocation kernel_order_allocation
C_KERNEL_FAULT_BINARIES := $(addprefix _tcc/,$(C_KERNEL_FAULT_CHECKS))
.PHONY: c-build c-test
c-build: _tcc/rt_test _tcc/rt_regression_test _tcc/rt_oom_test $(C_KERNEL_BINARIES) $(C_KERNEL_FAULT_BINARIES) _tcc/runtime_reachable.o

# bend2/tests/runtime_reachable.bend is a library module with no main: c-build compiles its port and
# prelude_runtime (S5.1) links it and runs its self test.
_tcc/runtime_reachable.o: c/tests/runtime_reachable.c $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -c -o $@ $<

$(C_KERNEL_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -o $@ $< $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

_tcc/rt_test _tcc/rt_regression_test: _tcc/%: c/tests/%.c $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -o $@ $< $(C_RT_SOURCES) $(LDFLAGS) -lpthread

_tcc/arena_fault.o: c/rt/arena.c $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -Darena_alloc=rt_test_arena_alloc_real -c -o $@ $<

_tcc/rt_oom_test: c/tests/rt_oom_test.c $(C_RT_SOURCES) $(C_RT_HEADERS) _tcc/arena_fault.o
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -o $@ $< $(filter-out c/rt/arena.c,$(C_RT_SOURCES)) _tcc/arena_fault.o $(LDFLAGS) -lpthread

$(C_KERNEL_FAULT_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS) _tcc/arena_fault.o
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -o $@ $< $(C_KERNEL_SOURCES) $(filter-out c/rt/arena.c,$(C_RT_SOURCES)) _tcc/arena_fault.o $(LDFLAGS) -lpthread

c-test: c-build
	./_tcc/rt_test
	./_tcc/rt_regression_test
	./_tcc/rt_oom_test
	$(foreach d,$(C_KERNEL_FAULT_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_KERNEL_CHECKS),./_tcc/$(d) &&) true
	./_tcc/kernel_global_index "$$(printf 'case/A\303\251\360\237\230\200')" "$$(printf 'case/Ae\314\201\360\237\230\200')" "$$(printf 'case/a\303\251\360\237\230\200')"
	./_tcc/kernel_usage_native 0
	./_tcc/kernel_usage_native 7
	./_tcc/kernel_quantity_native_probe 0 > _tcc/kernel_quantity_native_probe.actual
	cmp _tcc/kernel_quantity_native_probe.actual c/tests/kernel_quantity_native_probe.expected
	@echo "PROBE-OK kernel_quantity_native_probe lines=24"
	./_tcc/kernel_term_native_probe > _tcc/kernel_term_native_probe.actual
	cmp _tcc/kernel_term_native_probe.actual c/tests/kernel_term_native_probe.expected
	@echo "PROBE-OK kernel_term_native_probe lines=75"
	./_tcc/kernel_pp_native_probe > _tcc/kernel_pp_native_probe.actual
	cmp _tcc/kernel_pp_native_probe.actual c/tests/kernel_pp_native_probe.expected
	@echo "PROBE-OK kernel_pp_native_probe lines=109"
	./_tcc/kernel_linear_check_probe > _tcc/kernel_linear_check_probe.actual
	cmp _tcc/kernel_linear_check_probe.actual c/tests/kernel_linear_check_probe.expected
	@echo "PROBE-OK kernel_linear_check_probe lines=1"
	./_tcc/kernel_spec_count > _tcc/kernel_spec_count.actual
	cmp _tcc/kernel_spec_count.actual c/tests/kernel_spec_count.expected
	@echo "PROBE-OK kernel_spec_count lines=9"
