#include "hyper_writer.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <regex>

hyperapi::SqlType inferType(const std::vector<std::string>& sampleData);
hyperapi::TableDefinition inferTypesFromCsv(const std::string& csvFilePath, const std::string& tableName, char delimiter);
bool isBoolean(const std::string& value);

void createHyperFileFromCsv(const std::string& csvFilePath,
                            const std::string& hyperFilePath,
                            const std::optional<hyperapi::TableDefinition>& tableDefinition,
                            const std::string& tableName,
                            char delimiter,
                            int databaseVersion) {
    if (databaseVersion > 4) {
        throw std::invalid_argument("databaseVersion supports only 0, 1, 2, 3, 4.");
    }

    namespace fs = std::filesystem;

    fs::path pathToFile(csvFilePath);
    fs::path pathToDatabase(hyperFilePath);

    std::cout << "Load csv from: " << pathToFile.string() << ", Write hyper to: " << pathToDatabase.string() << std::endl;

    if (!fs::exists(pathToFile) || fs::is_directory(pathToFile)) {
        throw std::invalid_argument("File does not exist or is a directory.");
    }

    {
        std::unordered_map<std::string, std::string> processParameters = {
            {"log_file_max_count", "2"},
            {"log_file_size_limit", "100M"},
            {"default_database_version", std::to_string(databaseVersion)}
        };

        hyperapi::HyperProcess hyper(hyperapi::Telemetry::SendUsageDataToTableau, "", processParameters);

        {
            hyperapi::Connection connection(hyper.getEndpoint(), absolute(pathToDatabase).string(), hyperapi::CreateMode::CreateAndReplace);

            hyperapi::TableDefinition tableDefinitionData = (tableDefinition.has_value()) ? tableDefinition.value() : inferTypesFromCsv(absolute(pathToFile).string(), tableName, delimiter);

            const hyperapi::Catalog& catalog = connection.getCatalog();
            catalog.createTable(tableDefinitionData);

            std::cout << "Issuing the SQL COPY command to load the csv file into the table. Since the first line" << std::endl;
            std::cout << "of our csv file contains the column names, we use the `header` option to skip it." << std::endl;
            int64_t rowCount = connection.executeCommand(
                "COPY " + tableDefinitionData.getTableName().toString() + " FROM " +
                hyperapi::escapeStringLiteral(absolute(pathToFile).string()) + " WITH (format csv, delimiter '" + delimiter + "', header)"
                );

            std::cout << "The number of rows in table " << tableDefinitionData.getTableName() << " is " << rowCount << "." << std::endl;
        }

        std::cout << "The connection to the Hyper file has been closed." << std::endl;
    }

    std::cout << "The Hyper Process has been shut down." << std::endl;
}

hyperapi::TableDefinition inferTypesFromCsv(const std::string& csvFilePath,
                                            const std::string& tableName,
                                            char delimiter) {
    std::ifstream file(csvFilePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file.");
    }

    int64_t rowsNum = std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');

    if (rowsNum > std::numeric_limits<int>::max()) {
        throw std::out_of_range("The number of rows in the csv file is too large.");
    }
    int downCastedRowsNum = static_cast<int>(rowsNum);

    file.clear();
    file.seekg(0);

    std::string line;
    std::getline(file, line);
    std::istringstream lineStream(line);
    std::vector<std::string> headers;
    std::string column;

    while (std::getline(lineStream, column, delimiter)) {
        headers.push_back(column);
    }

    int sample_size = static_cast<int>(std::max(10.0, downCastedRowsNum * 0.01));
    std::vector<std::vector<std::string>> sample_data;

    for (int i = 0; i < sample_size && std::getline(file, line); ++i) {
        std::vector<std::string> row;
        std::istringstream rowStream(line);
        while (std::getline(rowStream, column, delimiter)) {
            row.push_back(column);
        }
        sample_data.push_back(row);
    }

    std::vector<std::vector<std::string>> columnsValues(headers.size());
    for (const auto& row : sample_data) {
        for (size_t i = 0; i < row.size(); ++i) {
            columnsValues[i].push_back(row[i]);
        }
    }

    std::vector<hyperapi::TableDefinition::Column> columns;
    for (size_t i = 0; i < headers.size(); ++i) {
        hyperapi::SqlType sqlType = inferType(columnsValues[i]);
        columns.emplace_back(headers[i], sqlType);
    }

    return hyperapi::TableDefinition(tableName, columns);
}

hyperapi::SqlType inferType(const std::vector<std::string>& sampleData) {
    std::vector<std::string> filtered;
    for (const auto& value : sampleData) {
        if (!value.empty() && value != "NULL" && value.find_first_not_of(' ') != std::string::npos) {
            filtered.push_back(value);
        }
    }

    if (filtered.empty()) {
        return hyperapi::SqlType::text();
    }

    std::vector<std::string> dataParsed;
    for (const auto& data : filtered) {
        try {
            float floatData = std::stof(data);

            if (static_cast<float>(static_cast<int>(floatData)) == floatData) {
                if (std::regex_match(data, std::regex("^[0-9]+(_[0-9]+)+$"))) {
                    dataParsed.emplace_back("TEXT");
                }
                else if (std::regex_match(data, std::regex("^[0-9]+.[0-9]+$"))) {
                    dataParsed.emplace_back("DOUBLE");
                }
                else {
                    dataParsed.emplace_back("BIGINT");
                }
                continue;
            }
            else {
                dataParsed.emplace_back("DOUBLE");
                continue;
            }
        } catch (...) {
            if (isBoolean(data)) {
                dataParsed.emplace_back("BOOLEAN");
                continue;
            }

            dataParsed.emplace_back("TEXT");
        }
    }

    hyperapi::SqlType sqlType = hyperapi::SqlType::text();

    if (std::all_of(dataParsed.begin(), dataParsed.end(), [](const std::string& data) { return data == "BIGINT"; })) {
        sqlType = hyperapi::SqlType::bigInt();
    }
    else if (std::all_of(dataParsed.begin(), dataParsed.end(), [](const std::string& data) { return data == "DOUBLE"; })) {
        sqlType = hyperapi::SqlType::doublePrecision();
    }
    else if (std::all_of(dataParsed.begin(), dataParsed.end(), [](const std::string& data) { return data == "BOOLEAN"; })) {
        sqlType = hyperapi::SqlType::boolean();
    }

    return sqlType;
}

bool isBoolean(const std::string& value) {
    std::string lowerCaseValue = value;
    std::transform(lowerCaseValue.begin(), lowerCaseValue.end(), lowerCaseValue.begin(), ::tolower);

    return lowerCaseValue == "true" || lowerCaseValue == "false";
}
