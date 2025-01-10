#include "print_utils.h"
#include "Item.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <json.hpp>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include <numeric>

using json = nlohmann::json;
using namespace std;
using namespace chrono;

const int MinStamina = 1000;
const int MinHit = 300;
const int MinExp = 100;

long long calculateTotalCombinations(const vector<vector<Item>>& itemsBySlot) {
    long long totalCombos = 1;

    for (const auto& slot : itemsBySlot) {
        totalCombos *= slot.size();
        if (totalCombos < 0) {
            cerr << "Overflow detected in total combinations calculation!" << endl;
            exit(1);
        }
    }
    return totalCombos;
}

vector<Item> loadItems(const string& path) {
    ifstream file(path);
    vector<Item> items;

    if (file.is_open()) {
        json j;
        file >> j;
        cout << "Loaded " << j.size() << " items from the JSON file." << endl;

        items.reserve(j.size());

        for (const auto& element : j) {
            Item item;
            item.name = element["Name"].get<string>();
            item.stamina = element["Stamina"].get<int>();
            item.hitRating = element["Hit Rating"].get<int>();
            item.expertiseRating = element["Expertise Rating"].get<int>();
            item.dps = element["DPS"].get<double>();
            item.cost = element["Cost"].get<int>();
            item.slotInt = element["SlotInt"].get<int>();
            item.UUID = element["UUID"].get<int>();
            items.emplace_back(move(item));
        }
    }
    else {
        cerr << "Failed to open the file!" << endl;
    }

    return items;
}

void processCombinations(const vector<vector<Item>>& itemsBySlot, long long startIdx, long long endIdx,
    const vector<long long>& multipliers,
    vector<Item>& threadBestCombo, double& threadBestDps,
    long long& threadProcessedCount,
    double& globalBestDps, vector<Item>& globalBestCombo,
    mutex& globalBestMutex)
{
    const vector<size_t> ringSlotIndices = { 10, 11 };
    array<int, 12> slotIndices;
    int tempStam, tempHit, tempExp, tempCost;
    double tempDps;
    vector<Item> tempCombo(itemsBySlot.size());

    for (long long idx = startIdx; idx < endIdx; ++idx) {
        long long remainder = idx;
        for (size_t i = 0; i < itemsBySlot.size(); ++i) {
            slotIndices[i] = (remainder / multipliers[i]) % itemsBySlot[i].size();
        }

        tempStam = tempHit = tempExp = tempCost = 0;
        tempDps = 0.0;

        for (size_t j = 0; j < itemsBySlot.size(); j++) {
            const Item& item = itemsBySlot[j][slotIndices[j]];
            tempCombo[j] = item;

            tempStam += item.stamina;
            tempHit += item.hitRating;
            tempExp += item.expertiseRating;
            tempDps += item.dps;
            tempCost += item.cost;
        }

        bool ringsUnique = true;
        if (ringSlotIndices.size() >= 2) {
            int lastTwoUUID1 = tempCombo[ringSlotIndices[0]].UUID % 100;
            int lastTwoUUID2 = tempCombo[ringSlotIndices[1]].UUID % 100;

            if (lastTwoUUID1 == lastTwoUUID2) {
                ringsUnique = false;
            }
        }

        if (!ringsUnique) {
            threadProcessedCount++;
            continue;
        }

        if (tempStam >= MinStamina && tempHit >= MinHit && tempExp >= MinExp && tempDps > 0) {
            if (tempDps > threadBestDps) {
                threadBestDps = tempDps;
                threadBestCombo = tempCombo;

                lock_guard<mutex> lock(globalBestMutex);
                if (tempDps > globalBestDps) {
                    globalBestDps = tempDps;
                    globalBestCombo = tempCombo;

                    int totalStam = tempStam;
                    int totalHit = tempHit;
                    int totalExpertise = tempExp;
                    int totalCost = tempCost;

                    printCombination(globalBestCombo, globalBestDps, totalCost, totalStam, totalHit, totalExpertise);
                }
            }
        }

        threadProcessedCount++;
    }
}

