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
        return false;
    }

    // Подключение к серверу (TCP Handshake)
    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        Logger::getInstance().log("Ошибка подключения к серверу", LogLevel::ERROR);
        std::cerr << "Не удалось подключиться к серверу." << std::endl;
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

std::string Client::receiveData() {
    // Проверяем, что клиент подключен, прежде чем пытаться получить данные от сервера
    if (!isConnected) {
        return "";
    }

    // Получаем данные от сервера. Для надежности сначала читаем размер сообщения, а затем само сообщение
    uint32_t messageSize = 0;

    // Получаем размер сообщения
    // MSG_WAITALL - флаг, который заставляет recv ждать, пока не будет получено указанное количество байт
    ssize_t sizeRead = recv(clientSocket, &messageSize, sizeof(messageSize), MSG_WAITALL);

    if (sizeRead <= 0) {
        return "";
    }

    // ntohl (Network TO Host Long) - переводит число из сетевого порядка байт (Big-Endian) в порядок байт хоста
    uint32_t packetSize = ntohl(messageSize);

    // Создаем строку нужного размера для получения сообщения
    std::string buffer(packetSize, '\0');

    // Получаем всё сообщение
    ssize_t bytesRead = recv(clientSocket, buffer.data(), packetSize, MSG_WAITALL);

    if (bytesRead <= 0) {
        return "";
    }

    return buffer;
}

void Client::disconnect() {
    // Проверяем, что сокет открыт и клиент подключен, прежде чем пытаться отключиться
    if (isConnected) {
        close(clientSocket);
        clientSocket = -1;
        isConnected = false;

        Logger::getInstance().log("Отключение от сервера", LogLevel::INFO);
        std::cout << "Отключился от сервера." << std::endl;
    }
}