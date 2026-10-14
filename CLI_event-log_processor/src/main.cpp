#include <iostream>
#include <fstream>
#include "event_processor.h"

void print_usage(std::ostream& out)
{
    out << "Usage: logproc --input <file> [--output <file>]\n"
        "\n"
        "Reads a JSONL event log, removes duplicate events (first occurrence\n"
        "of each id is kept) and prints a daily summary (UTC days).\n"
        "\n"
        "Options:\n"
        "  -i, --input <file>   input JSONL file (required)\n"
        "  -o, --output <file>  write the report to this file instead of the console\n"
        "  -h, --help           show this help\n";
}

int main(int argc, char* argv[])
{
    std::string input_path;
    std::string output_path;

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;

        if (arg == "-h" || arg == "--help") {
            print_usage(std::cout);
            return 0;
        }

        if ((arg == "-i" || arg == "--input") && has_value) {
            input_path = argv[++i];
        }
        else if ((arg == "-o" || arg == "--output") && has_value) {
            output_path = argv[++i];
        }
        else {
            std::cerr << "Error: Invalid argument '" << arg << "'\n\n";
            print_usage(std::cerr);
            return 2;
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: Missing --input\n\n";
        print_usage(std::cerr);
        return 2;
    }

    std::ifstream in(input_path);
    if (!in) {
        std::cerr << "Error: Can not open '" << input_path << "'\n";
        return 1;
    }
    std::ofstream out_file;
    if (!output_path.empty()) {
        out_file.open(output_path);
        if (!out_file) {
            std::cerr << "Error: Cannot write '" << output_path << "'\n";
            return 1;
        }
    }
    std::ostream& out = output_path.empty() ? std::cout : out_file;

    const Result result = process(in);
    report(result, out);

    out.flush();
    if (!out) {
        std::cerr << "Error: Failed while writing the report\n";
        return 1;
    }
    return 0;
}