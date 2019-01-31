#!/bin/bash
#
# Wrapper script to execute PIN with the AvDark cache simulator
# module.
#
# Course: Advanced Computer Architecture, Uppsala University
# Course Part: Lab assignment 1
#
# Author: Andreas Sandberg <andreas.sandberg@it.uu.se>
# Revision (2015, 2016, 2017, 2018): German Ceballos, Johan Janzen, Chris Sakalis
#

. FRAMEWORK

SCRIPT_DIR=$(dirname $(readlink -f $0))

COURSE_DIR=$SCRIPT_DIR

PIN_HOME=$CACHE_FRAMEWORK/pin
PIN=${PIN_HOME}/pin

TOOL="${SCRIPT_DIR}/obj-intel64/avdark-cache.so"

if [ ! -e "${TOOL}" ]; then
    echo "Can't find the PIN module for the AvDark cache simulator." 1>&2
    echo "Make sure that you built the PIN module prior to using this script." 1>&2
    exit 1
fi

if [ ! -x "${PIN}" ]; then
    echo "Can't find PIN or the pin binary isn't executable" 1>&2
    exit 1
fi

$PIN -t "${TOOL}" $*
