#ifndef HASHER_H
#define HASHER_H

#include <string>

/**
 * @brief Утилита для хеширования паролей через SHA-256.
 */
class Hasher {
public:
    /**
     * @brief Хеширует строку с солью.
     * @param password Исходный пароль.
     * @param salt Соль (случайная строка).
     * @return SHA-256 хеш в виде hex-строки (64 символа).
     */
    static std::string sha256(const std::string& password, const std::string& salt);

    /**
     * @brief Генерирует случайную соль заданной длины.
     * @param length Длина соли в байтах (по умолчанию 16).
     * @return Случайная строка в hex-формате.
     */
    static std::string generateSalt(size_t length = 16);
};

#endif