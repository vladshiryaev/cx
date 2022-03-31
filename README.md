# CX

The `cx` program allows you to run C/C++ code in the same way you would run
e.g. Python scripts, without explicit compilation, linking, library packaging.
It may also be seen as a simple alternative to Make, etc. In simple cases it
requires zero project configuration.

## Running

`cx [OPTION]... [[NAME] [ARG]...]`

Build and execute `NAME`. If `NAME` is a directory, there must be exactly one source
defining `main()` in that directory. If `NAME` is a .c/.cpp source, it must define
`main()`. `OPTION`s before `NAME` are build options (see below). Everything after
`NAME` is passed to the executed program verbatim.
If `NAME` is omitted, the current directory will be built, without running.

Examples:

```
cx hello_world.cpp
cx path/my_program_directory/  --my_arg -a -b -c
```

The program may contain multiple source files, in multiple directories. As long as you
follow some simple rules below, no special configuration is needed. But you may also add
`cx.unit` and/or `cx.top` files to configure some things like compilation options and
dependencies.

Only things that have changed since last invocation will be be recompiled.


## Options

|Option                      |Description |
|----------------------------|--------------------------------------------------------------|
|`-b, --build`               |Build only, don't run. This is the default if NAME is omitted.|
|`-f, --force`               |Rebuild everything, ignore current cached state.|
|`--clean`                   |Clean build state (delete artifacts directories) recursively, starting with the specied directory (or current directory, if omitted).|
|`--color=always,never,auto` |Enable color. `Auto` is the default and it means enabled if stderr is a terminal.|
|`-q, --quiet`               |Print nothing but errors.|
|`--verbose`                 |Print more. The opposite of --quiet. The last one wins.|
|`-h, --help`                |Print this summary and exit. Nothing else will be done.|


## Building

In order to build `cx` just run `./build` script in the source directory. It's a one-liner bash script that
will do a bootstrapping build of `cx` (without help from `make` and such), and copy the
binary to `/usr/bin`.

From now on you can build `cx` using `cx`. Just type `cx` in the source directory
then copy `.cx.cache/main.cpp.o.exe` to, e.g., `/usr/bin/cx`.

## Source structure

### Units

Programs built by `cx` consist of *units*.

A unit is a flat directory containing a number of .c/.cpp/.h files.

Units are always compiled and archived into libraries in the cache directory, except sources that contain `main()` function. Those are used only for running.

A unit may use other units. The rule is simple: if unit A #includes something from unit B, then A depends on B, which means B needs to be compiled and linked in whenever A is needed. Unit dependencies are transitive, so you don't have to explicitly specify sub-dependencies.

### Include search path

Unit A may include headers from unit B relative to the common path of A and B.

E.g. `src/foo/a/something.cpp` may include `src/foo/b/header.h` as `#include "b/header.h"`. In other words, you can drop leading `../` path elements. This should not require any configuration. Alternatively, you can use `include_path` in `cx.unit` or `cx.top`.

### Unit configiration

Optionally, there may be file called `cx.unit` in unit's directory. It may contain something like this:

```
cc_options: -O0 -g -Dfoo=bar
c_options: -std=c18
cxx_options: -std=c++17 -faligned-new
ld_options:
external_libs: -lz -L/home/jsmith/shelf_libs/ -lfoo -lbar /home/jsmith/other_libs/libxxx.a
include_path: submodules ../../common/util ../../common/funcs/

```
The values are in about the same format you would specify args in GCC's command line.

| Parameter     | Meaning |
|---------------|---------|
|`cc_options`   | Common to C and C++ |
|`c_options`    | C only |
|`cxx_options`  | C++ only |
|`ld_options`   | Linker (note, invoked as gcc or g++) |
|`external_libs`| Goes to the end of linker command line. May contain a mix of exact library/object paths, `-L<dir>`, `-l<id>`. Note, these libraries are not checked for changes, but dependency on them is transitive (if unit B needs them, then unit A using unit B also needs them). |
|`include_path` | List of include paths. Relative paths are are interpreted as relative to the directory in which this configuration file is located. |

Note: You probably should not use `cx.unit` in unit directory, and put most of common parameters in `cx.top` instead.


### Source tree top directory

You may put a file named `cx.top` at the top of your source tree. It may contain the same parameters as `cx.unit` (and most of them should probably be here instead of `cx.unit`). Besides, it may contain something like this:

```
gcc: /usr/bin/gcc-7
g++: /usr/bin/g++-7
ar: /usr/bin/gcc-ar-7
nm: /usr/bin/gcc-nm-7

```
Those are exact names by which compiler etc. will be invoked. Note, only GCC is currently supported, and whatever program is specified as, e.g., `g++` must behave exactly as `g++` does.

## Limitations

