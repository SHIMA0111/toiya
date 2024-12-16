#pragma once

#include <string>
#include <unordered_map>

void readHyperFile(const std::string& hyperFilePath,
                   std::unordered_map<std::string, std::string> process_params);