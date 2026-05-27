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

        case PacketType::REGISTER:
            return "register";

        case PacketType::LOGIN:
            return "login";

        case PacketType::LOGOUT:
            return "logout";

        case PacketType::CREATE_CHAT:
            return "create_chat";

        case PacketType::SEND_MESSAGE:
            return "send_message";

        case PacketType::NEW_MESSAGE:
            return "new_message";

        case PacketType::GET_CHATS:
            return "get_chats";

        case PacketType::GET_CHAT_HISTORY:
            return "get_chat_history";

        case PacketType::CHAT_LIST_RESPONSE:
            return "chat_list_response";
        
        case PacketType::HISTORY_RESPONSE:
            return "history_response";

        case PacketType::SUCCESS_RESPONSE:
            return "success_response";

        case PacketType::ERROR_RESPONSE:
            return "error_response";

        default:
            return "unknown";
    }
}

/**
 * @brief Преобразование string -> PacketType
 */
inline PacketType stringToPacketType(const std::string& type) {

    if (type == "register") {
        return PacketType::REGISTER;
    }

    if (type == "login") {
        return PacketType::LOGIN;
    }

    if (type == "logout") {
        return PacketType::LOGOUT;
    }

    if (type == "create_chat") {
        return PacketType::CREATE_CHAT;
    }

    if (type == "send_message") {
        return PacketType::SEND_MESSAGE;
    }

    if (type == "new_message") {
        return PacketType::NEW_MESSAGE;
    }

    if (type == "get_chats") {
        return PacketType::GET_CHATS;
    }

    if (type == "get_chat_history") {
        return PacketType::GET_CHAT_HISTORY;
    }

    if (type == "chat_list_response") {
        return PacketType::CHAT_LIST_RESPONSE;
    }

    if (type == "history_response") {
        return PacketType::HISTORY_RESPONSE;
    }

    if (type == "success_response") {
        return PacketType::SUCCESS_RESPONSE;
    }

    if (type == "error_response") {
        return PacketType::ERROR_RESPONSE;
    }

    throw std::invalid_argument("Неизвестный тип пакета: " + type);
}

#endif