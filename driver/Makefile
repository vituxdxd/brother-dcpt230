CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: t230_encode ppd

t230_encode: t230_encode.c
	$(CC) $(CFLAGS) -o $@ $<

ppd: papers.tsv make_ppd.py
	python3 make_ppd.py

clean:
	rm -f t230_encode

.PHONY: all ppd clean
