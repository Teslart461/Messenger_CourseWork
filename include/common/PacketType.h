#ifndef PACKET_TYPE_H
#define PACKET_TYPE_H

/**
 * @brief Типы пакетов для обмена между клиентом и сервером.
 */
enum class PacketType {
    REGISTER,               // Запрос: Регистрация нового пользователя
    LOGIN,                  // Запрос: Аутентификация существующего пользователя
    CREATE_CHAT,            // Запрос: Создание нового чата
    SEND_MESSAGE,           // Запрос: Отправка сообщения 
    SUCCESS_RESPONSE,       // Ответ сервера об успешной операции
    ERROR_RESPONSE,         // Ответ сервера об ошибке
};

#endif