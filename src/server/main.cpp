#include "common/Logger.h"
#include "server/DataBaseManager.h"
#include <iostream>

int main() {
    Logger::getInstance().log("Сервер запускается...");

    // Инициализируем базу данных
    auto& dbManager = DataBaseManager::getInstance();
    if (!dbManager.init()) {
        Logger::getInstance().log("Не удалось инициализировать базу данных. Завершение работы сервера.", LogLevel::ERROR);
        return 1;
    }  

    // Пробуем зарегестрировать тестового пользователя (для демонстрации)
    dbManager.registerUser("testuser", "testpasswordhash");

    // Проверяем аутентификацию тестового пользователя
    int userId = dbManager.authenticateUser("testuser", "testpasswordhash");
    if (userId != -1) { 
        std::cout << "Успешный вход! ID: " << userId << std::endl;
        Logger::getInstance().log("Пользователь 'testuser' успешно аутентифицирован с ID: " + std::to_string(userId), LogLevel::INFO);

        // Создаём личный чат для тестового пользователя (для демонстрации)
        int chatId = dbManager.createPersonalChat();
        if (chatId != -1) {
            std::cout << "Создан личный чат с ID: " << chatId << std::endl;
            Logger::getInstance().log("Создан личный чат с ID: " + std::to_string(chatId), LogLevel::INFO);
        } else {
            Logger::getInstance().log("Не удалось создать личный чат для пользователя 'testuser'", LogLevel::ERROR);
        }
    } else {
        std::cout << "Ошибка входа!" << std::endl;
        Logger::getInstance().log("Ошибка аутентификации для пользователя 'testuser'", LogLevel::ERROR);
    }

    return 0;
}