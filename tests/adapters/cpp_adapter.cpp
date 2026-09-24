#include "json_core.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

// Test-only bridge to the existing C++ implementation. No JSON parser lives here.
int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: cpp-adapter INPUT FORMAT object|any true|false|preserve\n";
        return 2;
    }
    const std::string format = argv[2];
    const std::string root_mode = argv[3];
    const std::string newline_mode = argv[4];
    if ((format != "compact" && format != "twoSpaces" && format != "fourSpaces" &&
         format != "tabs" && format != "detect") ||
        (root_mode != "object" && root_mode != "any") ||
        (newline_mode != "true" && newline_mode != "false" && newline_mode != "preserve")) {
        std::cerr << "invalid adapter option\n";
        return 2;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 2;
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) return 2;
#endif
    std::ifstream file;
    if (std::string(argv[1]) != "-") file.open(argv[1], std::ios::binary);
    std::istream& input = std::string(argv[1]) == "-" ? std::cin : file;
    if (!input) {
        std::cerr << "adapter input read failed\n";
        return 2;
    }
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) {
        std::cerr << "adapter input read failed\n";
        return 2;
    }
    try {
        const auto root = jsondict::parse(text);
        auto formatting = jsondict::Formatting::Compact;
        if (format == "twoSpaces") formatting = jsondict::Formatting::TwoSpaces;
        if (format == "fourSpaces") formatting = jsondict::Formatting::FourSpaces;
        if (format == "tabs") formatting = jsondict::Formatting::Tabs;
        if (format == "detect") formatting = jsondict::detect_formatting(text);
        const bool trailing = newline_mode == "preserve"
            ? !text.empty() && text.back() == '\n' : newline_mode == "true";
        std::cout << jsondict::write(root, {formatting, trailing, root_mode == "object"});
        return std::cout.good() ? 0 : 2;
    } catch (const jsondict::Error& error) {
        std::cerr << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "unexpected adapter failure: " << error.what() << '\n';
        return 2;
    }
}
