#ifndef PACKET_TYPE_H
#define PACKET_TYPE_H

/**
 * @brief Типы пакетов для обмена между клиентом и сервером.
 */
enum class PacketType {
    LOGIN,
    REGISTER,
    CREATE_CHAT,
    SEND_MESSAGE,
    SERVER_MESSAGE,
    ERROR
};

#endif