.PHONY: build build-native native-tests acceptance-build check test test-native test-import auction-check auction-check-all auction-gpu-check

PYTHON ?= python3

build:
	$(MAKE) --no-print-directory c-build

build-native:
	$(MAKE) --no-print-directory c-build

native-tests:
	$(MAKE) --no-print-directory c-build

acceptance-build: build
	$(MAKE) native-tests

check:
	$(MAKE) --no-print-directory c-build

test: test-native

test-native: acceptance-build
	$(PYTHON) dev/c-test.py --no-build

test-import:
	$(PYTHON) dev/c-test.py --suite import

.PHONY: prelude-compatibility-test
prelude-compatibility-test:
	$(MAKE) --no-print-directory c-build
	$(PYTHON) -P dev/prelude-compatibility-gates.py

.PHONY: prelude-category-compatibility-test
prelude-category-compatibility-test:
	$(MAKE) --no-print-directory c-build
	$(PYTHON) -P dev/prelude-category-compatibility.py --out "$$(mktemp -d)/evidence"

.PHONY: prelude-functor-compatibility-test
prelude-functor-compatibility-test:
	$(MAKE) --no-print-directory c-build
	$(PYTHON) -P dev/prelude-functor-compatibility.py --out "$$(mktemp -d)/evidence"

.PHONY: prelude-nattrans-compatibility-test
prelude-nattrans-compatibility-test:
	$(MAKE) --no-print-directory c-build
	$(PYTHON) -P dev/prelude-nattrans-compatibility.py --out "$$(mktemp -d)/evidence"

LEFT_KAN_TIMEOUT ?= 900
.PHONY: prelude-left-kan-compatibility-test
prelude-left-kan-compatibility-test:
	$(MAKE) --no-print-directory c-build
	$(PYTHON) -P dev/prelude-left-kan-compatibility.py --out "$$(mktemp -d)/evidence" --timeout $(LEFT_KAN_TIMEOUT)

.PHONY: reference-check reference-test require-ocaml-reference

reference-check: c-reference-check

reference-test: c-reference-test

require-ocaml-reference:
	@test -n "$(strip $(REFERENCE))" || { printf '%s\n' 'Set REFERENCE to an OCaml checkout for live comparisons.' >&2; exit 2; }

auction-check: build
	$(PYTHON) -P dev/gpu-auction-check.py

auction-check-all: build
	$(PYTHON) -P dev/gpu-auction-check.py --category --schemas

auction-gpu-check: build
	$(PYTHON) -P dev/gpu-auction-check.py --gpu

# Live OCaml comparison and evidence: dev/C-REFERENCE.md
.PHONY: c-reference-check c-reference-test c-diff

c-reference-check: require-ocaml-reference
	$(PYTHON) -P dev/c-reference-check.py --reference "$(REFERENCE)" $(BEND2_REFERENCE_ARGS)

c-reference-test: require-ocaml-reference
	$(PYTHON) -P dev/c-reference-test.py --reference "$(REFERENCE)"
	$(PYTHON) -P dev/c-equality-runtime-test.py --reference "$(REFERENCE)"
	$(PYTHON) -P dev/c-composition-runtime-test.py --reference "$(REFERENCE)"
	$(PYTHON) -P dev/c-reuse-runtime-test.py --reference "$(REFERENCE)"

c-diff: c-reference-check c-build _tcc/surface_check_driver
	$(PYTHON) -P dev/c-json-check.py
	$(PYTHON) -P dev/c-export-check.py
	$(PYTHON) -P dev/c-translate-check.py
	$(PYTHON) -P dev/c-pipeline-check.py
	$(PYTHON) -P dev/c-parser-check.py
	$(PYTHON) -P dev/c-surface-check.py --driver _tcc/surface_check_driver
	$(PYTHON) -P dev/c-erase-check.py
	$(PYTHON) -P dev/c-cli-check.py
	$(PYTHON) -P test/protocols/cli_core_check.py
	$(PYTHON) -P dev/c-equality-runtime-check.py
	$(PYTHON) -P dev/c-composition-runtime-check.py
	$(PYTHON) -P dev/c-reuse-runtime-check.py

