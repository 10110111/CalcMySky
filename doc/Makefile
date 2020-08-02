MAKEFILE_DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

data-deps.svg: data-dependencies.dot $(MAKEFILE_DIR)Makefile
	dot -Tsvg data-dependencies.dot > data-deps.svg

data-dependencies.dot: $(MAKEFILE_DIR)data-dependencies.dot.m4 $(MAKEFILE_DIR)Makefile
	m4 $(MAKEFILE_DIR)data-dependencies.dot.m4 > data-dependencies.dot
