#include "client/Client.h"
#include "common/Packet.h"

#include <string>
#include <iostream>

int main() {
    std::locale::global(std::locale(""));
    Client client;

    // Подключаемся
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    // Состояние сессии клиента
    int myUserId = -1; // Идентификатор пользователя
    int myChatId = -1; // Идентификатор чата

    while (true) {
        std::cout << "Добро пожаловать!" << std::endl;
        std::cout << "Текущий статус: " << (myUserId != -1 ? "В сети (ID: " + std::to_string(myUserId) + ")" : "Гость") << std::endl;
        std::cout << "1. Войти в систему" << std::endl;
        std::cout << "2. Зарегистрироваться" << std::endl;
        std::cout << "3. Создать чат" << std::endl;
        std::cout << "4. Отправить сообщение" << std::endl;
        std::cout << "5. Выйти" << std::endl;
        std::cout << "Выберите действие: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Очистка буфера ввода после чтения числа

        Packet requestPacket;
        if (choice == 1) {
            // Вход в систему
            std::string login, password;
            std::cout << "Введите имя пользователя: ";
            std::getline(std::cin, login);
            std::cout << "Введите пароль: ";
            std::getline(std::cin, password);

            requestPacket.type = PacketType::LOGIN;
            requestPacket.data["username"] = login;
            requestPacket.data["password"] = password;

        } else if (choice == 2) {
            // Регистрация
            std::string login, password;
            std::cout << "Введите имя пользователя: ";
            std::getline(std::cin, login);
            std::cout << "Введите пароль: ";
            std::getline(std::cin, password);

            requestPacket.type = PacketType::REGISTER;
            requestPacket.data["username"] = login;
            requestPacket.data["password"] = password;

        } else if (choice == 3) {
            if (myUserId == -1) {
                std::cout << "Сначала нужно войти в аккаунт!" << std::endl;
                continue;
            }
            // Создание чата
            std::string targetUsername;
            std::cout << "Введите логин пользователя, с кем хотите начать чат: "; 
            std::getline(std::cin, targetUsername);

            requestPacket.type = PacketType::CREATE_CHAT;
            requestPacket.data["sender_id"] = myUserId; // Сервер должен знать, кто создает чат
            requestPacket.data["target_username"] = targetUsername;

        } else if (choice == 4) {
            // Отправка сообщения
            if (myChatId == -1 || myUserId == -1) {
                std::cout << "Сначала нужно войти в аккаунт и выбрать чат!" << std::endl;
                continue;
            }
            std::string messageText;

            std::cout << "Введите сообщение: ";
            std::getline(std::cin, messageText);

            requestPacket.type = PacketType::SEND_MESSAGE;
            requestPacket.data["sender_id"] = myUserId;
            requestPacket.data["chat_id"] = myChatId;
            requestPacket.data["message"] = messageText;

        } else if (choice == 5) {
            break; // Выход из цикла и завершение программы
        } else {
            std::cout << "Неверный выбор. Пожалуйста, попробуйте снова." << std::endl;
            continue;
        }
    
        // Отправляем запрос на сервер
        client.sendMessage(requestPacket.serialize());

        // Получаем ответ от сервера
        std::string responseStr = client.receiveData();
            if (responseStr.empty()) {  
            std::cout << "Нет ответа от сервера. Возможно, соединение было потеряно." << std::endl;
            break;
        }

        // Десериализуем ответ от сервера в объект Packet
        Packet responsePacket;
        try {
            responsePacket = Packet::deserialize(responseStr);
        } catch (const std::exception& e) {
            std::cout << "Ошибка при обработке ответа от сервера: " << e.what() << std::endl;
            continue;
        }

        // Обрабатываем ответ от сервера в зависимости от типа ответа
        if (responsePacket.type == PacketType::SUCCESS_RESPONSE) {
            std::cout << "\n>>> УСПЕХ: " << responsePacket.data["message"].get<std::string>() << std::endl;
            if (responsePacket.data.contains("user_id")) {
                myUserId = responsePacket.data["user_id"].get<int>();
            }
            if (responsePacket.data.contains("chat_id")) {
                myChatId = responsePacket.data["chat_id"].get<int>();
            }
        } else if (responsePacket.type == PacketType::ERROR_RESPONSE) {
            std::cout << "\n>>> ОШИБКА: " << responsePacket.data["message"].get<std::string>() << std::endl;
        } else {
            std::cout << "\n>>> НЕИЗВЕСТНЫЙ ОТВЕТ ОТ СЕРВЕРА" << std::endl;
        }
}
    // После обработки ответа от сервера, цикл повторяется, и пользователь может выбрать следующее действие
    client.disconnect();
    return 0;
}