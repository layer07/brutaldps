#ifndef ITEM_H
#define ITEM_H

#include <string>

struct Item {
    std::string name;
    int stamina;
    int hitRating;
    int expertiseRating;
    double dps;
    int cost;
    int slotInt;
    int UUID; 
};

#endif
