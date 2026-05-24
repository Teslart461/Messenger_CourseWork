#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <string>
#include <sqlite3.h>
#include <vector>
#include <mutex>

#include "common/Logger.h"

/**
 * @brief Класс для управления базой данных (SQLite3).
 * Реализует паттерн Singleton — в приложении существует ровно один экземпляр,
 * через который выполняются все операции с базой данных.
 */
class DataBaseManager {
private:

    /**
     * @brief Приватный конструктор.
     * Вызывается только через getInstance(), само соединение не открывает —
     * для этого нужно явно вызвать init().
     */
    DataBaseManager() = default;

    /**
     * @brief Деструктор.
     * Автоматически закрывает соединение с базой данных при завершении программы.
     */
    ~DataBaseManager();
    
    // Запрет копирования и присваивания объекта
    DataBaseManager(const DataBaseManager&) = delete;
    DataBaseManager& operator=(const DataBaseManager&) = delete;

    sqlite3* db = nullptr;          // Указатель на открытое соединение с БД
    std::mutex dbMutex;             // Мьютекс для потокобезопасной инициализации и закрытия
    bool initialized = false;       // Флаг, показывающий, была ли выполнена инициализация

public:
    /**
     * @brief Единственный экземпляр базы данных.
     * @return Ссылка на объект DataBaseManager.
     */
    static DataBaseManager& getInstance();

    /**
     * @brief Инициализация соединение с базой данных и создаёт все необходимые таблицы.
     * 
     * Если файл БД не существует, SQLite создаст его автоматически.
     * При повторном вызове повторная инициализация не выполняется.
     * 
     * @param dbPath Путь к файлу базы данных.
     * @return true при успешной инициализации, false при ошибке.
     */
    bool init(const std::string& dbPath = "messenger.db");

    /**
     * @brief Корректное закрытие соединения с БД.
     */
    void close();
};

#endif