# C port compiled with TinyCC: sources under c/, outputs under _tcc/ (see MIGRATION notes in W/PLAN.md)
TCC ?= tcc
C_RT_SOURCES := $(wildcard c/rt/*.c)
C_RT_HEADERS := $(wildcard c/rt/*.h)
C_KERNEL_SOURCES := $(wildcard c/kernel/*.c)
C_KERNEL_HEADERS := $(wildcard c/kernel/*.h)
C_SURFACE_SOURCES := $(wildcard c/surface/*.c)
C_SURFACE_HEADERS := $(wildcard c/surface/*.h)
C_PRELUDE_SOURCES := $(wildcard c/prelude/*.c)
C_PRELUDE_HEADERS := $(wildcard c/prelude/*.h)
C_IMPORT_SOURCES := $(wildcard c/import/*.c)
C_IMPORT_HEADERS := $(wildcard c/import/*.h)
C_WASM_SOURCES := $(wildcard c/wasm/*.c)
C_WASM_HEADERS := $(wildcard c/wasm/*.h)
C_RUST_SOURCES := $(wildcard c/rust/*.c)
C_RUST_HEADERS := $(wildcard c/rust/*.h)
C_RUST_OBJECTS := $(patsubst c/rust/%.c,_tcc/rust/%.o,$(C_RUST_SOURCES))
# Kernel test drivers: one binary per bend2/tests/<driver>.bend ported to c/tests/<driver>.c.
# Checks print PASS or die; probes print lines that c-test compares with c/tests/<driver>.expected.
C_KERNEL_CHECKS := kernel_budget_callback_check kernel_budget_callbacks kernel_budget_dag kernel_check kernel_conversion kernel_declarations kernel_eval kernel_global_lookup kernel_head_ty_invalid_level kernel_numbers kernel_projection_motive kernel_quantity levels_budget levels_checker levels_exact levels_regression runtime_slice surface_equality wasm_circuit wasm_order
# Checks that take the manifest arguments (dev/cases/test-manifest.json) on the command line.
C_KERNEL_ARG_CHECKS := kernel_global_index kernel_usage_native
C_KERNEL_PROBES := kernel_quantity_native_probe kernel_term_native_probe kernel_pp_native_probe kernel_linear_check_probe kernel_spec_count
C_KERNEL_DRIVERS := $(C_KERNEL_CHECKS) $(C_KERNEL_ARG_CHECKS) $(C_KERNEL_PROBES)
C_TEST_HEADERS := $(wildcard c/tests/*.h)
C_KERNEL_BINARIES := $(addprefix _tcc/,$(C_KERNEL_DRIVERS))
C_KERNEL_FAULT_CHECKS := kernel_allocation_check kernel_lookup_allocation kernel_erasure_allocation kernel_order_allocation
C_KERNEL_FAULT_BINARIES := $(addprefix _tcc/,$(C_KERNEL_FAULT_CHECKS))
# Surface test drivers: one binary per bend2/tests/<driver>.bend ported to c/tests/<driver>.c; each prints
# its TAG-OK line (the R.finish protocol of bend2/tests/relational_common.bend) or TAG-FAIL and exits 1.
C_SURFACE_CHECKS := surface_expectations surface_lexer surface_parser surface_poly surface_diagnostics surface_transform surface_family surface_catalog surface_boundary surface_elab surface_crypto kernel_recursive surface_program surface_trust surface_compact_swap surface_prelude surface_universes
C_SURFACE_BINARIES := $(addprefix _tcc/,$(C_SURFACE_CHECKS)) _tcc/surface_driver _tcc/surface_shape_metadata _tcc/surface_check_driver _tcc/kernel_erase_driver
# surface_boundary, surface_shape_metadata and surface_universes import surface_catalog as Test: its library object (MECH_DRIVER_LIBRARY
# drops main) links into them.
C_SURFACE_LIBS := _tcc/surface_catalog_lib.o _tcc/kernel_recursive_lib.o
C_LEVELS_CHECKS := levels_protocol kernel_large_addresses kernel_conversion_budget kernel_numeric_regressions kernel_computation_replay
C_LEVELS_BINARIES := $(addprefix _tcc/,$(C_LEVELS_CHECKS))
C_LEVELS_LIBS := _tcc/levels_regression_lib.o _tcc/levels_protocol_lib.o _tcc/kernel_projection_motive_lib.o _tcc/kernel_large_addresses_lib.o _tcc/kernel_conversion_budget_lib.o
# The relational support modules (library objects, rule below): defined before c-build so that its
# prerequisite list, which make expands when it reads the rule, sees them.
C_RELATIONAL_HEADERS := c/tests/relational_fs.h c/tests/relational_common.h c/tests/relational_text.h c/tests/relational_program.h c/tests/relational_digest.h c/tests/relational_negatives.h
C_RELATIONAL_LIBS := _tcc/relational_fs.o _tcc/relational_common.o _tcc/relational_text.o _tcc/relational_program.o _tcc/relational_digest.o _tcc/relational_negatives.o
C_PRELUDE_CHECKS := prelude_category prelude_functor prelude_heterogeneous_functor prelude_heterogeneous_nattrans prelude_heterogeneous_whiskering prelude_shared_nattrans prelude_nattrans_laws surface_family_poly prelude_left_kan prelude_composable_functors prelude_nattrans prelude_heterogeneous_left_kan prelude_left_kan_laws prelude_horizontal_associativity prelude_horizontal_laws prelude_iterated_whiskering prelude_whiskering_laws prelude_shared_left_kan prelude_nattrans_units prelude_left_kan_roundtrip prelude_left_kan_cocone_action prelude_left_kan_mediator_equality prelude_left_kan_solution_transport
C_PRELUDE_BINARIES := $(addprefix _tcc/,$(C_PRELUDE_CHECKS))
# memcap (c/tests/memcap.c) reads the footprint of the child through libproc, so the Makefile builds it on
# Darwin only. MEMCAP defaults to it there and to nothing elsewhere: an empty MEMCAP runs the capped drivers
# directly. A wrapper with the same argv (capMB seconds cmd args) can replace it: `make MEMCAP=<path> c-test`.
C_MEMCAP_BINARY := $(if $(filter Darwin,$(shell uname -s)),_tcc/memcap)
MEMCAP ?= $(if $(C_MEMCAP_BINARY),./$(C_MEMCAP_BINARY))
MEMCAP_MB ?= 3500
MEMCAP_SECS ?= 120
# $(call capped,SECS): the `MEMCAP MEMCAP_MB SECS` prefix of a capped driver run, or nothing when MEMCAP is empty.
capped = $(if $(strip $(MEMCAP)),$(MEMCAP) $(MEMCAP_MB) $(1))
# Import test drivers (S3): one binary per bend2/tests/<driver>.bend ported to c/tests/<driver>.c. The CLI
# drivers (import_json takes one JSON argument) are replayed by the oracle gate; c-test runs the checks only.
C_IMPORT_CHECKS := import_types_protocol mapping_protocol import_regression
C_IMPORT_CLI := import_json import_export import_translate import_pipeline
C_IMPORT_BINARIES := $(addprefix _tcc/,$(C_IMPORT_CHECKS) $(C_IMPORT_CLI))
# The wasm CLI drivers print encoded modules as byte lists that the oracle gate replays; c-test runs the wasm checks.
C_WASM_CHECKS := wasm_link
C_WASM_CLI := wasm_encode wasm_layout wasm_nat_runtime wasm_emit wasm_reactor wasm_wide_metadata wasm_oracle
C_WASM_BINARIES := $(addprefix _tcc/,$(C_WASM_CHECKS) $(C_WASM_CLI))
# CLI component (S5): c/cli ports bend2/host_fs.bend, bend2/io.bend and bend2/cli (cert.c links c/surface). io_driver runs under
# dev/c-io-check.py in c-test (its oracle calls ran in a temporary directory that no longer exists).
# kanon.c (bend2/cli/kanon.bend) needs c/wasm, so it is kept out of C_CLI_SOURCES and linked only by
# the kanon drivers (C_KANON_CLI). The production programs (C_CLI_MAINS: bend2/mech_cert.bend) hold main,
# so they are kept out too and each links only into its own _tcc/bin/ binary.
C_CLI_KANON := c/cli/kanon.c
C_CLI_MAINS := c/cli/mech_cert.c c/cli/mech_main.c c/cli/main.c
# mech.c (bend2/cli/mech.bend) needs c/import, c/wasm (kanon.c) and c/rust (rust_out.c, rust_in.c), so it
# is kept out of C_CLI_SOURCES too and linked only into _tcc/bin/mech and _tcc/bin/main (C_MECH_BINARIES).
C_CLI_MECH := c/cli/mech.c
# rust_out.c and rust_in.c (bend2/cli/rust_out.bend, rust_in.bend) need c/rust, so they are kept out of
# C_CLI_SOURCES and compiled to their own objects (C_CLI_RUST_OBJECTS); the S6 drivers and the mech
# binary link them with C_RUST_OBJECTS.
C_CLI_RUST := c/cli/rust_out.c c/cli/rust_in.c
C_CLI_RUST_OBJECTS := $(patsubst c/cli/%.c,_tcc/cli/%.o,$(C_CLI_RUST))
C_CLI_SOURCES := $(filter-out $(C_CLI_KANON) $(C_CLI_MAINS) $(C_CLI_MECH) $(C_CLI_RUST),$(wildcard c/cli/*.c))
C_CLI_HEADERS := $(wildcard c/cli/*.h)
# CLI drivers over c/cli + surface + kernel. prelude_protocol (suite mode elaborates the whole prelude)
# is built by c-build; its suite and --audit runs are gated outside c-test. template_cost (live JS oracle
# W/oracle/template-cost) is built by c-build and run by c-test on prelude/cat/category.mech.
# wasm_circuit_bounds (Out.cli_exit from c/cli) is run by c-test (CIRCUIT-BOUNDS 18/18).
C_IO_CLI := io_driver prelude_protocol template_cost wasm_circuit_bounds
C_IO_BINARIES := $(addprefix _tcc/,$(C_IO_CLI))
C_CLI_FAULT_CHECKS := cli_allocation
C_CLI_FAULT_BINARIES := $(addprefix _tcc/,$(C_CLI_FAULT_CHECKS))
$(C_CLI_FAULT_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS) _tcc/arena_fault.o
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/cli -o $@ $< $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(filter-out c/rt/arena.c,$(C_RT_SOURCES)) _tcc/arena_fault.o $(LDFLAGS) -lpthread
$(C_IO_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/cli -o $@ $< $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# CLI drivers that also link the import pipeline (c/import). import_protocol reads the frozen UAT export
# through Files.read_file with --corpus; c-test runs its 120 fixture cases (IMPORT-OK).
C_IMPORT_IO := import_protocol
C_IMPORT_IO_BINARIES := $(addprefix _tcc/,$(C_IMPORT_IO))
$(C_IMPORT_IO_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_IMPORT_SOURCES) $(C_IMPORT_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/import -I c/cli -o $@ $< $(C_CLI_SOURCES) $(C_IMPORT_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# CLI drivers that also link the WasmGC backend (c/wasm). wasm_host writes /private/tmp and spawns
# node and wasmtime, so it is built by c-build but not run by c-test. wasm_protocol (SUITE-WASM) spawns
# wasm-opt and node over a fixture repo; its live oracle replay (W/hop168-driver-oracle.sh) gates it.
C_CLI_WASM := wasm_host wasm_protocol
C_CLI_WASM_BINARIES := $(addprefix _tcc/,$(C_CLI_WASM))
$(C_CLI_WASM_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_WASM_SOURCES) $(C_WASM_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/wasm -I c/cli -o $@ $< $(C_CLI_SOURCES) $(C_WASM_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# The kanon drivers link c/cli/kanon.c over c/cli + wasm + surface + kernel. cli_core (bend2/tests/cli_core.bend)
# is built by c-build; it writes temporary files and spawns node and wasmtime, so its live oracle replay
# (the corpus kanon label) and test/protocols/cli_core_check.py gate it outside c-test. The kanon drivers go to
# _tcc/bin/ as the Bend binaries go to _bend2/bin/: Host.root (bend2/cli/host.bend) is the third dirname of
# the executable, and kanon run finds dev/run-node.mjs and dev/run-wasmtime.sh under that root.
C_KANON_CLI := cli_core
C_KANON_BINARIES := $(addprefix _tcc/bin/,$(C_KANON_CLI))
$(C_KANON_BINARIES): _tcc/bin/%: c/tests/%.c $(C_CLI_KANON) $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_WASM_SOURCES) $(C_WASM_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc/bin
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/wasm -I c/cli -o $@ $< $(C_CLI_KANON) $(C_CLI_SOURCES) $(C_WASM_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# mech_cert (bend2/mech_cert.bend) is c/cli/mech_cert.c over c/cli (cert.c) + surface + kernel. c-build builds
# it to _tcc/bin/ beside the kanon drivers; dev/c-cli-check.py --cert _tcc/bin/mech_cert gates its 13
# cert cases (it writes temporary files, so it runs outside c-test).
C_CERT_BINARIES := _tcc/bin/mech_cert
$(C_CERT_BINARIES): _tcc/bin/%: c/cli/%.c $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc/bin
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/cli -o $@ $< $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# prelude_runtime also links the two runtime library modules: runtime_slice.c compiled with
# RUNTIME_SLICE_LIBRARY (which drops its c-test main) and runtime_reachable.o. c-build builds it, c-test
# runs its --slice-self-test; its oracle calls (test/gpu_auction.py) are replayed outside c-test.
C_CLI_RUNTIME := prelude_runtime
C_CLI_RUNTIME_BINARIES := $(addprefix _tcc/,$(C_CLI_RUNTIME))
C_RUNTIME_LINK := _tcc/runtime_slice_library.o _tcc/runtime_reachable.o
_tcc/runtime_slice_library.o: c/tests/runtime_slice.c $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -DRUNTIME_SLICE_LIBRARY -I c/rt -I c/kernel -c -o $@ $<
$(C_CLI_RUNTIME_BINARIES): _tcc/%: c/tests/%.c $(C_RUNTIME_LINK) $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_WASM_SOURCES) $(C_WASM_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/wasm -I c/cli -o $@ $< $(C_RUNTIME_LINK) $(C_CLI_SOURCES) $(C_WASM_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
C_PRELUDE_LINK := $(C_RELATIONAL_LIBS) _tcc/relational_syntax_equal.o
# Relational drivers that also link c/cli. kernel_protocol (SUITE-KERNEL) links the kernel_recursive
# library object; its live oracle replay (W/hop168-driver-oracle.sh) gates it, so c-test does not run it.
C_RELATIONAL_IO := kernel_protocol
C_RELATIONAL_IO_BINARIES := $(addprefix _tcc/,$(C_RELATIONAL_IO))
C_RELATIONAL_IO_LINK := $(C_PRELUDE_LINK) _tcc/kernel_recursive_lib.o
$(C_RELATIONAL_IO_BINARIES): _tcc/%: c/tests/%.c $(C_RELATIONAL_IO_LINK) c/tests/kernel_recursive.h $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -I c/cli -o $@ $< $(C_RELATIONAL_IO_LINK) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# S7.7 dispatchers (bend2/tests/runner.bend, surface_foundation_runner.bend, relational_native*.bend,
# relational_protocols.bend): exec wrappers that resolve `<dir of argv[0]>/<target>` and link no library
# source. c-test-dispatch checks their own texts (no mode, unknown mode, the boolean unit line) against the
# frozen c/tests/<name>.expected (no _bend2 artifact runs them: W/oracle/no-oracle-drivers.txt) and the
# pass-through modes against the direct driver (the prelude drivers on a missing root fail fast, 0 MB).
C_DISPATCH := runner surface_foundation_runner relational_native relational_native_0 relational_native_1 relational_native_2 relational_native_3 relational_native_4 relational_native_5 relational_protocols
C_DISPATCH_BINARIES := $(addprefix _tcc/,$(C_DISPATCH))
$(C_DISPATCH_BINARIES): _tcc/%: c/tests/%.c c/tests/dispatch.h
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -o $@ $<
c-build-artifacts: $(C_DISPATCH_BINARIES) _tcc/dispatch_probe $(C_MEMCAP_BINARY)
# The prelude drivers that the dispatch_same legs below run directly (a dispatcher run against a direct run).
C_DISPATCH_DIRECT := prelude_category prelude_left_kan prelude_composable_functors prelude_shared_nattrans prelude_horizontal_laws prelude_horizontal_associativity prelude_nattrans_units prelude_nattrans
_tcc/dispatch_path: c/tests/dispatch_path.c c/tests/dispatch.h
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -o $@ $<
_tcc/dispatch_probe _tcc/memcap: _tcc/%: c/tests/%.c
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -o $@ $<
# $(call dispatch_leg,NAME,ARGS): a `$ NAME ARGS` line, both streams, then rc=N, appended to _tcc/NAME.actual.
dispatch_leg = ( echo '$$ $(strip $(1) $(2))'; ./_tcc/$(1) $(2) 2>&1; echo "rc=$$?" ) >> _tcc/$(1).actual
# $(call dispatch_same,NAME,MODE ARGS,TARGET,ARGS): the dispatcher run and the direct run give the same bytes and code.
dispatch_same = ( ./_tcc/$(1) $(2) 2>&1; echo "rc=$$?" ) > _tcc/$(1).via; ( ./_tcc/$(3) $(4) 2>&1; echo "rc=$$?" ) > _tcc/$(1).direct; cmp _tcc/$(1).via _tcc/$(1).direct
.PHONY: c-dispatch-actual c-dispatch-probe c-test-dispatch
c-dispatch-actual: $(C_DISPATCH_BINARIES) $(addprefix _tcc/,$(C_DISPATCH_DIRECT))
	@rm -f $(addsuffix .actual,$(C_DISPATCH_BINARIES))
	@$(call dispatch_leg,runner,)
	@$(call dispatch_leg,runner,nope)
	@$(call dispatch_leg,runner,unit)
	@$(call dispatch_leg,runner,unit nope)
	@$(call dispatch_leg,runner,unit kernel-numbers extra)
	@$(call dispatch_leg,runner,fixture)
	@$(call dispatch_leg,runner,fixture nope)
	@$(call dispatch_leg,runner,unit surface-lexer)
	@$(call dispatch_leg,surface_foundation_runner,)
	@$(call dispatch_leg,surface_foundation_runner,nope)
	@$(call dispatch_leg,relational_native,)
	@$(call dispatch_leg,relational_native,nope)
	@$(call dispatch_leg,relational_native_0,)
	@$(call dispatch_leg,relational_native_0,nope)
	@$(call dispatch_leg,relational_native_1,)
	@$(call dispatch_leg,relational_native_1,nope)
	@$(call dispatch_leg,relational_native_2,)
	@$(call dispatch_leg,relational_native_2,nope)
	@$(call dispatch_leg,relational_native_3,)
	@$(call dispatch_leg,relational_native_3,nope)
	@$(call dispatch_leg,relational_native_4,)
	@$(call dispatch_leg,relational_native_4,nope)
	@$(call dispatch_leg,relational_native_5,)
	@$(call dispatch_leg,relational_native_5,nope)
	@$(call dispatch_leg,relational_protocols,)
	@$(call dispatch_leg,relational_protocols,nope)
# The routing probe: copies of runner and surface_foundation_runner in _tcc/dispatch-probe/, where each target
# name is a link to _tcc/dispatch_probe (c/tests/dispatch_probe.c). Each `$ ` line of
# c/tests/dispatch_probe.expected is one run; the output shows the target and the arguments of every table row.
c-dispatch-probe: _tcc/runner _tcc/surface_foundation_runner _tcc/dispatch_probe
	@rm -rf _tcc/dispatch-probe
	@mkdir -p _tcc/dispatch-probe
	@cp _tcc/runner _tcc/surface_foundation_runner _tcc/dispatch-probe/
	@awk '$$1 == "probe" || $$1 == "PASS" { print $$2 }' c/tests/dispatch_probe.expected | sort -u | xargs -I{} ln -s ../dispatch_probe _tcc/dispatch-probe/{}
	@awk '/^\$$ / { sub(/^\$$ /, ""); print }' c/tests/dispatch_probe.expected | while read -r run; do echo "$$ $$run"; ./_tcc/dispatch-probe/$$run 2>&1; echo "rc=$$?"; done > _tcc/dispatch_probe.actual
	cmp _tcc/dispatch_probe.actual c/tests/dispatch_probe.expected
	@echo "DISPATCH-PROBE-OK runs=$$(awk '/^\$$ /' c/tests/dispatch_probe.expected | wc -l | tr -d ' ')"
c-test-dispatch: c-dispatch-actual c-dispatch-probe _tcc/dispatch_path
	./_tcc/dispatch_path
	$(foreach d,$(C_DISPATCH),cmp _tcc/$(d).actual c/tests/$(d).expected &&) true
	@$(call dispatch_same,relational_native,category /nonexistent,prelude_category,/nonexistent)
	@$(call dispatch_same,relational_native_0,left-kan /nonexistent,prelude_left_kan,/nonexistent)
	@$(call dispatch_same,relational_native_1,composable-functors /nonexistent,prelude_composable_functors,/nonexistent)
	@$(call dispatch_same,relational_native_2,shared-nattrans /nonexistent,prelude_shared_nattrans,/nonexistent)
	@$(call dispatch_same,relational_native_3,horizontal-laws /nonexistent,prelude_horizontal_laws,/nonexistent)
	@$(call dispatch_same,relational_native_4,horizontal-associativity /nonexistent,prelude_horizontal_associativity,/nonexistent)
	@$(call dispatch_same,relational_native_5,nattrans-units /nonexistent,prelude_nattrans_units,/nonexistent)
	@$(call dispatch_same,relational_protocols,nattrans /nonexistent,prelude_nattrans,/nonexistent)
	@echo "DISPATCH-OK dispatchers=10 legs=26 same=8"

.PHONY: c-build c-build-artifacts c-test c-test-prelude
c-build:
	+$(PYTHON) dev/c-build.py --tcc "$(TCC)" --cflags="$(CFLAGS)" --ldflags="$(LDFLAGS)"

c-build-artifacts: _tcc/rt_test _tcc/rt_regression_test _tcc/rt_oom_test $(C_KERNEL_BINARIES) $(C_KERNEL_FAULT_BINARIES) $(C_SURFACE_BINARIES) $(C_LEVELS_BINARIES) $(C_IMPORT_BINARIES) $(C_WASM_BINARIES) $(C_IO_BINARIES) $(C_CLI_FAULT_BINARIES) $(C_IMPORT_IO_BINARIES) $(C_CLI_WASM_BINARIES) $(C_KANON_BINARIES) $(C_CERT_BINARIES) $(C_CLI_RUNTIME_BINARIES) $(C_RELATIONAL_IO_BINARIES) $(C_SURFACE_LIBS) _tcc/runtime_reachable.o _tcc/relational_syntax_equal.o $(C_LEVELS_LIBS) $(C_RELATIONAL_LIBS) $(C_PRELUDE_BINARIES) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) _tcc/rust_emit _tcc/rust_frontend _tcc/rust_let _tcc/rust_import _tcc/rust_lexer_lines _tcc/rust_lower_under

# The c/rust objects (S6, bend2/rust): c-build compiles them; the S6 drivers and c/cli link them.
$(C_RUST_OBJECTS): _tcc/rust/%.o: c/rust/%.c $(C_RUST_HEADERS) $(C_CLI_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS) $(C_SURFACE_HEADERS)
	@mkdir -p _tcc/rust
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -c -o $@ $<
$(C_CLI_RUST_OBJECTS): _tcc/cli/%.o: c/cli/%.c $(C_CLI_HEADERS) $(C_RUST_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS) $(C_SURFACE_HEADERS)
	@mkdir -p _tcc/cli
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -c -o $@ $<
# The rust_emit driver (bend2/tests/rust_emit.bend and its library module rust_emit_oracle.bend):
# c/tests/rust_emit.c + rust_emit_oracle.c over c/rust, c/cli (with rust_out.o), surface and kernel.
C_RUST_EMIT_SOURCES := c/tests/rust_emit.c c/tests/rust_emit_oracle.c
_tcc/rust_emit: $(C_RUST_EMIT_SOURCES) $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ $(C_RUST_EMIT_SOURCES) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The rust_frontend driver (bend2/tests/rust_frontend.bend): lex, parse, print, check and pos over
# c/rust (the fragment pass and the parser); linked as rust_emit is, since c/rust needs c/cli.
_tcc/rust_frontend: c/tests/rust_frontend.c $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ c/tests/rust_frontend.c $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The rust_let driver (bend2/tests/rust_let.bend): direct RIR let lowering through c/rust lower.c and
# infer.c; linked as rust_frontend is.
_tcc/rust_let: c/tests/rust_let.c $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ c/tests/rust_let.c $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The rust_lexer_lines driver (C only): the line and column tracking of c/rust/lexer.c across
# comments, which the Bend gates do not reach; linked as rust_frontend is.
_tcc/rust_lexer_lines: c/tests/rust_lexer_lines.c $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ c/tests/rust_lexer_lines.c $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The rust_lower_under driver (C only): the underscore rule of c/rust/lower.c local names
# (bend2/rust/lower.bend:50-59), which the Bend gates do not reach; linked as rust_let is.
_tcc/rust_lower_under: c/tests/rust_lower_under.c $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ c/tests/rust_lower_under.c $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The rust_import driver (bend2/tests/rust_import.bend): the lift, resolve, types, lower, check,
# seed-check, rt-mech and run modes over c/rust and c/cli/rust_in.c; linked as rust_frontend is.
_tcc/rust_import: c/tests/rust_import.c $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ c/tests/rust_import.c $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# Rust allocation regressions use the renamed arena allocator to inject one failed allocation.
C_RUST_FAULT_CHECKS := rust_resolve_oom rust_infer_oom rust_emit_oom rust_print_oom
C_RUST_FAULT_BINARIES := $(addprefix _tcc/,$(C_RUST_FAULT_CHECKS))
$(C_RUST_FAULT_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_RUST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS) _tcc/arena_fault.o
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/rust -I c/cli -I c/tests -o $@ $< $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(filter-out c/rt/arena.c,$(C_RT_SOURCES)) _tcc/arena_fault.o $(LDFLAGS) -lpthread
c-build-artifacts: $(C_RUST_FAULT_BINARIES)

# The mech and mechanism-driver programs (bend2/mech.bend, bend2/main.bend; S6.13): c/cli/mech_main.c
# and c/cli/main.c over c/cli/mech.c (bend2/cli/mech.bend), kanon.c, rust_out.o, rust_in.o, c/rust,
# c/import, c/wasm, surface and kernel. The S6 full-corpus oracle diff runs them.
C_MECH_LINK := $(C_CLI_MECH) $(C_CLI_KANON) $(C_CLI_RUST_OBJECTS) $(C_RUST_OBJECTS) $(C_CLI_SOURCES) $(C_IMPORT_SOURCES) $(C_WASM_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES)
C_MECH_HEADERS := $(C_CLI_HEADERS) $(C_RUST_HEADERS) $(C_IMPORT_HEADERS) $(C_WASM_HEADERS) $(C_SURFACE_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS)
C_MECH_BINARIES := _tcc/bin/mech _tcc/bin/main
_tcc/bin/mech: c/cli/mech_main.c $(C_MECH_LINK) $(C_MECH_HEADERS)
	@mkdir -p _tcc/bin
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/import -I c/wasm -I c/rust -I c/cli -o $@ c/cli/mech_main.c $(C_MECH_LINK) $(LDFLAGS) -lpthread
_tcc/bin/main: c/cli/main.c $(C_MECH_LINK) $(C_MECH_HEADERS)
	@mkdir -p _tcc/bin
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/import -I c/wasm -I c/rust -I c/cli -o $@ c/cli/main.c $(C_MECH_LINK) $(LDFLAGS) -lpthread
c-build-artifacts: $(C_MECH_BINARIES)

# bend2/tests/runtime_reachable.bend is a library module with no main: c-build compiles its port and
# prelude_runtime (S5.1) links it and runs its self test.
_tcc/runtime_reachable.o: c/tests/runtime_reachable.c $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -c -o $@ $<

# bend2/tests/relational_syntax_equal.bend is a library module with no main: c-build compiles its port and
# the drivers of relational_common, surface_sl, surface_prenex_exports and kernel_protocol link it (S2.5 on).
_tcc/relational_syntax_equal.o: c/tests/relational_syntax_equal.c $(C_TEST_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -c -o $@ $<

# bend2/tests/relational_fs.bend, relational_common.bend, relational_text.bend, relational_program.bend and
# relational_digest.bend are library modules with no main: c-build compiles their ports and the prelude_* and
# relational_* drivers link them (S2.7 on).
$(C_RELATIONAL_LIBS): _tcc/%.o: c/tests/%.c $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_KERNEL_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -c -o $@ $<

# Prelude drivers (bend2/tests/prelude_<x>.bend ported to c/tests/prelude_<x>.c): each takes the repository
# root as argv[1], links the relational support objects and prints PRELUDE-<X>-OK fields or PRELUDE-<X>-FAIL.
# C_PRELUDE_CHECKS, C_PRELUDE_BINARIES and C_PRELUDE_LINK are defined with the relational lists above c-build.
$(C_PRELUDE_BINARIES): _tcc/%: c/tests/%.c $(C_PRELUDE_LINK) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# Foundation prelude drivers (bend2/tests/surface_prelude_{poly,congruence,transport,dependent,equality_ops}.bend
# and surface_numbers.bend): each takes the repo root as argv[1] (surface_numbers takes none) and prints
# PRELUDE-<X>-OK fields or PRELUDE-<X>-FAIL message. They are cheap, so c-test runs them under memcap.
# surface_prelude_poly and surface_prelude_congruence are also libraries (Audit and Congruence of their
# importers): the _lib.o objects are built with -DMECH_DRIVER_LIBRARY, which drops main.
C_FOUNDATION_CHECKS := surface_prelude_poly surface_prelude_congruence surface_prelude_transport surface_prelude_dependent surface_prelude_equality_ops
C_FOUNDATION_BINARIES := $(addprefix _tcc/,$(C_FOUNDATION_CHECKS)) _tcc/surface_numbers
C_FOUNDATION_LIBS := _tcc/surface_prelude_poly_lib.o _tcc/surface_prelude_congruence_lib.o
C_FOUNDATION_OBJS := _tcc/surface_raw_common.o _tcc/surface_foundation_common.o _tcc/surface_foundation_data.o
C_FOUNDATION_COMMON := _tcc/surface_foundation_common.o _tcc/surface_foundation_data.o _tcc/surface_prelude_poly_lib.o
c-build-artifacts: $(C_FOUNDATION_BINARIES) _tcc/surface_raw_common.o

$(C_FOUNDATION_LIBS): _tcc/%_lib.o: c/tests/%.c $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -DMECH_DRIVER_LIBRARY -c -o $@ $<

$(C_FOUNDATION_OBJS): _tcc/%.o: c/tests/%.c $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -c -o $@ $<

_tcc/surface_prelude_congruence _tcc/surface_prelude_dependent _tcc/surface_prelude_equality_ops: C_FOUNDATION_LINK := $(C_FOUNDATION_COMMON)
_tcc/surface_prelude_transport: C_FOUNDATION_LINK := $(C_FOUNDATION_COMMON) _tcc/surface_prelude_congruence_lib.o

$(C_FOUNDATION_BINARIES): _tcc/%: c/tests/%.c $(C_PRELUDE_LINK) $(C_FOUNDATION_LIBS) $(C_FOUNDATION_OBJS) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_FOUNDATION_LINK) $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The prenex drivers (ports of bend2/tests/surface_prenex*.bend): each takes the repo root as argv[1] and prints
# <TAG>-OK fields or <TAG>-FAIL message (surface_prenex_exports dies with code 1 and PRENEX-EXPORTS FAIL message).
# surface_prenex_common and surface_prenex_data are plain library modules. surface_prenex and
# surface_prenex_exports are also libraries (Prenex of surface_prenex_families, Exports of
# surface_prenex_dependency_exports): the _lib.o objects are built with -DMECH_DRIVER_LIBRARY, which drops main.
C_PRENEX_CHECKS := surface_prenex surface_prenex_exports surface_prenex_dependency_exports surface_prenex_families \
	surface_prenex_groups
C_PRENEX_BINARIES := $(addprefix _tcc/,$(C_PRENEX_CHECKS))
C_PRENEX_LIBS := _tcc/surface_prenex_lib.o _tcc/surface_prenex_exports_lib.o
C_PRENEX_OBJS := _tcc/surface_prenex_common.o _tcc/surface_prenex_data.o
C_PRENEX_COMMON := $(C_PRENEX_OBJS) _tcc/surface_raw_common.o $(C_FOUNDATION_COMMON)
c-build-artifacts: $(C_PRENEX_BINARIES) $(C_PRENEX_LIBS)

$(C_PRENEX_LIBS): _tcc/%_lib.o: c/tests/%.c $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -DMECH_DRIVER_LIBRARY -c -o $@ $<

$(C_PRENEX_OBJS): _tcc/%.o: c/tests/%.c $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -c -o $@ $<

_tcc/surface_prenex _tcc/surface_prenex_exports _tcc/surface_prenex_groups: C_PRENEX_LINK := $(C_PRENEX_COMMON)
_tcc/surface_prenex_families: C_PRENEX_LINK := $(C_PRENEX_COMMON) _tcc/surface_prenex_lib.o
_tcc/surface_prenex_dependency_exports: C_PRENEX_LINK := $(C_PRENEX_COMMON) _tcc/surface_prenex_exports_lib.o

$(C_PRENEX_BINARIES): _tcc/%: c/tests/%.c $(C_PRELUDE_LINK) $(C_PRENEX_LIBS) $(C_PRENEX_COMMON) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_PRENEX_LINK) $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The SL surface driver (port of bend2/tests/surface_sl.bend): takes no argv, prints one SL-SURFACE <name> OK
# line per case, then SL-SURFACE-OK <n>/36 and SL-SURFACE OK (fewer than 36 passes die with code 1 and
# SL-SURFACE FAIL). surface_sl_data is a plain library module (port of surface_sl_data.bend).
C_SL_CHECKS := surface_sl
C_SL_BINARIES := $(addprefix _tcc/,$(C_SL_CHECKS))
C_SL_OBJS := _tcc/surface_sl_data.o
c-build-artifacts: $(C_SL_BINARIES)

$(C_SL_OBJS): _tcc/%.o: c/tests/%.c c/tests/%.h
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -c -o $@ $<

$(C_SL_BINARIES): _tcc/%: c/tests/%.c $(C_SL_OBJS) $(C_PRELUDE_LINK) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_SL_OBJS) $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# The family group drivers (ports of bend2/tests/surface_family_groups.bend and surface_family_members.bend)
# take no argv. surface_family_groups is also the Groups library of surface_family_members: the _lib.o object
# is built with -DMECH_DRIVER_LIBRARY, which drops main.
C_FAMILY_CHECKS := surface_family_groups surface_family_members
C_FAMILY_BINARIES := $(addprefix _tcc/,$(C_FAMILY_CHECKS))
C_FAMILY_LIBS := _tcc/surface_family_groups_lib.o
C_FAMILY_COMMON := _tcc/surface_raw_common.o $(C_FOUNDATION_COMMON)
c-build-artifacts: $(C_FAMILY_BINARIES) $(C_FAMILY_LIBS)

$(C_FAMILY_LIBS): _tcc/%_lib.o: c/tests/%.c c/tests/%.h $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -DMECH_DRIVER_LIBRARY -c -o $@ $<

$(C_FAMILY_BINARIES): _tcc/%: c/tests/%.c c/tests/surface_family_groups.h $(C_PRELUDE_LINK) $(C_FAMILY_COMMON) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_FAMILY_COMMON) $(if $(filter %members,$@),_tcc/surface_family_groups_lib.o) $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

_tcc/surface_family_members: _tcc/surface_family_groups_lib.o

# The template drivers (ports of bend2/tests/surface_template_{composition,reuse,symbolic_reuse}.bend) share
# two plain library modules: surface_template_data (generated tables, rows typed by surface_prenex_data.h) and
# surface_template_common.
C_TEMPLATE_OBJS := _tcc/surface_template_common.o _tcc/surface_template_data.o
c-build-artifacts: $(C_TEMPLATE_OBJS)

$(C_TEMPLATE_OBJS): _tcc/%.o: c/tests/%.c c/tests/%.h $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -c -o $@ $<

# The template driver binaries take the repository root as the optional argv[1]. They link the two template
# objects, $(C_PRENEX_COMMON) (template_common calls surface_prenex_common_family_count) and the Exports
# library _tcc/surface_prenex_exports_lib.o (Exports.roundtrip); C_TEMPLATE_LINK adds per-driver libraries.
C_TEMPLATE_CHECKS := surface_template_reuse surface_template_symbolic_reuse surface_template_composition
C_TEMPLATE_BINARIES := $(addprefix _tcc/,$(C_TEMPLATE_CHECKS))
C_TEMPLATE_COMMON := $(C_TEMPLATE_OBJS) $(C_PRENEX_COMMON) _tcc/surface_prenex_exports_lib.o
c-build-artifacts: $(C_TEMPLATE_BINARIES)
_tcc/surface_template_composition: C_TEMPLATE_LINK := _tcc/surface_family_groups_lib.o
_tcc/surface_template_composition: _tcc/surface_family_groups_lib.o

$(C_TEMPLATE_BINARIES): _tcc/%: c/tests/%.c c/tests/surface_template_common.h c/tests/surface_template_data.h $(C_PRELUDE_LINK) $(C_TEMPLATE_COMMON) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_TEMPLATE_COMMON) $(C_TEMPLATE_LINK) $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

# S7.5: the relational oracle drivers (bend2/tests/relational_oracle, relational_oracle_native_probe and
# relational_support) link the relational support modules. relational_oracle has a live oracle (the two
# relational-oracle captures); the other two have none and run against frozen c/tests/<driver>.expected files.
C_RELATIONAL_CHECKS := relational_oracle relational_oracle_native_probe relational_support
C_RELATIONAL_BINARIES := $(addprefix _tcc/,$(C_RELATIONAL_CHECKS))
c-build-artifacts: $(C_RELATIONAL_BINARIES)
$(C_RELATIONAL_BINARIES): _tcc/%: c/tests/%.c $(C_PRELUDE_LINK) $(C_RELATIONAL_HEADERS) $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_PRELUDE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# c-test-relational: the S7.5 legs of c-test. relational_support runs on a fixture directory made here
# (the files "plain" and "line<newline>break", as bend2/tests/relational_support.bend expects).
.PHONY: c-test-relational
c-test-relational: $(C_RELATIONAL_BINARIES)
	./_tcc/relational_oracle alpha
	./_tcc/relational_oracle 'λ🙂'
	./_tcc/relational_oracle_native_probe 'quantity: alpha' > _tcc/relational_oracle_native_probe.actual
	./_tcc/relational_oracle_native_probe 'mismatch:' >> _tcc/relational_oracle_native_probe.actual
	cmp _tcc/relational_oracle_native_probe.actual c/tests/relational_oracle_native_probe.expected
	@echo "PROBE-OK relational_oracle_native_probe lines=8"
	rm -rf _tcc/relational_support_fixture
	mkdir -p _tcc/relational_support_fixture
	: > _tcc/relational_support_fixture/plain
	: > "_tcc/relational_support_fixture/$$(printf 'line\nbreak')"
	./_tcc/relational_support _tcc/relational_support_fixture > _tcc/relational_support.actual
	cmp _tcc/relational_support.actual c/tests/relational_support.expected
	@echo "PROBE-OK relational_support lines=1"

# The Bend test drivers import each other (levels_protocol imports levels_regression as Algebra; the kernel
# numeric drivers import levels_protocol as Test; kernel_numeric_regressions sequences four suites): each such
# driver compiles once more as a library object (MECH_DRIVER_LIBRARY drops its main) that the dependents link.
$(C_LEVELS_LIBS): _tcc/%_lib.o: c/tests/%.c c/tests/levels_protocol.h c/tests/levels_regression.h $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -DMECH_DRIVER_LIBRARY -c -o $@ $<

_tcc/levels_protocol: C_LEVELS_LINK := _tcc/levels_regression_lib.o
_tcc/kernel_large_addresses _tcc/kernel_conversion_budget: C_LEVELS_LINK := _tcc/levels_regression_lib.o _tcc/levels_protocol_lib.o
_tcc/kernel_numeric_regressions: C_LEVELS_LINK := $(C_LEVELS_LIBS)

$(C_LEVELS_BINARIES): _tcc/%: c/tests/%.c c/tests/levels_protocol.h c/tests/levels_regression.h $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS) $(C_LEVELS_LIBS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_LEVELS_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

$(C_SURFACE_LIBS): _tcc/%_lib.o: c/tests/%.c $(C_TEST_HEADERS) $(C_KERNEL_HEADERS) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -DMECH_DRIVER_LIBRARY -c -o $@ $<

_tcc/surface_boundary _tcc/surface_shape_metadata _tcc/surface_universes: C_SURFACE_LINK := _tcc/surface_catalog_lib.o
_tcc/surface_boundary _tcc/surface_shape_metadata _tcc/surface_universes: $(C_SURFACE_LIBS)

$(C_KERNEL_BINARIES) $(C_SURFACE_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_SURFACE_LINK) $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

$(C_IMPORT_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_IMPORT_SOURCES) $(C_IMPORT_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/import -o $@ $< $(C_IMPORT_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

$(C_WASM_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_WASM_SOURCES) $(C_WASM_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/wasm -o $@ $< $(C_WASM_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread

_tcc/rt_test _tcc/rt_regression_test: _tcc/%: c/tests/%.c $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -o $@ $< $(C_RT_SOURCES) $(LDFLAGS) -lpthread

_tcc/arena_fault.o: c/rt/arena.c $(C_RT_HEADERS)
	@mkdir -p _tcc
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -Darena_alloc=rt_test_arena_alloc_real -c -o $@ $<

_tcc/rt_oom_test: c/tests/rt_oom_test.c $(C_RT_SOURCES) $(C_RT_HEADERS) _tcc/arena_fault.o
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -o $@ $< $(filter-out c/rt/arena.c,$(C_RT_SOURCES)) _tcc/arena_fault.o $(LDFLAGS) -lpthread

$(C_KERNEL_FAULT_BINARIES): _tcc/%: c/tests/%.c $(C_TEST_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(C_SURFACE_HEADERS) $(C_PRELUDE_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS) _tcc/arena_fault.o
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/prelude -o $@ $< $(C_KERNEL_SOURCES) $(C_SURFACE_SOURCES) $(C_PRELUDE_SOURCES) $(filter-out c/rt/arena.c,$(C_RT_SOURCES)) _tcc/arena_fault.o $(LDFLAGS) -lpthread

c-test: c-build
	./_tcc/rt_test
	./_tcc/rt_regression_test
	./_tcc/rt_oom_test
	$(foreach d,$(C_KERNEL_FAULT_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_RUST_FAULT_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_KERNEL_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_SURFACE_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_FOUNDATION_CHECKS),$(call capped,$(MEMCAP_SECS)) ./_tcc/$(d) $(CURDIR) &&) true
	$(call capped,$(MEMCAP_SECS)) ./_tcc/surface_numbers
	$(foreach d,$(C_PRENEX_CHECKS),$(call capped,$(MEMCAP_SECS)) ./_tcc/$(d) $(CURDIR) &&) true
	$(foreach d,$(C_SL_CHECKS),$(call capped,$(MEMCAP_SECS)) ./_tcc/$(d) &&) true
	$(foreach d,$(C_FAMILY_CHECKS),$(call capped,$(MEMCAP_SECS)) ./_tcc/$(d) &&) true
	$(foreach d,$(C_TEMPLATE_CHECKS),$(call capped,$(MEMCAP_SECS)) ./_tcc/$(d) $(CURDIR) &&) true
	$(MAKE) --no-print-directory c-test-relational
	$(MAKE) --no-print-directory c-test-dispatch
	$(foreach d,$(C_LEVELS_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_IMPORT_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_WASM_CHECKS),./_tcc/$(d) &&) true
	$(PYTHON) -P dev/c-io-check.py --driver _tcc/io_driver
	$(foreach d,$(C_CLI_FAULT_CHECKS),./_tcc/$(d) &&) true
	./_tcc/prelude_runtime --slice-self-test
	./_tcc/template_cost prelude/cat/category.mech --through Hom > /dev/null
	./_tcc/rust_lexer_lines
	./_tcc/rust_lower_under
	./_tcc/wasm_circuit_bounds > /dev/null
	./_tcc/import_protocol > /dev/null
	./_tcc/surface_driver 'def x : Type := Type'
	./_tcc/surface_check_driver 'def main : Nat := 12345678901234567890123456789'
	./_tcc/kernel_erase_driver 'def main : Nat := 12345678901234567890123456789'
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
	./_tcc/surface_shape_metadata > _tcc/surface_shape_metadata.actual
	cmp _tcc/surface_shape_metadata.actual c/tests/surface_shape_metadata.expected
	@echo "PROBE-OK surface_shape_metadata shapes=4"
	./_tcc/import_types_protocol > _tcc/import_types_protocol.actual
	cmp _tcc/import_types_protocol.actual c/tests/import_types_protocol.expected
	@echo "PROBE-OK import_types_protocol lines=29"
	./_tcc/mapping_protocol > _tcc/mapping_protocol.actual
	cmp _tcc/mapping_protocol.actual c/tests/mapping_protocol.expected
	@echo "PROBE-OK mapping_protocol lines=15"

# Prelude drivers (C_PRELUDE_CHECKS) elaborate the whole prelude per entry, so each runs under the memcap tool:
# footprint cap MEMCAP_MB (3500 MB, the 4 GB rule), time cap MEMCAP_SECS (120 s) or the per-driver
# MEMCAP_SECS_<driver> override (600 s for each driver whose full leg measures over 120 s; figures below;
# prelude_shared_nattrans instantiates 42 entries and needs about 245 s), SIGKILL at either cap. memcap exits
# with the driver's status. c-build builds every driver (one alone: `make _tcc/prelude_<x>`).
# c-test-prelude is not a c-test dependency until every listed driver measures under the cap.
# Twelve drivers joined C_PRELUDE_CHECKS after the structural sharing of the compact path (D-348a in
# c/kernel/compact.c) and the compact refusal checks (D-352a in relational_negatives.c, D-353a in
# prelude_heterogeneous_left_kan.c, D-355a in prelude_left_kan_laws.c, D-360a in the four
# prelude_left_kan_<x>.c contract drivers). Full leg `ROOT` alone under memcap 3800 900 (footMB =
# phys_footprint peak, wall seconds; the old kill figures in parentheses):
#   prelude_heterogeneous_left_kan        rc 0  foot  312 MB   24.8 s (killed, foot 3506 MB at 62.0 s)
#   prelude_left_kan_laws                 rc 0  foot 1814 MB  181.2 s (killed, foot 3837 MB at 301.3 s)
#   prelude_horizontal_associativity      rc 0  foot 1308 MB  188.7 s (killed, foot 3504 MB at 26.6 s)
#   prelude_horizontal_laws               rc 0  foot 2084 MB  303.1 s (killed, foot 3522 MB at 26.7 s)
#   prelude_iterated_whiskering           rc 0  foot  791 MB   63.6 s (killed, foot 2714 MB at 120.1 s)
#   prelude_whiskering_laws               rc 0  foot 1302 MB  140.7 s (killed, foot 3519 MB at 32.6 s)
#   prelude_shared_left_kan               rc 0  foot 1322 MB   85.9 s (killed, foot 3541 MB at 30.2 s)
#   prelude_nattrans_units                rc 0  foot  255 MB  120.3 s (killed, foot 3820 MB at 389.2 s)
#   prelude_left_kan_roundtrip            rc 0  foot 1579 MB  101.7 s (killed, foot 3882 MB at 88.9 s)
#   prelude_left_kan_cocone_action        rc 0  foot 1579 MB  158.2 s (killed, foot 3897 MB at 95.9 s)
#   prelude_left_kan_mediator_equality    rc 0  foot 1567 MB  188.2 s (killed, foot 3810 MB at 109.6 s)
#   prelude_left_kan_solution_transport   rc 0  foot 1592 MB  147.3 s (killed, foot 3845 MB at 112.8 s)
# Record legs (not run by c-test-prelude) print the tracked test/neg pins: prelude_left_kan_laws
# `ROOT --record-cocone-equality-oracles` the 5 left-kan-laws/cocone-eq-*.err pins byte for byte (rc 0, foot
# 1571 MB, 76.7 s), and the four contract drivers `ROOT --record-oracles` their left-kan-<x> pins by the pin
# policy of each directory (full text for roundtrip and cocone_action, prefix + .digest for mediator_equality
# and solution_transport): rc 0, foot 1539 to 1597 MB, 92.8 to 104.3 s. prelude_nattrans_units has no record leg.
# The contract drivers read _bend2/left-kan-<x>-contracts/focused-source.mech under ROOT (the
# dev/left-kan-cocone-congruence-focus.py output); mediator_equality and solution_transport need a ROOT
# that holds their focused sources.
MEMCAP_SECS_prelude_shared_nattrans := 600
MEMCAP_SECS_prelude_left_kan_laws := 600
MEMCAP_SECS_prelude_horizontal_associativity := 600
MEMCAP_SECS_prelude_horizontal_laws := 600
MEMCAP_SECS_prelude_whiskering_laws := 600
MEMCAP_SECS_prelude_nattrans_units := 600
MEMCAP_SECS_prelude_left_kan_cocone_action := 600
MEMCAP_SECS_prelude_left_kan_mediator_equality := 600
MEMCAP_SECS_prelude_left_kan_solution_transport := 600
c-test-prelude: $(C_PRELUDE_BINARIES) $(C_MEMCAP_BINARY)
	$(foreach d,$(C_PRELUDE_CHECKS),$(call capped,$(or $(MEMCAP_SECS_$(d)),$(MEMCAP_SECS))) ./_tcc/$(d) $(CURDIR) &&) true
