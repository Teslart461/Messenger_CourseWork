#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <cstdint>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>

class Server {
private:
    int serverSocket; // Файловый дескриптор главного сокета
    uint16_t port; // Порт, на котором сервер будет слушать входящие соединения
    bool isRunning; // Флаг для контроля работы сервера
    std::atomic<bool> shutdownFlag{false}; // Флаг для безопасного завершения работы сервера

    // Хранилище онлайн-пользователей: userId -> clientSocket
    std::unordered_map<int, int> onlineUsers;
    std::mutex onlineUsersMutex;
    
    /**
    * @brief Метод для обслуживания конкретного клиента.
    * Запускается в отдельном потоке.
    * @param clientSocket Сокет подключенного клиента.
    */
    void handleClient(int clientSocket);

    /**
     * @brief Добавляет пользователя в список онлайн-пользователей.
     * @param userId ID пользователя, который вошел в систему.
     * @param clientSocket Сокет, связанный с этим пользователем.
     */
    void addOnlineUser(int userId, int clientSocket);

    /**
     * @brief Удаляет пользователя из списка онлайн-пользователей.
     * @param userId ID пользователя, который вышел из системы.
     */
    void removeOnlineUser(int userId);

    /**
     * @brief Получает сокет онлайн-пользователя по его ID.
     * @param userId ID пользователя, для которого нужно получить сокет.
     */
    int getOnlineUserSocket(int userId);

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