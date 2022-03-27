# CX

The 'cx' program allows you to run C/C++ code in the same way you would run
e.g. Python scripts, without explicit compilation, linking, library packaging.
It may also be seen as a simple alternative to Make, etc. In simple cases it
requires zero project configuration.

`Usage: cx [OPTION]... [[NAME] [ARG]...]`

Build and execute. If NAME is a directory, there must be exactly one source
defining main() in that directory. If NAME is a .c/.cpp source, it must define
main(). OPTIONs before NAME are build options (see below). Everything after
NAME is passed to the executed program verbatim.
If NAME is omitted, the current directory will be built, without running.

Options:

|Option       |Description |
|-------------|--------------------------------------------------------------|
|`-b, --build`|Build only, don't run. This is the default if NAME is omitted.|
|`-f, --force`|Rebuild everything, ignore current cached state.|
|`--clean`    |Clean build state (delete artifacts directories) recursively, starting with the specied directory (or current directory, if omitted).|
|`-q, --quiet`|Print nothing but errors.|
|`--verbose`  |Print more. The opposite of --quiet. The last one wins.|
|`-h, --help` |Print this summary and exit. Nothing else will be done.|
