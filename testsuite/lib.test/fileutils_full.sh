#!/bin/sh

BASEDIR=$(dirname ${0})
${BASEDIR}/../../lib/test_fileutils > /dev/full
