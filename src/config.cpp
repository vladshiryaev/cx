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


inline const char* skipSpaces(const char* p, int& line) {
    for (;;) {
        if (*p == ' ' || *p == '\t') {
            p++;
        }
        else if (*p == '\\') {
            p++;
            if (*p == '\r') {
                p++;
                if (*p == '\n') {
                    line++;
                }
                else {
                    return p - 2;
                }
            }
            else if (*p == '\n') {
                p++;
                line++;
            }
            else {
                return p - 1;
            }
        }
        else {
            break;
        }
    }
    return p;
}


inline const char* skipLine(const char* p) {
    for (; *p; p++) {
        if (*p == '\n') {
            return ++p;
        }
    }
    return p;
}


static bool parseColon(const char* path, int& line, const char*& p) {
    p = skipSpaces(p, line);
    if (*p == ':') {
        p++;
    }
    else {
        FAILURE("%s:%d: Expected :", path, line);
        return false;
    }
    p = skipSpaces(p, line);
    return true;
}


static bool parseItem(const char* path, int line, const char*& p, char* out, int& len) {
    char* o = out;
    for (;;) {
        switch (*p) {
            case '#':
                return false;
            case '"':
                p++;
                for (;;) {
                    char c = *p++;
                    if (c == '"') {
                        len = o - out;
                        *o = 0;
                        return true;
                    }
                    if (c == '\\') {
                        c = *p++;
                    }
                    if ((unsigned char)(c) < 32) {
                        --p;
                        FAILURE("%s:%d: Expected closing \"", path, line, c);
                        return false;
                    }
                    if (o - out >= maxPath) {
                        FAILURE("%s:%d: String is too long", path, line);
                        return false;
                    }
                    *o++ = c;
                }
                break;
            case '\'':
                p++;
                for (;;) {
                    char c = *p++;
                    if (c == '\'') {
                        len = o - out;
                        *o = 0;
                        return true;
                    }
                    if (c == '\\') {
                        c = *p++;
                    }
                    if ((unsigned char)(c) < 32) {
                        --p;
                        FAILURE("%s:%d: Expected closing \"", path, line, c);
                        return false;
                    }
                    if (o - out >= maxPath) {
                        FAILURE("%s:%d: String is too long", path, line);
                        return false;
                    }
                    *o++ = c;
                }
                break;
            default:
                while ((unsigned char)(*p) > 32 && *p != '#') {
                    if (*p == '\\') {
                        p++;
                        if ((unsigned char)(*p) < 32) {
                            --p;
                            return false;
                        }
                    }
                    if (o - out >= maxPath) {
                        FAILURE("%s:%d: String is too long", path, line);
                        return false;
                    }
                    *o++ = *p++;
                }
                len = o - out;
                *o = 0;
                return len > 0;
        }
    }
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


static bool parseList(const char* path, int& line, const char*& p, StringList& list) {
    if (!parseColon(path, line, p)) {
        return false;
    }
    char item[maxPath];
    int length;
    while (parseItem(path, line, p, item, length)) {
        //say("item %s (%d)\n", item, length);
        list.add(item, length);
        p = skipSpaces(p, line);
    }
    return true;
}


static bool parseValue(const char* path, int& line, const char*& p, char* value, int& length) {
    return parseColon(path, line, p) && parseItem(path, line, p, value, length);
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
                p = skipSpaces(++p, line);
                break;
            case '#':
                p = skipLine(++p);
                line++;
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
                        includeSearchPath.add(rebasePath(dir, i->data, absIncPath));
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