int main() {
    string path = "clean_pre_items.json";
    vector<Item> allItems = loadItems(path);

    if (allItems.empty()) {
        cerr << "No items loaded." << endl;
        return 1;
    }

    const int numSlots = 12;
    vector<vector<Item>> itemsBySlot(numSlots);

    for (auto& item : allItems) {
        if (item.slotInt >= 10 && item.slotInt < 10 + numSlots) {
            itemsBySlot[item.slotInt - 10].emplace_back(move(item));
        }
    }

    for (size_t i = 0; i < itemsBySlot.size(); i++) {
        cout << "Slot " << (i + 10) << " has " << itemsBySlot[i].size() << " items." << endl;
        if (itemsBySlot[i].empty()) {
            cerr << "No items found for slot " << (i + 10) << endl;
            return 1;
        }
    }

    long long totalCombos = calculateTotalCombinations(itemsBySlot);
    cout << "Total combinations to process: " << totalCombos << endl;

    vector<long long> multipliers(numSlots, 1);
    for (size_t i = 1; i < numSlots; ++i) {
        multipliers[i] = multipliers[i - 1] * itemsBySlot[i - 1].size();
    }

    unsigned int hardwareThreads = thread::hardware_concurrency();
    unsigned int numThreads = (hardwareThreads > 0) ? min(hardwareThreads, (unsigned int)32) : 32;
    cout << "Using " << numThreads << " threads for processing." << endl;

    vector<vector<Item>> threadBestCombos(numThreads, vector<Item>(numSlots));
    vector<double> threadBestDpses(numThreads, 0.0);
    vector<long long> threadProcessedCounts(numThreads, 0);

    double globalBestDps = 0.0;
    vector<Item> globalBestCombo(numSlots);
    mutex globalBestMutex;

    vector<long long> threadStartIndices(numThreads);
    vector<long long> threadEndIndices(numThreads);
    long long combosPerThread = totalCombos / numThreads;
    for (unsigned int i = 0; i < numThreads; ++i) {
        threadStartIndices[i] = i * combosPerThread;
        if (i == numThreads - 1) {
            threadEndIndices[i] = totalCombos;
        }
        else {
            threadEndIndices[i] = (i + 1) * combosPerThread;
        }
    }

    auto startTime = high_resolution_clock::now();
    auto lastTime = startTime;
    long long lastProcessed = 0;

    vector<thread> threads;
    threads.reserve(numThreads);
    for (unsigned int i = 0; i < numThreads; ++i) {
        threads.emplace_back(processCombinations, cref(itemsBySlot),
            threadStartIndices[i], threadEndIndices[i],
            cref(multipliers),
            ref(threadBestCombos[i]), ref(threadBestDpses[i]),
            ref(threadProcessedCounts[i]),
            ref(globalBestDps), ref(globalBestCombo),
            ref(globalBestMutex));
    }

    while (true) {
        this_thread::sleep_for(seconds(1));

        long long totalProcessed = accumulate(threadProcessedCounts.begin(), threadProcessedCounts.end(), 0LL);

        long long processedInLastSec = totalProcessed - lastProcessed;
        lastProcessed = totalProcessed;

        double combosPerSec = static_cast<double>(processedInLastSec);
        double remainingCombos = static_cast<double>(totalCombos - totalProcessed);
        double etaSeconds = (combosPerSec > 0) ? (remainingCombos / combosPerSec) : 0.0;

        int etaH = static_cast<int>(etaSeconds / 3600);
        int etaM = static_cast<int>((etaSeconds - etaH * 3600) / 60);
        int etaS = static_cast<int>(etaSeconds) % 60;

        const int barWidth = 40;
        double progress = static_cast<double>(totalProcessed) / totalCombos;
        int pos = static_cast<int>(barWidth * progress);

        string bar = "[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) bar += "#";
            else bar += "-";
        }
        bar += "]";

        cout << "\r" << bar << " "
            << fixed << setprecision(1) << (progress * 100.0) << "% | "
            << "SPD: " << (combosPerSec / 1e6) << "M/s | ETA: "
            << etaH << "h " << etaM << "m " << etaS << "s | "
            << totalProcessed << "/" << totalCombos
            << flush;

        if (totalProcessed >= totalCombos) {
            break;
        }
    }

    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    if (!globalBestCombo.empty()) {
        int totalCost = 0;
        int totalStam = 0;
        int totalHit = 0;
        int totalExpertise = 0;
        double totalDps = 0.0;

        for (const auto& item : globalBestCombo) {
            totalCost += item.cost;
            totalStam += item.stamina;
            totalHit += item.hitRating;
            totalExpertise += item.expertiseRating;
            totalDps += item.dps;
        }

        printCombination(globalBestCombo, totalDps, totalCost, totalStam, totalHit, totalExpertise);
    }
    else {
        cout << "\nNo valid combination found." << endl;
    }

    auto endTime = high_resolution_clock::now();
    auto totalDuration = duration_cast<seconds>(endTime - startTime).count();
    cout << "Total processing time: " << totalDuration << " seconds." << endl;

    return 0;
}
