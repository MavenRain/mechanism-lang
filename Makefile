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
# Checks that take the manifest arguments (dev/bend2/test-manifest.json) on the command line.
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
C_PRELUDE_CHECKS := prelude_category prelude_functor prelude_heterogeneous_functor prelude_heterogeneous_nattrans prelude_heterogeneous_whiskering prelude_shared_nattrans prelude_nattrans_laws surface_family_poly
C_PRELUDE_BINARIES := $(addprefix _tcc/,$(C_PRELUDE_CHECKS))
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
# dev/bend2-io-check.py in c-test (its oracle calls ran in a temporary directory that no longer exists).
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
# (the corpus kanon label) and bend2/tests/cli_core_check.py gate it outside c-test. The kanon drivers go to
# _tcc/bin/ as the Bend binaries go to _bend2/bin/: Host.root (bend2/cli/host.bend) is the third dirname of
# the executable, and kanon run finds dev/run-node.mjs and dev/run-wasmtime.sh under that root.
C_KANON_CLI := cli_core
C_KANON_BINARIES := $(addprefix _tcc/bin/,$(C_KANON_CLI))
$(C_KANON_BINARIES): _tcc/bin/%: c/tests/%.c $(C_CLI_KANON) $(C_TEST_HEADERS) $(C_CLI_SOURCES) $(C_CLI_HEADERS) $(C_WASM_SOURCES) $(C_WASM_HEADERS) $(C_SURFACE_SOURCES) $(C_SURFACE_HEADERS) $(C_KERNEL_SOURCES) $(C_KERNEL_HEADERS) $(C_RT_SOURCES) $(C_RT_HEADERS)
	@mkdir -p _tcc/bin
	$(TCC) $(CFLAGS) -Wall -Werror -I c/rt -I c/kernel -I c/surface -I c/wasm -I c/cli -o $@ $< $(C_CLI_KANON) $(C_CLI_SOURCES) $(C_WASM_SOURCES) $(C_SURFACE_SOURCES) $(C_KERNEL_SOURCES) $(C_RT_SOURCES) $(LDFLAGS) -lpthread
# mech_cert (bend2/mech_cert.bend) is c/cli/mech_cert.c over c/cli (cert.c) + surface + kernel. c-build builds
# it to _tcc/bin/ beside the kanon drivers; dev/bend2-cli-check.py --cert _tcc/bin/mech_cert gates its 13
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
.PHONY: c-build c-test c-test-prelude
c-build: _tcc/rt_test _tcc/rt_regression_test _tcc/rt_oom_test $(C_KERNEL_BINARIES) $(C_KERNEL_FAULT_BINARIES) $(C_SURFACE_BINARIES) $(C_LEVELS_BINARIES) $(C_IMPORT_BINARIES) $(C_WASM_BINARIES) $(C_IO_BINARIES) $(C_CLI_FAULT_BINARIES) $(C_IMPORT_IO_BINARIES) $(C_CLI_WASM_BINARIES) $(C_KANON_BINARIES) $(C_CERT_BINARIES) $(C_CLI_RUNTIME_BINARIES) $(C_RELATIONAL_IO_BINARIES) $(C_SURFACE_LIBS) _tcc/runtime_reachable.o _tcc/relational_syntax_equal.o $(C_LEVELS_LIBS) $(C_RELATIONAL_LIBS) $(C_PRELUDE_BINARIES) $(C_RUST_OBJECTS) $(C_CLI_RUST_OBJECTS) _tcc/rust_emit _tcc/rust_frontend _tcc/rust_let _tcc/rust_import

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
c-build: $(C_RUST_FAULT_BINARIES)

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
c-build: $(C_MECH_BINARIES)

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
	$(foreach d,$(C_LEVELS_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_IMPORT_CHECKS),./_tcc/$(d) &&) true
	$(foreach d,$(C_WASM_CHECKS),./_tcc/$(d) &&) true
	$(PYTHON) -P dev/bend2-io-check.py --driver _tcc/io_driver
	$(foreach d,$(C_CLI_FAULT_CHECKS),./_tcc/$(d) &&) true
	./_tcc/prelude_runtime --slice-self-test
	./_tcc/template_cost prelude/cat/category.mech --through Hom > /dev/null
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
# MEMCAP_SECS_<driver> override (prelude_shared_nattrans instantiates 42 entries and needs about 245 s), SIGKILL
# at either cap. memcap exits with the driver's status. c-test-prelude is not a c-test dependency until every
# listed driver measures under the cap.
# Six ported drivers live in c/tests but are not in C_PRELUDE_CHECKS: each one is killed under memcap 3500 120
# inside a single compact entry (the two traced drivers die in top-level declaration 7), so sharding the entry
# list does not help. Kill figures (footMB = phys_footprint at the kill, wall seconds):
#   prelude_horizontal_associativity  memory  foot 3504 MB   26.6 s
#   prelude_horizontal_laws           memory  foot 3522 MB   26.7 s
#   prelude_iterated_whiskering       time    foot 2714 MB  120.1 s (still rising)
#   prelude_whiskering_laws           memory  foot 3519 MB   32.6 s
#   prelude_shared_left_kan           memory  foot 3541 MB   30.2 s
#   prelude_heterogeneous_left_kan    memory  foot 3506 MB   62.0 s
# Build one with `make C_PRELUDE_CHECKS=prelude_<x> _tcc/prelude_<x>`; a driver joins C_PRELUDE_CHECKS once the
# compact path splits that entry per member and it measures under the cap. Nine bend2/tests/prelude_*.bend
# drivers (composable_functors, nattrans, nattrans_units, left_kan, left_kan_laws, left_kan_cocone_action,
# left_kan_mediator_equality, left_kan_roundtrip, left_kan_solution_transport) have no C port yet.
MEMCAP ?= /Users/oobi/Documents/mechanism-lang-tcc/memcap
MEMCAP_MB ?= 3500
MEMCAP_SECS ?= 120
MEMCAP_SECS_prelude_shared_nattrans := 600
c-test-prelude: $(C_PRELUDE_BINARIES)
	$(foreach d,$(C_PRELUDE_CHECKS),$(MEMCAP) $(MEMCAP_MB) $(or $(MEMCAP_SECS_$(d)),$(MEMCAP_SECS)) ./_tcc/$(d) $(CURDIR) &&) true
