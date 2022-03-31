#include "config.h"
#include "output.h"
#include "blob.h"
#include "hash.h"
#include "dirs.h"

#include <cstring>


Profile::Profile() {
    commonConfig.profile = this;
    strcpy(id, "default");
    strcpy(version, "unknown");
    strcpy(c, "gcc");
    strcpy(cxx, "g++");
    strcpy(linker, "g++");
    strcpy(librarian, "ar");
    strcpy(symList, "nm");
}


void Profile::init() {
    if (!*c) PANIC("C compiler path cannot be empty"); 
    if (!*cxx) PANIC("C++ compiler path cannot be empty"); 
    if (!*linker) PANIC("Linker path cannot be empty"); 
    if (!*librarian) PANIC("Librarian path cannot be empty"); 
    if (!*symList) PANIC("Symbol list (nm) path cannot be empty"); 
    tag = hash(c) + hash(cxx);
}


inline bool parseSpaces(const char*& p, int& line) {
    const char* start = p;
    for (;;) {
        if (*p == ' ' || *p == '\t') {
            p++;
        }
        else if (*p == '\\') {
            p++;
            if (*p == '\r') {
                p++;
                if (*p == '\n') {
                    p++;
                    line++;
                }
            }
            else if (*p == '\n') {
                p++;
                line++;
            }
            else {
                p--;
                break;
            }
        }
        else {
            break;
        }
    }
    return p != start;
}


static bool parseItem(const char* path, int line, const char*& p, char* out, int& len, bool& error) {
    error = false;
    char quote = 0;
    char* o = out;
    for (;;) {
        if ((unsigned char)(*p) < 32) {
            if (quote) {
                error = true;
                FAILURE("%s:%d: Closing %c expected", path, line, quote);
                return false;
            }
            break;
        }
        else if (*p == '\\') {
            p++;
            if ((unsigned char)(*p) < 32) {
                --p;
                if (quote) {
                    error = true;
                    FAILURE("%s:%d: Closing %c expected", path, line, quote);

                }
                return false;
            }
        }
        else if (*p == quote) {
           p++;
           quote = 0;
           continue;
        }
        else if (*p == '"' || *p == '\'') {
            if (quote) {
                goto store;
            }
            else {
                quote = *p++;
            }
            continue;
        }
        else if (*p == '\'') {
            p++;
            quote = '\'';
            continue;
        }
        else if (*p == ' ' || *p == '\t' || *p == '#') {
            if (!quote) {
                break;
            }
        }
    store:
        if (o - out >= maxPath) {
            error = true;
            FAILURE("%s:%d: String is too long", path, line);
            return false;
        }
        *o++ = *p++;
    }
    len = o - out;
    *o = 0;
    return len > 0;
}


static void skipRestOfLine(const char*& p, int& line) {
    for (; *p; p++) {
        if (*p == '\n') {
            p++;
            line++;
            break;
        }
    }
}


static bool parseEndOfLine(const char*& p, int& line) {
    if (*p == '#') {
        skipRestOfLine(p, line);
        return true;
    }
    else if (*p == '\n') {
        p++;
        line++;
        return true;
    }
    else if (*p == '\r') {
        p++;
        if (*p == '\n') {
            p++;
            line++;
            return true;
        }
    }
    return false;
}


static bool parseId(const char*& p, const char* id, int idLen) {
    if (memcmp(p, id, idLen) == 0) {
        char c = p[idLen];
        if ((unsigned char)(c) < 32 || c == ' ' || c == '\t' || c == ':') {
            p += idLen;
            return true;
        }
    }
    return false;
}


static bool parseColon(const char* path, int& line, const char*& p) {
    parseSpaces(p, line);
    if (*p == ':') {
        p++;
    }
    else {
        FAILURE("%s:%d: Expected :", path, line);
        return false;
    }
    parseSpaces(p, line);
    return true;
}


static bool parseList(const char* path, int& line, const char*& p, StringList& list) {
    if (!parseColon(path, line, p)) {
        return false;
    }
    char item[maxPath];
    int length;
    bool error;
    while (parseItem(path, line, p, item, length, error)) {
        list.add(item, length);
        if (!parseSpaces(p, line)) {
            break;
        }
    }
    return !error && parseEndOfLine(p, line);
}


