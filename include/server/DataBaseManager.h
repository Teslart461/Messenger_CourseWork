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


    /**
     * @brief Регистрация нового пользователя.
     * @param username Логин.
     * @param passwordHash Хеш пароля.
     * @return true при успехе, false если логин занят или ошибка.
     */
    bool registerUser(const std::string& username, const std::string& passwordHash);

    /**
     * @brief Аутентификация пользователя.
     * @param username Логин.
     * @param passwordHash Хеш пароля.
     * @return ID пользователя при успехе, -1 при ошибке.
     */
    int authenticateUser(const std::string& username, const std::string& passwordHash);

    /**
     * @brief Создать новый личный чат.
     * @return ID созданного чата или -1 при ошибке.
     */
    int createPersonalChat();

    /**
     * @brief Получить ID сущестсвующего личного чата между двумя пользователями
     * @param userId1 ID первого пользователя
     * @param userId2 ID второго пользователя
     * @return ID личного чата или -1 при ошибке  
     */
    
    int getPersonalChat(int userId1, int userId2);

    /**
     * @brief Получить ID пользователя по логину.
     * @param username Логин.
     * @return ID пользователя или -1, если не найден.
     */
    int getUserIdByUsername(const std::string& username);

    /**
     * @brief Добавить пользователя в чат.
     * @param chatId ID чата.
     * @param userId ID пользователя.
     * @return true при успехе, false при ошибке.
     */
    bool addChatMember(int chatId, int userId);

    /**
     * @brief Сохранить сообщение в чате.
     * @param chatId ID чата.
     * @param senderId ID отправителя.
     * @param content Текст сообщения.
     * @return true при успехе, false при ошибке.
     */
    bool saveMessage(int chatId, int senderId, const std::string& content);


};

#endif