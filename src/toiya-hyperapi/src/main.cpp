#include "reader_sample.hpp"

#include <iostream>

int main() {
    Result result = read_from_hyper_query("/Users/seigooshima/git/toiya/src/toiya-hyperapi/src/data/train.hyper",
                                          "SELECT * FROM spaceship", 5);
    std::cout << "result.data: " << result.data << std::endl;

    return 0;
}
