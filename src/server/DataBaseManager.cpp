#include "server/DataBaseManager.h"
#include "common/Hasher.h"

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
            password_salt TEXT NOT NULL,
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

    // Генерируем уникальную соль для этого пользователя
    std::string salt = Hasher::generateSalt();
    std::string hash = Hasher::sha256(passwordHash, salt);

    // SQL-запрос для вставки нового пользователя. Используем параметризованный запрос для безопасности
    const char* sql = "INSERT INTO users (username, password_hash, password_salt) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем false
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса registerUser", LogLevel::ERROR);
        return false;
    }

    // Привязываем параметры: 1-й параметр - username, 2-й - hash пароля, 3-й - соль
    // SQLITE_TRANSIENT указывает SQLite сделать копию строки, чтобы избежать проблем с памятью
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);

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

    // SQL-запрос для поиска пользователя по логину и получению его ID, хеша пароля и соли
    // Сначала получаем соль пользователя
    const char* getSaltSql = "SELECT id, password_hash, password_salt FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    int userId = -1;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем -1
    if (sqlite3_prepare_v2(db, getSaltSql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::getInstance().log("Ошибка подготовки запроса authenticateUser", LogLevel::ERROR);
        return -1;
    }

    // Привязываем параметр: 1-й параметр - username
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    // Выполняем запрос. Если результат SQLITE_ROW, значит найден пользователь с таким логином и паролем, и мы можем получить его ID
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int dbUserId = sqlite3_column_int(stmt, 0); // Получаем ID пользователя
        const unsigned char* dbHash = sqlite3_column_text(stmt, 1); // Получаем хеш пароля из БД
        const unsigned char* dbSalt = sqlite3_column_text(stmt, 2); // Получаем соль из БД

        // Преобразуем хеш и соль из БД в строки для дальнейшей работы
        std::string storedHash(reinterpret_cast<const char*>(dbHash));
        std::string storedSalt(reinterpret_cast<const char*>(dbSalt));

        // Хешируем введённый пароль с солью из БД
        std::string computedHash = Hasher::sha256(passwordHash, storedSalt);

        if (computedHash == storedHash) {
            userId = dbUserId;
        }
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

std::vector<ChatInfo> DataBaseManager::getUserChats(int userId) {
    std::lock_guard<std::mutex> lock(dbMutex);

    std::vector<ChatInfo> chats;

    if (!db) return chats;

    // Для личных чатов: получаем ID чата и логин собеседника.
    // Для групповых: получаем ID чата и название группы из chats.name.
    // Объединяем оба запроса через UNION ALL.
    const char* sql = R"(
        SELECT c.id AS chat_id, u.username AS display_name, c.type
        FROM chats c
        JOIN chat_members m1 ON c.id = m1.chat_id AND m1.user_id = ?
        JOIN chat_members m2 ON c.id = m2.chat_id AND m2.user_id != ?
        JOIN users u ON m2.user_id = u.id
        WHERE c.type = 'personal'

        UNION ALL

        SELECT c.id AS chat_id, c.name AS display_name, c.type
        FROM chats c
        JOIN chat_members m ON c.id = m.chat_id AND m.user_id = ?
        WHERE c.type = 'group'

        ORDER BY chat_id;
    )";

    sqlite3_stmt* stmt = nullptr;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем пустой вектор
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Привязываем параметры: 1-й параметр - userId для личных чатов
        // 2-й параметр - userId для исключения самого себя 
        // 3-й параметр - userId для групповых чатов
        sqlite3_bind_int(stmt, 1, userId);  // для cm1.user_id (personal)
        sqlite3_bind_int(stmt, 2, userId);  // для cm2.user_id != (personal)
        sqlite3_bind_int(stmt, 3, userId);  // для cm.user_id (group)

        // sqlite3_step возвращает SQLITE_ROW каждый раз, когда находит новую строку
        // Поэтому используем цикл while, чтобы собрать весь список чатов
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatInfo info;
            info.chatId = sqlite3_column_int(stmt, 0); // Получаем ID чата
            
            // Получаем имя чата (логин собеседника для личного чата или название группы для группового)
            const unsigned char* nameText = sqlite3_column_text(stmt, 1);
            info.chatName = nameText ? reinterpret_cast<const char*>(nameText) : "Unknown";

            // Получаем тип чата (personal или group)
            std::string typeStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            info.chatType = (typeStr == "group") ? ChatType::GROUP : ChatType::PERSONAL;
            
            chats.push_back(info); // Добавляем информацию о чате в вектор
        }
    } else {
        Logger::getInstance().log("Ошибка подготовки запроса getUserChats", LogLevel::ERROR);
    }
    
    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return chats;
}

std::vector<MessageInfo> DataBaseManager::getChatHistory(int chatId) {
    std::lock_guard<std::mutex> lock(dbMutex);

    std::vector<MessageInfo> history;

    if (!db) return history;

    // Сортировка ASC (Ascending) гарантирует, что старые сообщения будут первыми (сверху)
    const char* sql = "SELECT sender_id, content, timestamp FROM messages WHERE chat_id = ? ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем пустой вектор
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Привязываем параметр: 1-й параметр - chatId
        sqlite3_bind_int(stmt, 1, chatId);

        // sqlite3_step возвращает SQLITE_ROW каждый раз, когда находит новую строку
        // Поэтому используем цикл while, чтобы собрать всю историю сообщений
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            // Извлекаем данные из каждой строки результата и сохраняем их в структуру MessageInfo, которую затем добавляем в вектор history
            MessageInfo msg;
            msg.senderId = sqlite3_column_int(stmt, 0); // Получаем ID отправителя
            
            // Получаем текст сообщения
            const unsigned char* contentText = sqlite3_column_text(stmt, 1);
            msg.content = contentText ? reinterpret_cast<const char*>(contentText) : "";
            
            // Получаем временную метку сообщения
            const unsigned char* timeText = sqlite3_column_text(stmt, 2);
            msg.timestamp = timeText ? reinterpret_cast<const char*>(timeText) : "";

            history.push_back(msg); // Добавляем информацию о сообщении в вектор истории
        }
    } else {
        Logger::getInstance().log("Ошибка подготовки запроса getChatHistory", LogLevel::ERROR);
    }
    
    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return history;
}

int DataBaseManager::getOtherChatMember(int chatId, int myUserId) {
    std::lock_guard<std::mutex> lock(dbMutex);

    if (!db) return -1;

    // В личном чате ровно 2 участника — находим того, чей ID не равен нашему.
    const char* sql = "SELECT user_id FROM chat_members WHERE chat_id = ? AND user_id != ?;";
    sqlite3_stmt* stmt = nullptr;
    int otherUserId = -1; // Инициализируем ID собеседника как -1, что будет означать "не найден"

    // Подготавливаем SQL-запрос. Если подготовка не удалась, логируем ошибку и возвращаем -1
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        // Привязываем параметры: 1-й параметр - chatId, 2-й - myUserId
        sqlite3_bind_int(stmt, 1, chatId);
        sqlite3_bind_int(stmt, 2, myUserId);

        // Выполняем запрос. Если результат SQLITE_ROW, значит найден другой участник личного чата, и мы можем получить его ID
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            otherUserId = sqlite3_column_int(stmt, 0);
        }
    } else {
        Logger::getInstance().log("Ошибка подготовки запроса getOtherChatMember", LogLevel::ERROR);
        return -1;
    }

    // Освобождаем ресурсы, связанные с подготовленным запросом
    sqlite3_finalize(stmt);
    return otherUserId;
}