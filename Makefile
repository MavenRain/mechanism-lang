.PHONY: build auction-check auction-check-all auction-gpu-check

PYTHON ?= python3

build:
	zsh dev/dunecho.sh build

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

bend2-reference-check:
	$(PYTHON) -P dev/bend2-reference-check.py

bend2-reference-test:
	$(PYTHON) -P dev/bend2-reference-test.py --reference "$(CURDIR)"
	$(PYTHON) -P dev/bend2-equality-runtime-test.py --reference "$(CURDIR)"

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
