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

    // send - отправляет данные на сервер. Возвращает количество байт, отправленных на сервер
    ssize_t bytesSent = send(clientSocket, message.c_str(), message.length() + 1,0);
    return bytesSent > 0;
}

std::string Client::receiveData() {
    // Проверяем, что клиент подключен, прежде чем пытаться получить данные от сервера
    if (!isConnected) {
        return "";
    }

    // Буфер для получения данных от сервера
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer)); // Инициализация буфера нулями

    // recv - получает данные от сервера и сохраняет их в буфер. Возвращает количество байт, полученных от сервера
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    // Проверка на ошибки при получении данных
    if (bytesRead <= 0) {
        Logger::getInstance().log("Ошибка при получении данных от сервера", LogLevel::ERROR);
        std::cerr << "Ошибка при получении данных от сервера." << std::endl;
        return "";
    }

    // Преобразуем буфер в строку и возвращаем её
    return std::string(buffer);
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