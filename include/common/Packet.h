#ifndef PACKET_H
#define PACKET_H

#include <string>

#include "external/json.hpp"
#include "common/PacketType.h"
#include "common/PacketUtils.h"

// Для удобства использования json в коде, определим псевдоним типа json из библиотеки nlohmann
using json = nlohmann::json;

/**
* @brief Пакет для обмена данными между клиентом и сервером.
*/
struct Packet {
    PacketType type; // Тип пакета, определяющий его назначение
    json data; // Данные, содержащиеся в пакете, в формате JSON

    /**
     * Метод для сериализации пакета в строку JSON, которая может быть отправлена по сети
     * Сериализация включает в себя преобразование типа пакета в строку и упаковку данных в JSON-формат
     */
    std::string serialize() const {
        json j;
        j["type"] = packetTypeToString(type); // Преобразуем enum в строку для сериализации
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
        packet.type = stringToPacketType(j.at("type").get<std::string>()); // Преобразуем строку обратно в enum
        packet.data = j.at("data"); // Извлекаем данные из JSON, используя at() для проверки наличия ключа
        return packet; // Возвращаем десериализованный пакет

        } catch (const std::exception& e) {
            // В случае ошибки десериализации выбрасываем исключение с подробным сообщением
            throw std::runtime_error(std::string("Ошибка десериализации Packet: ")+ e.what());
        }
    }
};

#endif