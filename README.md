![](https://github.com/bnoordhuis/v8-cmake/workflows/v8-cmake/badge.svg)

A port of the [V8 JavaScript engine](https://v8.dev/) to the
[CMake](https://cmake.org/) build system.

motivation
==========

V8 natively uses [GN](https://gn.googlesource.com/gn/) to build itself, which
is part of the Chromium ecosystem but pretty much not used outside of it.

CMake is a widely used build system for C and C++ projects. This port will
hopefully make it easier to integrate V8 with third-party projects.

Caveat emptor: v8-cmake is very much a work in progress. Bug reports and
pull requests welcome!

build
=====

To build the library and the `d8` shell:

    $ mkdir build && cd build

    $ cmake /path/to/v8-cmake

    $ make -j8

If CMake complains about `python` being incompatible or too old:

    $ cmake -DPYTHON_EXECUTABLE=/usr/bin/python3 /path/to/v8-cmake


debug
===============

- [Debug in VSCode](docs/debug-in-vscode.md)
