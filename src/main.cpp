#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// The book-of-mormon-reference.js file was originally used in a javascript project which I worked on about a year ago.
// As such, to use this file in a C++ project, we need to fix all the JS stuff you would have in strings.
std::string fixJavascriptTypology(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            const char next = input[i + 1];
            switch (next) {
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case '\'': out.push_back('\''); break;
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                default: out.push_back(next); break;
            }
            ++i;
        } else {
            out.push_back(input[i]);
        }
    }

    return out;
}

std::vector<std::string> extractVerses(const std::string& filePath) {
    std::ifstream input(filePath);
    if (!input) {
        throw std::runtime_error("Could not open file: " + filePath);
    }

    // This monster captures two values: the verse number and the verse text.
    // I can't explain how the regex works tho... Thats above my paygrade...
    const std::regex verseLinePattern(R"REGEX(^\s*"(\d+)"\s*:\s*"((?:\\.|[^"\\])*)"\s*,?\s*$)REGEX");

    std::vector<std::string> verses;
    std::string line;

    while (std::getline(input, line)) {
        std::smatch match;
        if (std::regex_match(line, match, verseLinePattern)) {
            verses.push_back(fixJavascriptTypology(match[2].str()));
        }
    }

    return verses;
}

std::string getBookOfMormonFile(int argc, char* argv[]) {
    if (argc > 1) {
        return argv[1];
    }

    const char* kDefaultFile = "book-of-mormon-reference.js";
    const std::vector<std::string> candidates = {
        kDefaultFile,
        std::string("../") + kDefaultFile,
        std::string("../../") + kDefaultFile,
    };

    for (const auto& candidate : candidates) {
        std::ifstream input(candidate);
        if (input) {
            return candidate;
        }
    }

    return kDefaultFile;
}

std::string recordFindingsInANeatLittleCSVFile(int argc, char* argv[]) {
    if (argc > 2) {
        return argv[2];
    }
    return "word-frequency.csv";
}

// CSV files are weird
// this function ensures that certin characters are properly read. 
// new line, tab, and comma characters are quite often problematic in CSV files.
std::string csvEscape(const std::string& value) {
    bool needsQuotes = false;
    std::string escaped;
    escaped.reserve(value.size());

    for (char ch : value) {
        if (ch == '"') {
            escaped.push_back('"');
            escaped.push_back('"');
            needsQuotes = true;
        } else {
            if (ch == ',' || ch == '\n' || ch == '\r') {
                needsQuotes = true;
            }
            escaped.push_back(ch);
        }
    }

    if (needsQuotes) {
        return '"' + escaped + '"';
    }

    return escaped;
}

// Writes the word frequency ranking to a CSV file.
// This CSV file has three nifty little columns: rank, word, and count.
void writeCsv(const std::string& outputPath, const std::vector<std::pair<std::string, std::size_t>>& ranking) {
    std::ofstream output(outputPath);
    if (!output) {
        throw std::runtime_error("Could not open output file: " + outputPath);
    }

    output << "rank,word,count\n";

    std::size_t rank = 1;
    for (const auto& entry : ranking) {
        output << rank << ',' << csvEscape(entry.first) << ',' << entry.second << '\n';
        ++rank;
    }
}

void addWords(const std::string& text, std::unordered_map<std::string, std::size_t>& counts, std::size_t& totalWordCount) {
    std::string current;

    auto flushWord = [&]() {
        if (!current.empty()) {
            ++counts[current];
            ++totalWordCount;
            current.clear();
        }
    };

    for (char ch : text) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalpha(uch)) {
            current.push_back(static_cast<char>(std::tolower(uch)));
        } else {
            flushWord();
        }
    }

    flushWord();
}

}

int main(int argc, char* argv[]) {
    try {
        const std::string inputPath = getBookOfMormonFile(argc, argv);
        const std::string outputPath = recordFindingsInANeatLittleCSVFile(argc, argv);

        const auto verses = extractVerses(inputPath);
        if (verses.empty()) {
            std::cerr << "No verses found in file: " << inputPath << "\n";
            return 1;
        }

        std::unordered_map<std::string, std::size_t> counts;
        std::size_t totalWordCount = 0;

        // The addWords function adds all instances of words
        for (const auto& verse : verses) {
            addWords(verse, counts, totalWordCount);
        }

        // Rank the words after word count is found
        std::vector<std::pair<std::string, std::size_t>> ranking(counts.begin(), counts.end());
        std::sort(ranking.begin(), ranking.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) {
                return a.second > b.second;
            }
            return a.first < b.first;
        });

        std::cout << "Book of Mormon Word Frequency\n";
        std::cout << "================================\n";
        std::cout << "Verses parsed: " << verses.size() << "\n";
        std::cout << "Total words: " << totalWordCount << "\n";
        std::cout << "Unique words: " << ranking.size() << "\n\n";

        std::cout << std::left
                  << std::setw(8) << "Rank"
                  << std::setw(26) << "Word"
                  << "Count\n";
        std::cout << std::string(46, '-') << "\n";

        std::size_t rank = 1;
        for (const auto& entry : ranking) {
            std::cout << std::left
                      << std::setw(8) << rank
                      << std::setw(26) << entry.first
                      << entry.second << '\n';
            ++rank;
        }

        writeCsv(outputPath, ranking);
        std::cout << "\nCSV export written to: " << outputPath << "\n";

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
