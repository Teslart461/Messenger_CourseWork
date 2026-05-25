#ifndef PACKET_H
#define PACKET_H

#include <string>
#include "external/json.hpp"

// Для удобства использования json в коде, определим псевдоним типа json из библиотеки nlohmann
using json = nlohmann::json;

// Определение типов пакетов, которые будут использоваться для обмена данными между клиентом и сервером
enum class PacketType {
    REGISTER,
    LOGIN,
    CREATE_CHAT,
    SEND_MESSAGE,
    SERVER_MESSAGE,
    ERROR
};

struct Packet {
    PacketType type; // Тип пакета, определяющий его назначение
    json data; // Данные, содержащиеся в пакете, в формате JSON

    /**
     * Метод для сериализации пакета в строку JSON, которая может быть отправлена по сети
     * Сериализация включает в себя преобразование типа пакета в строку и упаковку данных в JSON-формат
     */
    std::string serialize() const {
        json j;
        j["type"] = static_cast<int>(type); // Преобразуем enum в int для сериализации
        j["data"] = data; // Упаковываем данные в JSON
        return j.dump(); // Преобразуем JSON-объект в строку
    }

    /**
     * Метод для десериализации строки JSON в объект пакета
     * Десериализация включает в себя преобразование строки JSON в объект и извлечение данных
     */
    static Packet deserialize(const std::string& rawData) {
        
        try {
        auto j = json::parse(rawData); // Парсим строку JSON в объект
        Packet packet;
        packet.type = static_cast<PacketType>(j["type"].get<int>()); // Преобразуем int обратно в enum
        packet.data = j.at("data"); // Извлекаем данные из JSON, используя at() для проверки наличия ключа
        return packet; // Возвращаем десериализованный пакет

        } catch (const std::exception& e) {
            // В случае ошибки десериализации выбрасываем исключение с подробным сообщением
            throw std::runtime_error(std::string("Ошибка десериализации Packet: ")+ e.what());
        }
    }
};

#endif