#include "server/DataBaseManager.h"

// Реализация метода получения Singleton-объекта
DataBaseManager& DataBaseManager::getInstance() {
    static DataBaseManager instance;
    return instance;
}

// Деструктор, что закрывает соединение с базой данных при уничтожении объекта
DataBaseManager::~DataBaseManager() {
    close();
}

bool DataBaseManager::init(const std::string& dbPath) {
    // Захватываем мьютекс на всё время инициализации,
    // чтобы два потока случайно не открыли два разных соединения
    std::lock_guard<std::mutex> lock(dbMutex);

    // Повторная инициализация не требуется
    if (initialized) {
        Logger::getInstance().log("База данных уже инициализирована.", LogLevel::WARNING);
        return true;
    }

    // Открываем соединение с базой данных. Если файл не существует, SQLite создаст его.
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        Logger::getInstance().log("Ошибка открытия БД: " + std::string(sqlite3_errmsg(db)), LogLevel::ERROR);
        return false;
    }

    Logger::getInstance().log("База данных открыта: " + dbPath, LogLevel::INFO);

    // Включаем поддержку внешних ключей 
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Схема базы данных: таблицы для пользователей, чатов, участников и сообщений
    const char* sqlSchema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS chats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            type TEXT NOT NULL CHECK(type IN ('personal', 'group')),
            name TEXT
        );

        CREATE TABLE IF NOT EXISTS chat_members (
            chat_id INTEGER NOT NULL,
            user_id INTEGER NOT NULL,
            FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE,
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
            PRIMARY KEY (chat_id, user_id)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            chat_id INTEGER NOT NULL,
            sender_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE,
            FOREIGN KEY (sender_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

    // Выполняем SQL-запрос на создание таблиц.
    // sqlite3_exec выполняет сразу все SQL-команды из переданной строки.
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, sqlSchema, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        Logger::getInstance().log("Ошибка инициализации таблиц: " + std::string(errMsg), LogLevel::ERROR);
        sqlite3_free(errMsg);
        return false;
    }

    Logger::getInstance().log("Таблицы базы данных проверены/созданы.", LogLevel::INFO);

    // Помечаем, что инициализация прошла успешно
    initialized = true;
    return true;
}

// Метод для корректного закрытия соединения с базой данных
void DataBaseManager::close() {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (db) {
        sqlite3_close(db);
        db = nullptr;
        initialized = false;
        Logger::getInstance().log("Соединение с БД закрыто.", LogLevel::INFO);
    }
}