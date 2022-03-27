# CX

The 'cx' program allows you to run C/C++ code in the same way you would run
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
