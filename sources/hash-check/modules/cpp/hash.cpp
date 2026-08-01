#include "sha256.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage(const char* program) {
    std::fprintf(stderr,
        "Usage: %s [options] [file]\n"
        "\n"
        "Hash input with SHA-256 and print the digest as lowercase hex.\n"
        "\n"
        "Options:\n"
        "  -s <text>   Hash a literal string\n"
        "  -c <hex>    Verify input against a 64-character hex digest\n"
        "  -h          Show this help\n"
        "\n"
        "With no options, reads from <file> or stdin when no file is given.\n",
        program);
}

bool read_file(const char* path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "error: cannot open '%s'\n", path);
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size < 0) {
        std::fprintf(stderr, "error: cannot read '%s'\n", path);
        return false;
    }
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    if (size > 0 && !file.read(reinterpret_cast<char*>(out.data()), size)) {
        std::fprintf(stderr, "error: cannot read '%s'\n", path);
        return false;
    }
    return true;
}

bool read_stdin(std::vector<uint8_t>& out) {
    std::vector<uint8_t> buffer(4096);
    while (std::cin) {
        std::cin.read(reinterpret_cast<char*>(buffer.data()),
                      static_cast<std::streamsize>(buffer.size()));
        std::streamsize got = std::cin.gcount();
        if (got > 0) {
            out.insert(out.end(), buffer.begin(), buffer.begin() + got);
        }
    }
    return !std::cin.bad();
}

} // namespace

int main(int argc, char* argv[]) {
    const char* string_arg = nullptr;
    const char* check_hex = nullptr;
    const char* file_arg = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg == "-s") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: -s requires an argument\n");
                return 1;
            }
            string_arg = argv[++i];
            continue;
        }
        if (arg == "-c") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: -c requires an argument\n");
                return 1;
            }
            check_hex = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "error: unknown option '%s'\n", arg.c_str());
            print_usage(argv[0]);
            return 1;
        }
        if (file_arg != nullptr) {
            std::fprintf(stderr, "error: only one input file is allowed\n");
            return 1;
        }
        file_arg = argv[i];
    }

    std::vector<uint8_t> data;
    if (string_arg != nullptr) {
        const char* bytes = string_arg;
        data.assign(bytes, bytes + std::strlen(bytes));
    } else if (file_arg != nullptr) {
        if (!read_file(file_arg, data)) {
            return 1;
        }
    } else if (check_hex == nullptr) {
        if (!read_stdin(data)) {
            std::fprintf(stderr, "error: failed to read stdin\n");
            return 1;
        }
    } else {
        std::fprintf(stderr, "error: -c requires input from a file, string, or stdin\n");
        return 1;
    }

    hashcheck::Sha256Digest digest = hashcheck::sha256(data.data(), data.size());
    std::string hex = hashcheck::digest_to_hex(digest);

    if (check_hex != nullptr) {
        bool ok = hashcheck::verify_hex(data.data(), data.size(), check_hex);
        if (!ok) {
            std::fprintf(stderr, "mismatch: expected %s, got %s\n", check_hex, hex.c_str());
            return 1;
        }
        std::printf("%s  OK\n", hex.c_str());
        return 0;
    }

    std::printf("%s\n", hex.c_str());
    return 0;
}
