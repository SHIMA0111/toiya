#include "date_calc.hpp"

#include <iostream>

int main() {
//    runReadAndPrintDataFromExistingHyperFile("/Users/seigooshima/git/toiya/src/toiya-hyperapi/src/data/train.hyper");
//    createHyperFileFromCsv("/Users/seigooshima/git/toiya/src/toiya-hyperapi/src/data/train.csv", "/Users/seigooshima/git/toiya/src/toiya-hyperapi/src/data/train_cxx.hyper");

    std::cout << DateCalc::calculateDaysSinceAD(1970, 1, 1) << std::endl;
    return 0;
}