static bool parseValue(const char* path, int& line, const char*& p, char* value, int& length) {
    bool error;
    return parseColon(path, line, p) && (parseItem(path, line, p, value, length, error) || !error) && parseEndOfLine(p, line);
}


bool Config::load(const char* path) {
    Blob text;
    if (!text.load(path)) {
        afterParse();
        return true;
    }
    return parse(path, text.data);
}


bool Config::parse(const char* path, const char* text) {
    #define PROFILE_ONLY if (!profile) goto not_profile
    #define PARSE_VALUE(WHAT)  if (!parseValue(path, line, p, WHAT, length)) goto error
    #define PARSE_LIST(WHAT)  if (!parseList(path, line, p, WHAT)) goto error

    int line = 1;
    const char* p = text;
    int length;
    for (;;) {
        switch (*p) {
            case 0:
                goto done;
            case ' ':
            case '\t':
                parseSpaces(p, line);
                break;
            case '#':
                skipRestOfLine(p, line);
                break;
            case '\r':
                p++;
                if (*p == '\n') {
                    p++;
                }
                else {
                    FAILURE("%s:%d: Unexpected CR character", path, line);
                    goto error;
                }
                line++;
                break;
            case '\n':
                p++;
                line++;
                break;
            case 'a':
                if (parseId(p, "ar", 2)) {
                    PROFILE_ONLY;
                    PARSE_VALUE(profile->librarian);
                }
                else {
                    goto other;
                }
                break;
            case 'c':
                if (parseId(p, "cc_options", 10)) {
                    PARSE_LIST(compilerOptions);
                }
                else if (parseId(p, "c_options", 9)) {
                    PARSE_LIST(compilerCOptions);
                }
                else if (parseId(p, "cxx_options", 11)) {
                    PARSE_LIST(compilerCppOptions);
                }
                else {
                    goto other;
                }
                break;
            case 'g':
                if (parseId(p, "gcc", 3)) {
                    PROFILE_ONLY;
                    PARSE_VALUE(profile->c);
                }
                else if (parseId(p, "g++", 3)) {
                    PROFILE_ONLY;
                    PARSE_VALUE(profile->cxx);
                    memcpy(profile->linker, profile->cxx, length + 1);
                }
                else {
                    goto other;
                }
                break;
            case 'i':
                if (parseId(p, "include_path", 12)) {
                    StringList temp;
                    PARSE_LIST(temp);
                    char dir[maxPath];
                    char name[maxPath];
                    splitPath(path, dir, name);
                    char absIncPath[maxPath];
                    for (StringList::Iterator i(temp); i; i.next()) {
                        includeSearchPath.add(rebasePath(dir, i->string, absIncPath));
                        TRACE("Include path: %s", absIncPath);
                    }
                }
                else {
                    goto other;
                }
                break;
            case 'l':
                if (parseId(p, "ld_options", 10)) {
                    PARSE_LIST(linkerOptions);
                }
                else {
                    goto other;
                }
                break;
            case 'n':
                if (parseId(p, "nm", 2)) {
                    PROFILE_ONLY;
                    PARSE_VALUE(profile->symList);
                }
                else {
                    goto other;
                }
                break;
            case 'e':
                if (parseId(p, "external_libs", 13)) {
                    PARSE_LIST(externalLibs);
                }
                else {
                    goto other;
                }
                break;
            default:
                other:
                FAILURE("%s:%d: Invalid directive", path, line);
                goto error;

        }
    }
done:
    afterParse();
    return true;
error:
    return false;
not_profile:
    FAILURE("%s:%d: This is allowed in cx.top only", path, line);
    return false;

    #undef PROFILE_ONLY
    #undef PARSE_VALUE
    #undef PARSE_LIST
}


void Config::afterParse() {
    uint32_t compTag = getStringListHash(compilerOptions);
    cOptionsTag = compTag + getStringListHash(compilerCOptions);
    cxxOptionsTag = compTag + getStringListHash(compilerCppOptions);
    linkerOptionsTag = getStringListHash(linkerOptions) + getStringListHash(externalLibs);
}


