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
