#include "common/Logger.h"
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <filesystem>

Logger::Logger() {
    namespace fs = std::filesystem;

    // Создаём директорию
    if (!fs::exists(LOG_DIR)) {
        fs::create_directories(LOG_DIR);
    }

    // Открываем файл, дозаписывая в него
    std::string fullPath = std::string(LOG_DIR) + "/" + LOG_FILE;
    logFile.open(fullPath, std::ios::app);

    if (!logFile.is_open()) {
        std::cerr << "[Логгер] CRITICAL: Невозможно открыть файл для логов: " << fullPath << std::endl;
    }
}

// При уничтожении объекта закрываем файл
Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

// Реализация метода получения Singleton-объекта
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::log(const std::string& message, LogLevel level) {
    // Фильтрация по уровню
    if (static_cast<int>(level) < static_cast<int>(MIN_LEVEL)) {
        return;
    }

    // std::lock_guard автоматически захватывает мьютекс.
    // Если другой поток попытается вызвать log(), он уснет на этой строке и
    // будет ждать, пока первый поток не закончит запись и не выйдет из функции.
    std::lock_guard<std::mutex> lock(logMutex);

    // Формируем строку
    std::string formatted = formatMessage(message, level);

    // Выводим в консоль, если есть соответствующий флаг
    if (CONSOLE_OUTPUT) {
        std::cout << formatted << std::endl;
    }

    // Пишем в файл
    if (logFile.is_open()) {
        logFile << formatted << std::endl;

        if (level == LogLevel::ERROR) {
            logFile.flush();   // Мгновенный сброс при ошибках
        }
    }
}

std::string Logger::formatMessage(const std::string& message, LogLevel level) {
    return "[" + getCurrentTimestamp() + "] [" + levelToString(level) + "] " + message;
}

std::string Logger::getCurrentTimestamp() {
    using namespace std::chrono;

    // Получаем текущее время с точностью до системных часов
    auto now = system_clock::now();

    // Преобразуем в формат time_t
    auto in_time_t = system_clock::to_time_t(now);

    std::stringstream ss;

    // std::put_time форматирует время по заданному шаблону
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

    return ss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "????";
    }
}