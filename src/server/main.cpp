#include "common/Logger.h"
#include "server/DataBaseManager.h"
#include "server/Server.h"
#include <iostream>

int main() {
    Logger::getInstance().log("Инициализируем сервер...");

    // Инициализируем базу данных
    auto& dbManager = DataBaseManager::getInstance();
    if (!dbManager.init()) {
        Logger::getInstance().log("Не удалось инициализировать базу данных. Завершение работы сервера.", LogLevel::ERROR);
        return 1;
    }  

    // Запускаем сервер
    Server server(8080);
    server.start(); // Принимаем клиентов в бесконечном цикле внутри метода start()

    // Остановка сервера и закрытие базы данных при завершении программы 
    dbManager.close();

    return 0;
}