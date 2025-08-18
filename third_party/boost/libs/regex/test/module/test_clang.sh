#!/bin/sh

#
# Builds all of the tests in this folder using clang modules
#
: ${CXX:="clang++"}
: ${CXXFLAGS:="-std=c++20 -g -I../../../.."}

rm *.o *.pcm *.exe

cmd="$CXX $CXXFLAGS -x c++-module ../../module/regex.cxx --precompile -o boost.regex.pcm"
echo $cmd
$cmd || exit 1
cmd="$CXX $CXXFLAGS boost.regex.pcm -c -o regex.o"
echo $cmd
$cmd || exit 1
cmd="$CXX $CXXFLAGS -fprebuilt-module-path=. -c ../../module/*.cpp"
echo $cmd
$cmd || exit 1


for file in *.cpp; do

cmd="$CXX $CXXFLAGS -fprebuilt-module-path=. $file *.o -o ${file%.*}.exe"
echo $cmd
$cmd || exit 1

done
