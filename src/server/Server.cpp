#include "common/Logger.h"
#include "server/DataBaseManager.h"
#include "server/Server.h"
#include "common/Packet.h"

#include <iostream>
#include <cstring>

// Библиотеки для работы с сокетами
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

Server::Server(uint16_t port) : port(port), serverSocket(-1), isRunning(false) {}

Server::~Server() {
    stop();
}

// Метод для запуска сервера: создание сокета, привязка и прослушивание
void Server::start() {
    // Создаем TCP сокет, где
    // AF_INET - IPv4
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

        // Закрываем сокет, если привязка не удалась
        close(serverSocket);
        serverSocket = -1;

        return;
    }

    // Перевод сокета в режим ожидания подключений
    // SOMAXCONN - максимальное количество ожидающих соединений, поддерживаемое системой
    if (listen(serverSocket, SOMAXCONN) < 0) {
        Logger::getInstance().log("Ошибка прослушивания (listen)", LogLevel::ERROR);
        return;
    }

    Logger::getInstance().log("Сервер успешно запущен. Ожидание подключений на порту " + std::to_string(port) + "...", LogLevel::INFO);
    isRunning = true;

    // Бесконечный цикл для принятия клиентов (accept)
    while (isRunning) {
        sockaddr_in clientAddress{};
        socklen_t clientSize = sizeof(clientAddress);

        // accept блокирует программу, пока кто-нибудь не подключится по сети
        int clientSocket = accept(serverSocket, (sockaddr*)(&clientAddress), &clientSize);
        
        if (clientSocket < 0) {
            if (isRunning) { // Если сервер всё ещё работает, логируем ошибку
                Logger::getInstance().log("Ошибка при принятии подключения (accept)", LogLevel::ERROR);
            }
            continue;
        }

        Logger::getInstance().log("Клиент подключился!", LogLevel::INFO);
         
        // Запускаем новый поток для обслуживания клиента, передавая ему сокет клиента
        // Создаем новый поток, передаем ему указатель на метод handleClient, а также указатель на текущий экземпляр Server (this) и сокет клиента
        std::thread clientThread(&Server::handleClient, this, clientSocket);

        // Отделяем поток для обслуживания клиента от основного потока сервера, чтобы он мог продолжать принимать новых клиентов
        // Поток будет работать отдельно и сам очистит память, когда завершит работу
        clientThread.detach();
    }   
}

// Метод для обслуживания конкретного клиента. Запускается в отдельном потоке.
void Server::handleClient(int clientSocket) {
    Logger::getInstance().log("Начало обслуживания клиента в новом потоке. Для обслуживаемого сокета: " + std::to_string(clientSocket), LogLevel::DEBUG);

    // Отправляем приветственное сообщение клиенту
    std::string welcomeMessage = "Добро пожаловать на сервер! Вы в отдельном потоке. Ожидаю ваш JSON пакет...";
    send(clientSocket, welcomeMessage.c_str(), welcomeMessage.length() + 1, 0);

    // Буфер для приема данных
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    // recv - получает данные от клиента и сохраняет их в буфер. Возвращает количество байт, полученных от клиента
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    // Проверка на ошибки при получении данных
    if (bytesRead > 0) {
        std::string receivedData(buffer);
        Logger::getInstance().log("Сырые данные от клиента: " + receivedData, LogLevel::DEBUG);

        try {
            // Пытаемся десериализовать полученные данные в объект Packet
            Packet incomingPacket = Packet::deserialize(receivedData);

            // Проверяем тип пакета и обрабатываем его соответствующим образом
            if (incomingPacket.type == PacketType::SEND_MESSAGE) {
                // Извлекаем данные из пакета и логируем их
                int senderId = incomingPacket.data["sender_id"];
                std::string text = incomingPacket.data["text"];

                Logger::getInstance().log("Получено сообщение: [От ID " + std::to_string(senderId) + "]: " + text, LogLevel::INFO);

                // Отправляем ответ клиенту, подтверждая получение сообщения
                Packet response;
                response.type = PacketType::SERVER_MESSAGE;
                response.data["status"] = "OK";
                response.data["info"] = "Сообщение обработано сервером";

                // Сериализуем ответ в строку JSON и отправляем его клиенту
                std::string responseStr = response.serialize();
                send(clientSocket, responseStr.c_str(), responseStr.length() + 1, 0);
            }
        } catch (const std::exception& e) {
            // Если произошла ошибка при десериализации JSON, логируем её и отправляем клиенту сообщение об ошибке
            Logger::getInstance().log("Ошибка обработки JSON: " + std::string(e.what()), LogLevel::ERROR);
            
            Packet error;
            error.type = PacketType::ERROR;
            error.data["info"] = "Неверный формат JSON";
            std::string errorStr = error.serialize();
            send(clientSocket, errorStr.c_str(), errorStr.length() + 1, 0);
        }
    } 
    else if (bytesRead == 0) {
        Logger::getInstance().log("Клиент разорвал соединение до отправки данных.", LogLevel::WARNING);
    } 
    else {
        Logger::getInstance().log("Ошибка при чтении из сокета (recv).", LogLevel::ERROR);
    }

    // Закрываем сокет клиента после обслуживания
    close(clientSocket);
    Logger::getInstance().log("Клиент отключился. Поток завершён. Сокет закрыт.", LogLevel::INFO);
}

void Server::stop() {
    if (isRunning) {
        isRunning = false;
        // Закрываем главный сокет сервера
        if (serverSocket != -1) {
            close(serverSocket); // close - закрывает файловый дескриптор сокета
            serverSocket = -1;
        }
        Logger::getInstance().log("Сервер остановлен", LogLevel::INFO);
    }
}