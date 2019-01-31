#
# Main Makefile for the AvDark cache simulator.
#
# Course: Advanced Computer Architecture, Uppsala University
# Course Part: Lab assignment 1
#
# Author: Andreas Sandberg <andreas.sandberg@it.uu.se>
# Revision (2015, 2016, 2017, 2018): German Ceballos, Johan Janzen, Chris Sakalis
#

include FRAMEWORK
PIN_ROOT = $(CACHE_FRAMEWORK)/pin

CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
include $(CONFIG_ROOT)/makefile.config
include makefile.rules
include $(TOOLS_ROOT)/Config/makefile.default.rules

test: test-direct test-assoc
test-direct: test0
	@echo "**************************************************"
	@echo "* Running tests for direct mapped caches         *"
	@echo "**************************************************"
	./test0 > /dev/null

test-assoc: test1
	@echo "**************************************************"
	@echo "* Running tests for associative caches           *"
	@echo "**************************************************"
	./test1 > /dev/null

test0: test0.o avdark-cache.o
test1: test1.o avdark-cache.o

all: test0 test1

clean: clean-test

clean-test:
	$(RM) -f test0 test1 test0.o test1.o avdark-cache.o
