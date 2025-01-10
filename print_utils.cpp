#include "print_utils.h"
#include <iostream>
#include <iomanip>
#include <numeric>
#include <locale>
#include <sstream>
#include <fstream>
#include <mutex>

std::string formatNumber(int number) {
    std::locale comma_locale("");
    std::stringstream ss;
    ss.imbue(comma_locale);
    ss << number;
    return ss.str();
}

static std::ofstream logFile;
static std::mutex logMutex;
static bool headersWritten = false;

void printTableHeader(std::ostream& os) {
    os << "\n"
        << std::left << std::setw(64) << "Item Name"
        << " "
        << std::right << std::setw(10) << "Gold"
        << " "
        << std::right << std::setw(10) << "DPS"
        << " "
        << std::right << std::setw(10) << "Stam"
        << " "
        << std::right << std::setw(10) << "Hit Rate"
        << " "
        << std::right << std::setw(10) << "Expertise"
        << "\n";
    os << std::string(64 + 1 + 10 + 1 + 10 + 1 + 10 + 1 + 10 + 1 + 10, '-') << "\n";
}

void printTableFooter(std::ostream& os) {
    os << std::string(64 + 1 + 10 + 1 + 10 + 1 + 10 + 1 + 10 + 1 + 10, '-') << "\n";
}

void printCombination(const std::vector<Item>& combo, double totalDps, int totalCost, int totalStam, int totalHit, int totalExpertise) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (!logFile.is_open()) {
        logFile.open("results.txt", std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "Failed to open results.txt for logging." << std::endl;
        }
        else {
            logFile.seekp(0, std::ios::end);
            if (logFile.tellp() == 0) {
                printTableHeader(logFile);
                headersWritten = true;
            }
        }
    }

    printTableHeader(std::cout);
    for (const auto& item : combo) {
        std::string name = item.name;
        if (name.length() > 64) {
            name = name.substr(0, 61) + "...";
        }

        std::cout << std::left << std::setw(64) << name
            << " "
            << std::right << std::setw(10) << formatNumber(item.cost)
            << " "
            << std::right << std::setw(10) << std::fixed << std::setprecision(2) << item.dps
            << " "
            << std::right << std::setw(10) << item.stamina
            << " "
            << std::right << std::setw(10) << item.hitRating
            << " "
            << std::right << std::setw(10) << item.expertiseRating
            << "\n";
    }

    printTableFooter(std::cout);
    std::cout << "Total Cost: " << formatNumber(totalCost)
        << " Gold, Total DPS: " << std::fixed << std::setprecision(2) << totalDps
        << ", Total Stam: " << totalStam
        << ", Total Hit: " << totalHit
        << ", Total Expertise: " << totalExpertise << "\n";

    if (logFile.is_open()) {
        if (!headersWritten) {
            printTableHeader(logFile);
            headersWritten = true;
        }

        for (const auto& item : combo) {
            std::string name = item.name;
            if (name.length() > 64) {
                name = name.substr(0, 61) + "...";
            }

            logFile << std::left << std::setw(64) << name
                << " "
                << std::right << std::setw(10) << formatNumber(item.cost)
                << " "
                << std::right << std::setw(10) << std::fixed << std::setprecision(2) << item.dps
                << " "
                << std::right << std::setw(10) << item.stamina
                << " "
                << std::right << std::setw(10) << item.hitRating
                << " "
                << std::right << std::setw(10) << item.expertiseRating
                << "\n";
        }

        printTableFooter(logFile);
        logFile << "Total Cost: " << formatNumber(totalCost)
            << " Gold, Total DPS: " << std::fixed << std::setprecision(2) << totalDps
            << ", Total Stam: " << totalStam
            << ", Total Hit: " << totalHit
            << ", Total Expertise: " << totalExpertise << "\n";
    }
}
