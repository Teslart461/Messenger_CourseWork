#include "common/Hasher.h"

#include <openssl/sha.h>
#include <openssl/rand.h>

#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>

std::string Hasher::sha256(const std::string& password, const std::string& salt) {
    // Конкатенируем пароль и соль для хеширования
    std::string data = password + salt;

    // Вычисляем SHA-256 хеш
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    // Преобразуем хеш в hex-строку
    std::stringstream ss;
    // Каждой байт хеша преобразуем в 2-значный hex и добавляем к строке
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    // Возвращаем итоговую hex-строку (64 символа для SHA-256)
    return ss.str();
}

std::string Hasher::generateSalt(size_t length) {
    // Генерируем случайную соль
    std::vector<unsigned char> bytes(length);

    // RAND_bytes - функция OpenSSL для генерации криптографически стойких случайных байт
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }

    // Конвертируем байты в hex
    std::stringstream ss;

    for (unsigned char byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    return ss.str();
}