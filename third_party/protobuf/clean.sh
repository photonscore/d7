#!/bin/sh

cd $( cd "$(dirname "$0")" ; pwd -P )
rm -rf `find src -name '*_unittest.*'`
rm -rf `find src -name '*_test.*'`
rm -rf `find src -name '*_test_util.*'`
rm -rf `find src -name 'test_util*'`
rm -rf `find src -name 'testdata'`
rm -rf `find src -name 'testing'`
rm -rf `find src -name 'unittest*'`
rm -rf `find src -name 'test*'`
rm -rf `find src -name '*_tester.*'`
rm -rf `find src -name '*test_helper*'`
rm -rf `find src -name '*.sh'`
rm -rf `find src -name 'ruby_generated_*'`
rm -rf src/*.map src/Makefile* src/README.md
