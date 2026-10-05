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
