#include "common/Logger.h"
#include "server/DataBaseManager.h"

int main() {
    Logger::getInstance().log("Сервер запускается...");

    // Инициализируем базу данных
    if (!DataBaseManager::getInstance().init()) {
        Logger::getInstance().log("Не удалось инициализировать базу данных. Завершение работы.", LogLevel::ERROR);
        return 1;
    }

    // Здесь будет код для запуска сервера, принятия подключений и обработки сообщений.
    Logger::getInstance().log("Сервер успешно запущен. Ожидание подключений...");

    // Для демонстрации просто ждем ввода в консоли, чтобы не завершать программу сразу.
    DataBaseManager::getInstance().close();
    Logger::getInstance().log("Сервер завершил работу.");

    return 0;
}