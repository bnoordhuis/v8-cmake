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

maintainers
===========

To update V8 to a new patch release:

    $ python update_v8.py
    # <snip>
    4dc61d3cd0 (tag: 8.5.210.20, refs/branch-heads/8.5) Version 8.5.210.20

    $ git add .

    $ git commit -am 8.5.210.20

    $ git tag 8.5.210.20

To update V8 to a new minor or major release, open `update_v8.json` in an
editor and update the `"branch"` field, then run `update_v8.py`.

The diff for `update_v8.json` should look like this:
```diff
diff --git a/update_v8.json b/update_v8.json
index 9d4a79ba..eb1f89f0 100644
--- a/update_v8.json
+++ b/update_v8.json
@@ -3,7 +3,7 @@
     "url": "https://chromium.googlesource.com/v8/v8.git",
     "commit": "4dc61d3cd02f0a2462cc655095db1e99ad9047d2",
     "/* comment */": "Dependency v8 must be first.",
-    "branch": "branch-heads/8.5",
+    "branch": "branch-heads/8.6",
     "path": "v8"
   },
   {
```
