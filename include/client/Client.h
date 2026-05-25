#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstdint>

class Client {
private:
    int clientSocket; // Файловый дескриптор сокета клиента
    bool isConnected; // Флаг, указывающий, подключен ли клиент к серверу

public:
    /**
    * @brief Конструктор клиента
    */
    Client();

    /**
    * @brief Деструктор клиента
    */
    ~Client();

    /**
     * @brief Подключение к серверу
     * @param ip IP-адрес сервера
     * @param port Порт сервера
     * @return true, если подключение успешно, иначе false
     */
    bool connectToServer(const std::string& ip, uint16_t port);

    /**
     * @brief Отправка сообщения на сервер
     * @param message Сообщение для отправки
     * @return true, если сообщение успешно отправлено, иначе false
     */
    bool sendMessage(const std::string& message);

    /**
    * @brief Получение данных от сервера.
    * @return Строка с полученными данными
    */
    std::string receiveData();

    /**
     * @brief Получение сообщения от сервера
     */
    void disconnect();
};

#endif