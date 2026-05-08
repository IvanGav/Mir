// This will be my commandline argument library from now on

#include "core/prelude.h"
#include "core/mem.h"
#include "core/str.h"

Str readFile(const char* path, mem::Arena& arena = default_arena) {
    std::ifstream infile(path);
    return str::clone_cstr(std::string(std::istreambuf_iterator<char>(infile), std::istreambuf_iterator<char>()).data(), arena);
}

void writeFile(const char* path, Str content) {
    std::ofstream outfile(path);
    outfile << content;
}

// return 0 if not found
usize index_of(const char* const argv[], const char* fullname, char shortname) {
    usize fullname_size = strlen(fullname);
    for(usize i = 1; argv[i] != nullptr; i++) {
        usize size = strlen(argv[i]);
        if(size == 2 && argv[i][0] == '-' && argv[i][1] == shortname) return i;
        if(size == fullname_size+2 && argv[i][0] == '-' && argv[i][1] == '-' && strcmp(argv[i]+2, fullname) == 0) return i;
    }
    return 0;
}

bool has_flag(const char* const argv[], const char* fullname, char shortname) {
    return index_of(argv, fullname, shortname) != 0;
}

bool has_int_arg(const char* const argv[], const char* fullname, char shortname) {
    usize i = index_of(argv, fullname, shortname);
    if(i == 0 || argv[i+1] == nullptr) return false;
    return true;
}

u32 get_int_arg(const char* const argv[], const char* fullname, char shortname) {
    usize i = index_of(argv, fullname, shortname);
    return atoi(argv[i+1]);
}

bool has_str_arg(const char* const argv[], const char* fullname, char shortname) {
    usize i = index_of(argv, fullname, shortname);
    if(i == 0 || argv[i+1] == nullptr) return false;
    return true;
}

const char* get_str_arg(const char* const argv[], const char* fullname, char shortname) {
    usize i = index_of(argv, fullname, shortname);
    return argv[i+1];
}

u32 get_int_arg_or_default(const char* const argv[], const char* fullname, char shortname, u32 default_val) {
    if(has_int_arg(argv, fullname, shortname)) { return get_int_arg(argv, fullname, shortname); }
    return default_val;
}

const char* get_str_arg_or_default(const char* const argv[], const char* fullname, char shortname, const char* default_val) {
    if(has_str_arg(argv, fullname, shortname)) { return get_str_arg(argv, fullname, shortname); }
    return default_val;
}