#ifndef PACKET_UTILS_H
#define PACKET_UTILS_H

#include <string>
#include <stdexcept>

#include "common/PacketType.h"

/**
 * @brief Преобразование PacketType -> string
 */
inline std::string packetTypeToString(PacketType type) {

    switch (type) {

        case PacketType::LOGIN:
            return "login";

        case PacketType::REGISTER:
            return "register";

        case PacketType::CREATE_CHAT:
            return "create_chat";

        case PacketType::SEND_MESSAGE:
            return "send_message";

        case PacketType::SERVER_MESSAGE:
            return "server_message";

        case PacketType::ERROR:
            return "error";

        default:
            return "unknown";
    }
}

/**
 * @brief Преобразование string -> PacketType
 */
inline PacketType stringToPacketType(const std::string& type) {

    if (type == "login") {
        return PacketType::LOGIN;
    }

    if (type == "register") {
        return PacketType::REGISTER;
    }

    if (type == "create_chat") {
        return PacketType::CREATE_CHAT;
    }

    if (type == "send_message") {
        return PacketType::SEND_MESSAGE;
    }

    if (type == "server_message") {
        return PacketType::SERVER_MESSAGE;
    }

    if (type == "error") {
        return PacketType::ERROR;
    }

    throw std::invalid_argument("Неизвестный тип пакета: " + type);
}

#endif