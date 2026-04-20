.PHONY: all c clean u upload m monitor snapshot s

all:
	pio run

clean:
	pio run -t clean

c: clean

upload:
	pio run -t upload

u: upload

monitor:
	pio device monitor --no-reconnect 

m: monitor

snapshot:
	python ./snapshot.py

s: snapshot