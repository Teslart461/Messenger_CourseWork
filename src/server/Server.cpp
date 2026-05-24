#include "common/Logger.h"
#include "server/DataBaseManager.h"
#include "server/Server.h"

#include <iostream>
#include <cstring>

// Библиотеки для работы с сокетами
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

Server::Server(uint16_t port) : port(port), serverSocket(-1) {}

Server::~Server() {
    stop();
}

// Метод для запуска сервера: создание сокета, привязка и прослушивание
void Server::start() {
    // Создаем TCP сокет, где
    // AF_INET - используем IPv4
    // SOCK_STREAM - используем протокол TCP
    // 0 - IP-протокол по умолчанию
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Проверка успешности создания сокета
    if (serverSocket < 0) {
        Logger::getInstance().log("Ошибка создания сокета!", LogLevel::ERROR);
        return;
    }

    // Настройка сокета для повторного использования адреса (SO_REUSEADDR)
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        Logger::getInstance().log("Ошибка настройки setsockopt", LogLevel::ERROR);
        return;
    }

    // Настройка структуры для адреса сервера
    sockaddr_in serverAddress{};

    // AF_INET - IPv4, INADDR_ANY - слушаем на всех интерфейсах
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    
    // htons (Host TO Network Short) - переводит число порта в сетевой порядок байт (Big-Endian)
    serverAddress.sin_port = htons(port);

    // Привязываем сокет к адресу и порту
    if (bind(serverSocket, (sockaddr*)(&serverAddress), sizeof(serverAddress)) < 0) {
        Logger::getInstance().log("Ошибка привязки (bind)" + std::to_string(port), LogLevel::ERROR);
        return;
    }

    // Перевод сокета в режим ожидания подключений
    // SOMAXCONN - максимальное количество ожидающих соединений, поддерживаемое системой
    if (listen(serverSocket, SOMAXCONN) < 0) {
        Logger::getInstance().log("Ошибка прослушивания (listen)", LogLevel::ERROR);
        return;
    }

    Logger::getInstance().log("Сервер успешно запущен. Ожидание подключений на порту " + std::to_string(port) + "...", LogLevel::INFO);

    // Ожидаем клиента
    sockaddr_in clientAddress{};
    socklen_t clientSize = sizeof(clientAddress);

    // accept блокирует программу, пока кто-нибудь не подключится по сети
    int clientSocket = accept(serverSocket, (sockaddr*)(&clientAddress), &clientSize);
        
    if (clientSocket < 0) {
        Logger::getInstance().log("Ошибка при принятии подключения (accept)", LogLevel::ERROR);
    }

    Logger::getInstance().log("Клиент пожключился!", LogLevel::INFO);
         
    // Буфер для получения данных от клиента
    char buffer[1024] = {0};

    // recv - получает данные от клиента, блокирует программу, пока не придут данные
    ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

    // Проверяем, что данные были получены
    if (bytesReceived > 0) {
        std::string message(buffer, bytesReceived);

        Logger::getInstance().log("Получено сообщение: " + message, LogLevel::INFO);

        std::cout << "Получено: " << message << std::endl;
    }

    // Ещё поздороваемся с клиентом
    std::string welcomeMsg = "Привет от C++ сервера!\n";
    send(clientSocket, welcomeMsg.c_str(), welcomeMsg.length(), 0);

    close(clientSocket);
}

void Server::stop() {
    if (serverSocket != -1) {
        close(serverSocket);
        Logger::getInstance().log("Сервер остановлен", LogLevel::INFO);
    }
}