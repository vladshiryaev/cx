# CX

The `cx` program allows you to run C/C++ code in the same way you would run
e.g. Python scripts, without explicit compilation, linking, library packaging.
It may also be seen as a simple alternative to Make, etc. In simple cases it
requires zero project configuration.

## Running

`Usage: cx [OPTION]... [[NAME] [ARG]...]`

Build and execute. If `NAME` is a directory, there must be exactly one source
defining `main()` in that directory. If `NAME` is a .c/.cpp source, it must define
`main()`. `OPTION`s before `NAME` are build options (see below). Everything after
`NAME` is passed to the executed program verbatim.
If `NAME` is omitted, the current directory will be built, without running.

Examples:

```
cx hello_world.cpp
cx my_program_directory/  --my_arg -a -b -c
```

The program may contain multiple source files, in multiple directories. As long as you
follow some simple rules below, no special configuration is needed. But you may also add
`cx.unit` and/or `cx.top` files to configure some things like compilation options and
dependencies.

Only things that have changed since last invocation will be be recompiled.


## Options

|Option       |Description |
|-------------|--------------------------------------------------------------|
|`-b, --build`|Build only, don't run. This is the default if NAME is omitted.|
|`-f, --force`|Rebuild everything, ignore current cached state.|
|`--clean`    |Clean build state (delete artifacts directories) recursively, starting with the specied directory (or current directory, if omitted).|
|`-q, --quiet`|Print nothing but errors.|
|`--verbose`  |Print more. The opposite of --quiet. The last one wins.|
|`-h, --help` |Print this summary and exit. Nothing else will be done.|


## Building

In order to build `cx` just run `./build` script in the source directory. It's a one-liner bash script that
will do a bootstrapping build of `cx` without help from `make` and such, and copy the
binary to `/usr/bin`.

From now on you can build `cx` using `cx`. Just type `cx` in the source directory
then move `.cx.cache/main.cpp.o.exe` to, e.g., `/usr/bin/cx`.

## Source structure

### Units

Programs built by `cx` consist of *units*.

A unit is a flat directory containing a number of .c/.cpp/.h files.

Units are always compiled and archived into libraries in the cache directory, except sources that contain `main()` function. Those are used only for running.

A unit may use other units. The rule is simple: if unit A cludes something from unit B, then A depens on B, which means B needs to be compiled and linked in whenever a program needs A. Dpendency is transitive.

### Include search path

## unit configiration

... `cx.unit` ....
```
cc_options
c_options
cxx_options
ld_options
include_path
external_libs
```


Example:
```
cxx_options: -O0 -g

```

### Source tree top directory

... `cx.top` ....

Example:
```
gcc: /usr/bin/gcc-7
g++: /usr/bin/g++-7
ar: /usr/bin/gcc-ar-7
nm: /usr/bin/gcc-nm-7

include_path: common/parsing common/util

```
## Limitations

