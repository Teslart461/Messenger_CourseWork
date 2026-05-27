#include "client/Client.h"
#include "common/Logger.h"

#include <iostream>
#include <cstring>

// Библиотеки для работы с сокетами
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

Client::Client(): clientSocket(-1), isConnected(false) {}

Client::~Client() {
    disconnect();
}

// Метод для подключения к серверу
bool Client::connectToServer(const std::string& ip, uint16_t port) {
    // Создание сокета
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Проверка на успешное создание сокета
    if (clientSocket < 0) {
        Logger::getInstance().log("Ошибка создания сокета", LogLevel::ERROR);
        std::cerr << "Не удалось создать сокет." << std::endl;
        return false;
    }

    // Настройка адреса сервера
    sockaddr_in serverAddress{};

    // AF_INET - IPv4, htons (Host TO Network Short) - переводит число порта в сетевой порядок байт (Big-Endian)
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    // inet_pton - преобразует строковый IP-адрес в бинарный формат и сохраняет его в структуре serverAddress.sin_addr
    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        Logger::getInstance().log("Неверный IP-адрес", LogLevel::ERROR);
        std::cerr << "Неверный IP-адрес." << std::endl;
        close(clientSocket);
        clientSocket = -1;
        return false;
    }

    // Подключение к серверу (TCP Handshake)
    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        Logger::getInstance().log("Ошибка подключения к серверу", LogLevel::ERROR);
        std::cerr << "Не удалось подключиться к серверу." << std::endl;
        close(clientSocket);
        clientSocket = -1;
        return false;
    }

    isConnected = true;
    Logger::getInstance().log("Подключение к серверу успешно", LogLevel::INFO);
    std::cout << "Успешно подключился к серверу: " << ip << ":" << port << std::endl;
    return true;
}

// Метод для отправки сообщения на сервер
bool Client::sendMessage(const std::string& message) {
    if (!isConnected) {
        Logger::getInstance().log("Попытка отправить сообщение без подключения к серверу", LogLevel::WARNING);
        std::cerr << "Вы не подключены к серверу. Сначала подключитесь, а затем отправляйте сообщения." << std::endl;
        return false;
    }

    // Для надежной передачи данных по сети, особенно если сообщения могут быть длинными
    // полезно сначала отправлять размер сообщения, а затем само сообщение.
    // Это позволяет серверу знать, сколько байт ожидать.
    uint32_t messageSize = htonl(message.size());

    // Сначала отправляем размер
    if (send(clientSocket, &messageSize, sizeof(messageSize), 0) <= 0) {
        return false;
    }

    // Затем отправляем само сообщение
    if (send(clientSocket, message.c_str(), message.size(), 0) <= 0) {
        return false;
    }

    return true;
}

std::string Client::receiveRaw() {
    if (!isConnected) return "";

    // Читаем префикс длины
    uint32_t messageSize = 0;
    ssize_t sizeRead = recv(clientSocket, &messageSize, sizeof(messageSize), MSG_WAITALL);

    if (sizeRead <= 0) {
        isConnected = false;
        return "";
    }

    uint32_t packetSize = ntohl(messageSize);

    // Читаем тело сообщения
    std::string buffer(packetSize, '\0');
    ssize_t bytesRead = recv(clientSocket, buffer.data(), packetSize, MSG_WAITALL);

    if (bytesRead <= 0) {
        isConnected = false;
        return "";
    }

    return buffer;
}

void Client::startListening(std::function<void(const Packet&)> onNewMessage) {
    if (!isConnected) return;

    listening = true;
    // Запускаем фоновый поток для прослушивания сообщений от сервера
    listenerThread = std::thread(&Client::listenLoop, this, onNewMessage);
}

void Client::listenLoop(std::function<void(const Packet&)> onNewMessage) {
    while (listening && isConnected) {
        std::string rawData = receiveRaw();
        if (rawData.empty()) {
            // Соединение разорвано
            if (listening) {
                std::cerr << "\n[СИСТЕМА] Потеряно соединение с сервером.\n";
                isConnected = false;
                cv.notify_all();  // Разблокируем главный поток
            }
            break;
        }

        try {
            Packet incomingPacket = Packet::deserialize(rawData);

            // Диспетчеризация пакетов
            if (incomingPacket.type == PacketType::NEW_MESSAGE) {
                // Асинхронное событие — немедленно вызываем callback
                if (onNewMessage) {
                    onNewMessage(incomingPacket);
                }
            } else {
                // Ответ на запрос главного потока — кладём в очередь
                std::lock_guard<std::mutex> lock(queueMutex);
                responseQueue.push(incomingPacket);
                cv.notify_one();  // Будим главный поток
            }
        } catch (const std::exception& e) {
            Logger::getInstance().log("Ошибка десериализации: " + std::string(e.what()), LogLevel::ERROR);
        }
    }
}

Packet Client::waitForResponse() {
    std::unique_lock<std::mutex> lock(queueMutex);

    // Ждём, пока в очереди появится пакет или соединение не закроется
    cv.wait(lock, [this]() {
        return !responseQueue.empty() || !isConnected;
    });

    if (!isConnected && responseQueue.empty()) {
        // Соединение потеряно, а ответа нет — возвращаем пустой пакет ошибки
        Packet errorPacket;
        errorPacket.type = PacketType::ERROR_RESPONSE;
        errorPacket.data["message"] = "Соединение с сервером потеряно.";
        return errorPacket;
    }

    Packet resp = responseQueue.front();
    responseQueue.pop();
    return resp;
}

void Client::disconnect() {
    // Проверяем, что сокет открыт и клиент подключен, прежде чем пытаться отключиться
    if (!isConnected.exchange(false)) {
        return;
    }

    // Устанавливаем флаг listening в false, чтобы остановить фоновый поток прослушивания
    listening = false;

    // Разбудить recv()
    if (clientSocket >= 0) {
        shutdown(clientSocket, SHUT_RDWR);
    }

    // Будим главный поток, если он ждал ответа
    cv.notify_all();

    // Дожидаемся потока
    if (listenerThread.joinable()) {
        listenerThread.join();
    }

    // Теперь окончательно закрываем 
    if (clientSocket >= 0) {
        close(clientSocket);
        clientSocket = -1;
    }

    Logger::getInstance().log("Отключение от сервера", LogLevel::INFO);
    std::cout << "Отключился от сервера." << std::endl;
}