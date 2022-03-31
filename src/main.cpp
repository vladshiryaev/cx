#include <cstdio> 
#include <cstring> 

#include "compiler.h"
#include "builder.h"
#include "lists.h"
#include "dirs.h"
#include "output.h"


const char* path = "";
Builder::Options buildOptions;
StringList runArgs;
bool sanity = false;
bool clean = false;
bool cleanOnly = true;
bool help = false;


void invalidOption(const char* opt) {
    PANIC("Invalid option %s", opt);
}

void printHelp() {
    printf("The 'cx' program allows you to run C/C++ code in the same way you would run\n");
    printf("e.g. Python scripts, without explicit compilation, linking, library packaging.\n");
    printf("It may also be seen as a simple alternative to Make, etc. In simple cases it\n");
    printf("requires zero project configuration (no Makefiles, etc.)\n\n");

    printf("Usage: cx [OPTION]... [[NAME] [ARG]...]\n\n"); 

    printf("Build and execute. If NAME is a directory, there must be exactly one source\n");
    printf("defining main() in that directory. If NAME is a .c/.cpp source, it must define\n");
    printf("main(). OPTIONs before NAME are build options (see below). Everything after\n");
    printf("NAME is passed to the executed program verbatim.\n");
    printf("If NAME is omitted, the current directory will be built, without running.\n\n");

    printf("Options:\n\n");
    printf("-b, --build\n");
    printf("    Build only, don't run. This is the default if NAME is omitted.\n");
    printf("-f, --force\n");
    printf("    Rebuild everything, ignore current cached state.\n");
    printf("--clean\n");
    printf("    Clean build state (delete artifacts directories) recursively, starting\n");
    printf("    with the specied directory (or current directory, if omitted).\n");
    printf("--color=auto|never|always\n");
    printf("    Enable color. By default auto, meaning enabled if stderr is a terminal.\n");
    printf("-q, --quiet\n");
    printf("    Print nothing but errors.\n");
    printf("--verbose\n");
    printf("    Print more. The opposite of --quiet. The last one wins.\n");
    printf("-h, --help\n");
    printf("    Print this summary and exit. Nothing else will be done.\n");
    printf("\n");
}


void parseOptions(const char* args[]) {
     const char* arg;
     for (int i = 1; (arg = args[i]); i++) {
         if (buildOptions.runArgs) {
             buildOptions.runArgs->add(arg);
             continue;
         }
         if (arg[0] == '-') {
             const char* opt = arg + 1;
             if (*opt == '-') {
                 opt++;
             }
             int length = strlen(opt);
             bool ok = false;
             switch (*opt) {
                 case 'h':
                     if (opt[1] == 0 || strcmp(opt, "help") == 0) {
                         help = true;
                         ok = true;
                     }
                     break;
                 case 'q':
                     if (opt[1] == 0 || strcmp(opt, "quiet") == 0) {
                         logLevel = logLevelError;
                         ok = true;
                     }
                     break;
                 case 'v':
                     if (strcmp(opt, "verbose") == 0) {
                         logLevel = logLevelDebug;
                         ok = true;
                     }
                     break;
                 case 'b':
                     if (opt[1] == 0 || strcmp(opt, "build") == 0) {
                         buildOptions.skipRunning = true;
                         cleanOnly = false;
                         ok = true;
                     }
                     break;
                 case 'c':
                     if (strcmp(opt, "clean") == 0) {
                         clean = true;
                         ok = true;
                     }
                     else if (strncmp(opt, "color=", 6) == 0) {
                          if (strcmp(opt + 6, "always") == 0) {
                              setColor(colorAlways);
                          }
                          else if (strcmp(opt + 6, "never") == 0) {
                              setColor(colorNever);
                          }
                          else if (strcmp(opt + 6, "auto") == 0) {
                              setColor(colorAuto);
                          }
                          else {
                              PANIC("Expected: --color=always|never|auto");
                          }
                          ok = true;
                     }
                     break;
                 case 'f':
                     if (opt[1] == 0 || strcmp(opt, "force") == 0) {
                         buildOptions.force = true;
                         cleanOnly = false;
                         ok = true;
                     }
                     break;
                 // Secret. For debugging only.
                 case 's':
                     if (strcmp(opt, "sanity") == 0) { // Run unit tests.
                         sanity = true;
                         cleanOnly = false;
                         ok = true;
                     }
                     break;
                 // Secret. For debugging only.
                 case 'k':
                     if (strcmp(opt, "keep-deps") == 0) { // Keep make dependency files produced by GCC.
                         buildOptions.keepDeps = true;
                         ok = true;
                     }
                     break;
             };
             if (!ok) {
                 invalidOption(arg);
             }

         }
         else if (!buildOptions.runArgs) {
             path = arg;
             buildOptions.runArgs = &runArgs;
         }
     }
     if (runArgs.isEmpty()) {
         buildOptions.runArgs = nullptr;
     }
}

bool doit(const char* argv[]) {
    parseOptions(argv);

    if (help) {
        printHelp();
        return true;
    }
    if (clean) {
        if (!Builder::clean(path)) {
            return false;
        }
        if (cleanOnly) {
            return true;
        }
    }
    if (sanity) {
        extern void test();
        test();
        return true;
    }

    Builder builder;
    builder.options = buildOptions;

    return builder.build(path);
}

int main(int argc, const char* argv[]) {
    bool ok = doit(argv);
    delayedErrorFlush();
    return ok ? 0 : 1;
}

