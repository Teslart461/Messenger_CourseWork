#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <cstdint>

class Server {
private:
    int serverSocket; // Файловый дескриптор главного сокета
    uint16_t port; // Порт, на котором сервер будет слушать входящие соединения
    
public:
    /**
    * @brief Конструктор сервера.
    * @param port Порт, который будет слушать сервер
    */
    explicit Server(uint16_t port = 8080);

    /**
    * @brief Деструктор сервера.
    */
    ~Server();

    /**
    * @brief Запуск сервера (создание сокета, привязка и прослушивание).
    */
    void start();

    /**
    * @brief Остановка сервера.
    */
    void stop();
};

#endif