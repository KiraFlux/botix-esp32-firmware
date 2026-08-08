.PHONY: snapshot s

include ./.pio/libdeps/esp32dev/KiraFlux-Toolkit/examples/common.mak

snapshot:
	python snapshot.py

s: snapshot