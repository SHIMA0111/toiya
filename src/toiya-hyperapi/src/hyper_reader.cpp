#include "hyper_reader.hpp"

#include <fstream>
#include <hyperapi/hyperapi.hpp>
#include <iostream>

void readHyperFile(const std::string& hyperFilePath,
                   std::unordered_map<std::string, std::string> process_params) {
    {
        hyperapi::HyperProcess hyper(hyperapi::Telemetry::DoNotSendUsageDataToTableau);

        {
            hyperapi::Connection connection(hyper.getEndpoint(), hyperFilePath);
            const hyperapi::Catalog& catalog = connection.getCatalog();

            std::unordered_set<hyperapi::TableName> tableNames = catalog.getTableNames("spaceship");
            for (auto& tableName : tableNames) {
                hyperapi::TableDefinition tableDefinition = catalog.getTableDefinition(tableName);
                for (auto& column : tableDefinition.getColumns()) {
                    std::cout << "\t Column " << column.getName() << " has type " << column.getType() << " and nullability " << column.getNullability() << std::endl;
                }
                std::cout << std::endl;
            }
            hyperapi::TableName extractTable("spaceship");
            std::cout << "These are all rows in the table " << extractTable.toString() << ":" << std::endl;

            hyperapi::Result rowsInTable = connection.executeQuery("SELECT * FROM " + extractTable.toString());
            for (const hyperapi::Row& row : rowsInTable) {
                for (const hyperapi::Value& value : row) {
                    std::cout << value << '\t';
                }
                std::cout << '\n';
            }
        }
        std::cout << "The connection to the Hyper file has been closed." << std::endl;
    }
    std::cout << "The Hyper Process has been shut down." << std::endl;
}
