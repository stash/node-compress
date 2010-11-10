#!/bin/sh
#
# Author: Ivan Egorov (egorich.3.04@gmail.com).
#

node-waf configure $*
node-waf build
cp ./build/default/compress-bindings.node ./lib/compress
