#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

/**
 * @brief Уровни важности сообщений лога.
 */
enum class LogLevel {
    DEBUG,  // Отладочная информация 
    INFO,  // Штатная работа сервера
    WARNING,  // Некритичные проблемы
    ERROR  // Критичные сбои
};

/**
 * @brief Класс для логирования системных событий.
 * Реализует паттерн Singleton, гарантируя существование только одного 
 * экземпляра логгера на всю программу. Поддерживает потокобезопасную запись.
 * ERROR-сообщения немедленно сбрасываются на диск.
 */
class Logger {
private:
    std::ofstream logFile;          // Файловый поток
    std::mutex logMutex;            // Защита от гонки данных

    /**
     * @brief Конструктор логера.
     * Открывает файл для дозаписи.
     */
    Logger();

    /**
     * @brief Деструктор логера.
     * Закрывает файловый поток при завершении программы.
     */
    ~Logger();

    // Запрет копирования и присваивания объекта
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief Форматирует сообщение: [2026-05-23 14:30:01] [INFO] Текст
     */
    std::string formatMessage(const std::string& message, LogLevel level);

    /**
     * @brief Текущее время.
     */
    static std::string getCurrentTimestamp();

    /**
     * @brief Строковое представление уровня.
     */
    static std::string levelToString(LogLevel level);

    // Зашитые настройки
    static constexpr const char* LOG_DIR  = "logs";
    static constexpr const char* LOG_FILE = "server.log";
    static constexpr LogLevel MIN_LEVEL   = LogLevel::DEBUG;
    static constexpr bool CONSOLE_OUTPUT  = true;

public:
    /**
     * @brief Единственный экземпляр логгера.
     * @return Ссылка на объект Logger.
     */
    static Logger& getInstance();

    /**
     * @brief Записать сообщение в лог.
     * @param message Текст.
     * @param level   Уровень важности (по умолчанию INFO).
     * 
     * Потокобезопасен. Сообщения ниже minLevel игнорируются.
     */
    void log(const std::string& message, LogLevel level = LogLevel::INFO);
};

#endif