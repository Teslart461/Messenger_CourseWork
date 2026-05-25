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

    // Открываем соединение с базой данных. Если файл не существует, SQLite создаст его
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
    // sqlite3_exec выполняет сразу все SQL-команды из переданной строки
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



// Отделение логики регистрации, аутентификации и создания чата для удобства использования




// Метод для регистрации нового пользователя. Возвращает true при успешной регистрации, false при ошибке (например, если логин уже занят)
bool DataBaseManager::registerUser(const std::string& username, const std::string& passwordHash) {
    // Как только создается lock_guard, он захватывает мьютекс.
    // Если другой поток уже держит мьютекс, этот поток заснет на этой строчке
    // и будет ждать своей очереди. Когда lock_guard выходит из области видимости (например, при выходе из функции), он автоматически отпускает мьютекс.
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return false;

    // SQL-запрос для вставки нового пользователя. Используем параметризованный запрос для безопасности
    const char* sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем false
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса registerUser", LogLevel::ERROR);
        return false;
    }

    // Привязываем параметры: 1-й параметр - username, 2-й - passwordHash
    // SQLITE_TRANSIENT указывает SQLite сделать копию строки, чтобы избежать проблем с памятью
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    // Выполняем запрос. Если результат SQLITE_DONE, значит вставка прошла успешно
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);

    // Логируем результат регистрации
    if (success) {
        Logger::getInstance().log("Зарегистрирован новый пользователь: " + username, LogLevel::INFO);
    } else {
        Logger::getInstance().log("Не удалось зарегистрировать пользователя (возможно логин уже используется): " + username, LogLevel::WARNING);
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return success;
}

// Метод для аутентификации пользователя. Возвращает ID пользователя при успешной аутентификации, -1 при ошибке (например, если логин/пароль не совпадают)
int DataBaseManager::authenticateUser(const std::string& username, const std::string& passwordHash) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    // SQL-запрос для поиска пользователя с заданным логином и хешем пароля
    const char* sql = "SELECT id FROM users WHERE username = ? AND password_hash = ?;";
    sqlite3_stmt* stmt = nullptr;
    int userId = -1;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем -1
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса authenticateUser", LogLevel::ERROR);
        return -1;
    }

    // Привязываем параметры: 1-й параметр - username, 2-й - passwordHash
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    // Выполняем запрос. Если результат SQLITE_ROW, значит найден пользователь с таким логином и паролем, и мы можем получить его ID
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt, 0); // Получаем значение первого столбца (ID пользователя)
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return userId;
}

// Метод для создания нового личного чата. Возвращает ID созданного чата или -1 при ошибке
int DataBaseManager::createPersonalChat() {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    // SQL-запрос для создания нового личного чата. Тип чата - 'personal', имя не задается (NULL)
    const char* sql = "INSERT INTO chats (type, name) VALUES ('personal', NULL);";

    // Выполняем запрос. Если результат не SQLITE_OK, значит произошла ошибка при создании чата
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка создания чата", LogLevel::ERROR);
        return -1;
    }

    // Получаем ID только что созданного чата
    int chatId = sqlite3_last_insert_rowid(db);
    Logger::getInstance().log("Создан новый чат с ID: " + std::to_string(chatId), LogLevel::INFO);
    return chatId;
}

int DataBaseManager::getPersonalChat(int userId1, int userId2) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    // SQL-запрос для поиска существующего личного чата между двумя пользователями
    // из таблицы chat_members как для user1, так и для user2 одновременно.
    const char* sql = R"(
        SELECT c.id
        FROM chats c
        JOIN chat_members m1 ON c.id = m1.chat_id
        JOIN chat_members m2 ON c.id = m2.chat_id
        WHERE c.type = 'personal'
          AND m1.user_id = ?
          AND m2.user_id = ?
        LIMIT 1;
    )";

    sqlite3_stmt* stmt = nullptr;
    int chatId = -1;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем -1
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса getPersonalChat", LogLevel::ERROR);
        return -1;
    }

    // Привязываем параметры: 1-й параметр - userId1, 2-й - userId2
    sqlite3_bind_int(stmt, 1, userId1);
    sqlite3_bind_int(stmt, 2, userId2);

    // Выполняем запрос. Если результат SQLITE_ROW, значит найден личный чат между этими пользователями, и мы можем получить его ID
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        chatId = sqlite3_column_int(stmt, 0); // Получаем значение первого столбца (ID чата)
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return chatId; // Вернет ID личного чата или -1 при ошибке (например, если такого чата нет)
}

int DataBaseManager::getUserIdByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    // SQL-запрос для получения ID пользователя по его логину
    const char* sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    int userId = -1;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем -1
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса getUserIdByUsername", LogLevel::ERROR);
        return -1;
    }

    // Привязываем параметр: 1-й параметр - username
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    // Выполняем запрос. Если результат SQLITE_ROW, значит найден пользователь с таким логином, и мы можем получить его ID
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt, 0); // Получаем значение первого столбца (ID пользователя)
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return userId;
}

bool DataBaseManager::addChatMember(int chatId, int userId) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return false;

    // SQL-запрос для добавления пользователя в чат. Используем параметризованный запрос для безопасности
    const char* sql = "INSERT INTO chat_members (chat_id, user_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем false
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса addChatMember", LogLevel::ERROR);
        return false;
    }

    // Привязываем параметры: 1-й параметр - chatId, 2-й - userId
    sqlite3_bind_int(stmt, 1, chatId);
    sqlite3_bind_int(stmt, 2, userId);

    // Выполняем запрос. Если результат SQLITE_DONE, значит пользователь успешно добавлен в чат
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);

    // Логируем результат добавления участника в чат
    if (success) {
        Logger::getInstance().log("Пользователь с ID " + std::to_string(userId) + " добавлен в чат с ID " + std::to_string(chatId), LogLevel::DEBUG);
    } else {
        Logger::getInstance().log("Не удалось добавить пользователя с ID " + std::to_string(userId) + " в чат с ID " + std::to_string(chatId), LogLevel::WARNING);
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return success;
}

bool DataBaseManager::saveMessage(int chatId, int senderId, const std::string& content) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return false;

    // SQL-запрос для сохранения сообщения в чате. Используем параметризованный запрос для безопасности
    const char* sql = "INSERT INTO messages (chat_id, sender_id, content) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем false
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса saveMessage", LogLevel::ERROR);
        return false;
    }

    // Привязываем параметры: 1-й параметр - chatId, 2-й - senderId, 3-й - content
    sqlite3_bind_int(stmt, 1, chatId);
    sqlite3_bind_int(stmt, 2, senderId);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);

    // Выполняем запрос. Если результат SQLITE_DONE, значит сообщение успешно сохранено в базе данных
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);

    // Логируем результат сохранения сообщения
    if (success) {
        Logger::getInstance().log("Сообщение от пользователя с ID " + std::to_string(senderId) + " сохранено в чате с ID " + std::to_string(chatId), LogLevel::DEBUG);
    } else {
        Logger::getInstance().log("Не удалось сохранить сообщение от пользователя с ID " + std::to_string(senderId) + " в чате с ID " + std::to_string(chatId), LogLevel::WARNING);
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return success;
}