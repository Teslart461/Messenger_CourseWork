#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstdint>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include "common/Packet.h"

class Client {
private:
    int clientSocket; // Файловый дескриптор сокета клиента
    std::atomic<bool> isConnected{false}; // Флаг, указывающий, подключен ли клиент к серверу

    std::thread listenerThread; // Поток для прослушивания входящих сообщений от сервера
    std::atomic<bool> listening{false}; // Флаг для контроля работы потока прослушивания
    
    std::queue<Packet> responseQueue; // Очередь для хранения входящих сообщений от сервера
    std::mutex queueMutex; // Мьютекс для синхронизации доступа к очереди входящих сообщений
    std::condition_variable cv; // Условная переменная для уведомления о новых сообщениях в очереди
    
    /**
     * @brief Цикл приёма данных (выполняется в фоновом потоке).
     * @param onNewMessage Callback для асинхронных событий (NEW_MESSAGE).
     */
    void listenLoop(std::function<void(const Packet&)> onNewMessage);

    /**
     * @brief Принять одно сообщение с префиксом длины.
     * @return Строка с данными или пустая строка при ошибке.
     */
    std::string receiveRaw();

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
     * @brief Запустить фоновый поток-слушатель.
     * @param onNewMessage Callback для NEW_MESSAGE (вызывается в фоновом потоке).
     */
    void startListening(std::function<void(const Packet&)> onNewMessage);

    /**
     * @brief Дождаться ответа от сервера (блокирует главный поток).
     * @return Пакет с ответом.
     */
    Packet waitForResponse();

    /**
     * @brief Получение сообщения от сервера
     */
    void disconnect();
};

#endif