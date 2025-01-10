#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include <vector>
#include <string>
#include <ostream>    
#include "Item.h"     


void printTableHeader(std::ostream& os);


void printTableFooter(std::ostream& os);


void printCombination(const std::vector<Item>& combo, 
                      double totalDps, 
                      int totalCost, 
                      int totalStam, 
                      int totalHit, 
                      int totalExpertise);

#endif // PRINT_UTILS_H
