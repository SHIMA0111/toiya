#pragma once

#include <string>
#include <optional>
#include <hyperapi/hyperapi.hpp>

void createHyperFileFromCsv(const std::string& csvFilePath,
                            const std::string& hyperFilePath,
                            const std::optional<hyperapi::TableDefinition>& tableDefinition = std::nullopt,
                            const std::string& tableName = "Untitled",
                            char delimiter = ',',
                            int databaseVersion = 0);
