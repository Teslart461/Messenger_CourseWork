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

    while(true){
        // Для надежной передачи данных по сети, особенно если сообщения могут быть длинными
        // полезно сначала отправлять размер сообщения, а затем само сообщение.
        // Это позволяет серверу знать, сколько байт ожидать.
        uint32_t packetSizeNetwork;

        // Получаем размер пакета от клиента. Используем MSG_WAITALL, чтобы гарантировать получение всех байт размера
        ssize_t sizeRead = recv(clientSocket, &packetSizeNetwork, sizeof(packetSizeNetwork), MSG_WAITALL);

        // Проверяем, что клиент не отключился и данные были получены успешно
        if (sizeRead <= 0) {
            Logger::getInstance().log("Клиент отключился.", LogLevel::WARNING);
            break;
        }

        // Переводим размер из network byte order
        uint32_t packetSize = ntohl(packetSizeNetwork);

        // Создаем строку нужного размера для получения пакета
        std::string receivedData(packetSize, '\0');

        // Получаем весь пакет от клиента. Используем MSG_WAITALL, чтобы гарантировать получение всех байт пакета
        ssize_t bytesRead = recv(clientSocket, receivedData.data(), packetSize, MSG_WAITALL);

        if (bytesRead <= 0) { 
            Logger::getInstance().log("Ошибка получения данных.", LogLevel::ERROR);
            break;
        }

        Logger::getInstance().log("Получены данные от клиента: " + receivedData, LogLevel::DEBUG);

        // Десериализуем полученные данные в объект Packet
        Packet incomingPacket;
        try {
            incomingPacket = Packet::deserialize(receivedData);
        } catch (const std::exception& e) {
            Logger::getInstance().log("Ошибка при десериализации пакета: " + std::string(e.what()), LogLevel::ERROR);
            continue;
        }
        
        try {
        // Готовим ответный пакет для отправки клиенту
        Packet responsePacket;

        // Обрабатываем пакет в зависимости от его типа
        if (incomingPacket.type == PacketType::LOGIN) {
            std::string username = incomingPacket.data["username"];
            std::string password = incomingPacket.data["password"];
                
            int userId = DataBaseManager::getInstance().authenticateUser(username, password);
            if (userId != -1) {
                responsePacket.type = PacketType::SUCCESS_RESPONSE;
                responsePacket.data["user_id"] = userId;
                responsePacket.data["message"] = "Успешный вход!";
            } else {
                responsePacket.type = PacketType::ERROR_RESPONSE;
                responsePacket.data["message"] = "Неверный логин или пароль.";
            }



        } else if (incomingPacket.type == PacketType::REGISTER) {
            std::string username = incomingPacket.data["username"];
            std::string password = incomingPacket.data["password"];

            bool success = DataBaseManager::getInstance().registerUser(username, password);
            if (success) {
                responsePacket.type = PacketType::SUCCESS_RESPONSE;
                responsePacket.data["message"] = "Регистрация прошла успешно!";
            } else {
                responsePacket.type = PacketType::ERROR_RESPONSE;
                responsePacket.data["message"] = "Ошибка регистрации. Возможно, логин уже используется.";
            }



        } else if (incomingPacket.type == PacketType::CREATE_CHAT) {
            int senderId = incomingPacket.data["sender_id"];
            std::string targetUsername = incomingPacket.data["target_username"];

            int targetId = DataBaseManager::getInstance().getUserIdByUsername(targetUsername);

            if (targetId == -1) {
                responsePacket.type = PacketType::ERROR_RESPONSE;
                responsePacket.data["message"] = "Пользователь '" + targetUsername + "' не найден.";
            } else if (senderId == targetId) {
                responsePacket.type = PacketType::ERROR_RESPONSE;
                responsePacket.data["message"] = "Нельзя создать чат с самим собой.";
            } else {
                // Сначала проверяем существует ли чат между этими пользователями,
                // если нет, создаём новый
                int existingChatId = DataBaseManager::getInstance().getPersonalChat(senderId, targetId);

                if (existingChatId != -1) {
                    // Чат уже существует, выдаем его ID без создания нового
                    responsePacket.type = PacketType::SUCCESS_RESPONSE;
                    responsePacket.data["chat_id"] = existingChatId;
                    responsePacket.data["message"] = "Чат с " + targetUsername + " уже существует. Вы вошли в него.";
                } else {
                    // Чата нет, создаем новый
                    int newChatId = DataBaseManager::getInstance().createPersonalChat();
                    if (newChatId != -1) {
                        DataBaseManager::getInstance().addChatMember(newChatId, senderId);
                        DataBaseManager::getInstance().addChatMember(newChatId, targetId);

                        responsePacket.type = PacketType::SUCCESS_RESPONSE;
                        responsePacket.data["chat_id"] = newChatId;
                        responsePacket.data["message"] = "Чат с " + targetUsername + " успешно создан!";
                    } else {
                        responsePacket.type = PacketType::ERROR_RESPONSE;
                        responsePacket.data["message"] = "Ошибка БД при создании чата.";
                    }
                }
            }


    
        } else if (incomingPacket.type == PacketType::SEND_MESSAGE) {
            int senderId = incomingPacket.data["sender_id"];
            int chatId = incomingPacket.data["chat_id"];
            std::string text = incomingPacket.data["message"];

            bool isSaved = DataBaseManager::getInstance().saveMessage(chatId, senderId, text);
            if (isSaved) {
                responsePacket.type = PacketType::SUCCESS_RESPONSE;
                responsePacket.data["status"] = "OK";
                responsePacket.data["message"] = "Сообщение сохранено";
            } else {
                responsePacket.type = PacketType::ERROR_RESPONSE;
                responsePacket.data["message"] = "Ошибка БД при сохранении сообщения";
            }



        } else if (incomingPacket.type == PacketType::GET_CHATS) {
            int userId = incomingPacket.data["user_id"];
                
            // Достаем вектор чатов из БД
            auto chats = DataBaseManager::getInstance().getUserChats(userId);

            // Формируем JSON-массив
            json chatArray = json::array();
            for (const auto& chat : chats) {
                chatArray.push_back({
                    {"chat_id", chat.chatId}, 
                    {"chat_name", chat.chatName}
                });
            }

            responsePacket.type = PacketType::CHAT_LIST_RESPONSE;
            responsePacket.data["chats"] = chatArray;
            responsePacket.data["message"] = "Список чатов получен";



        } else if (incomingPacket.type == PacketType::GET_CHAT_HISTORY) {
                int chatId = incomingPacket.data["chat_id"];
                
                // Достаем вектор сообщений из БД
                auto history = DataBaseManager::getInstance().getChatHistory(chatId);

                // Формируем JSON-массив сообщений
                json historyArray = json::array();
                for (const auto& msg : history) {
                    historyArray.push_back({
                        {"sender_id", msg.senderId},
                        {"content", msg.content},
                        {"timestamp", msg.timestamp}
                    });
                }

                responsePacket.type = PacketType::HISTORY_RESPONSE;
                responsePacket.data["history"] = historyArray;
                responsePacket.data["message"] = "История сообщений получена";
            }
            
        // Отправляем ответ клиенту
        std::string responseStr = responsePacket.serialize();

        // Для надежной передачи данных по сети, особенно если сообщения могут быть длинными
        // полезно сначала отправлять размер сообщения, а затем само сообщение. 
        // Это позволяет клиенту знать, сколько байт ожидать.
        uint32_t responseSize = htonl(responseStr.size());

        // Отправляем размер ответа, а затем само сообщение
        send(clientSocket, &responseSize, sizeof(responseSize), 0);
        
        // send - отправляет данные клиенту. Возвращает количество байт, отправленных клиенту
        ssize_t bytesSent = send(clientSocket, responseStr.c_str(), responseStr.size(), 0);

        if (bytesSent < 0) {
            Logger::getInstance().log("Ошибка при отправке данных клиенту.", LogLevel::ERROR);
        } else {
            Logger::getInstance().log("Ответ успешно отправлен клиенту: " + responseStr, LogLevel::DEBUG);
        }

        } catch (const std::exception& e) {
            Logger::getInstance().log("Ошибка JSON: " + std::string(e.what()), LogLevel::ERROR);
            Packet error;
            error.type = PacketType::ERROR_RESPONSE;
            error.data["message"] = "Неверный формат пакета";
            std::string errorStr = error.serialize();

            uint32_t errorSize = htonl(errorStr.size());

            // Отправляем размер ответа, а затем само сообщение
            send(clientSocket, &errorSize, sizeof(errorSize), 0);

            send(clientSocket, errorStr.c_str(), errorStr.size(), 0);
        }
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