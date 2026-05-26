#include "client/Client.h"
#include "common/Packet.h"

#include <string>
#include <iostream>

/**
 * @brief Состояния интерфейса приложения
 */
enum class AppState {
    AUTH,       // Экран логина/регистрации
    MAIN_MENU,  // Экран списка чатов
    IN_CHAT     // Экран внутри конкретного диалога
};

/**
 * @brief Очистка консоли
 */
void clearConsole() {
    system("clear");
}

/**
 * @brief Ожидание нажатия Enter для продолжения.
 */
void pressEnterToContinue() {
    std::cout << "\nНажмите Enter, чтобы продолжить...";
    std::cin.get();
}

/**
 * @brief Безопасное чтение целого числа из cin.
 * @return Введённое число.
 */
int readInt() {
    int value;
    while (!(std::cin >> value)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Неверный ввод. Введите число: ";
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return value;
}

int main() {
    std::locale::global(std::locale(""));
    Client client;

    // Подключаемся
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    // Состояние сессии клиента
    AppState currentState = AppState::AUTH; // Изначально пользователь должен авторизоваться или зарегистрироваться
    int myUserId = -1; // Идентификатор пользователя
    int myChatId = -1; // Идентификатор чата
    std::string currentChatName; // Название текущего чата

    while (true) {
        clearConsole();

        /**
         * В зависимости от текущего состояния интерфейса, отображаем соответствующее меню и обрабатываем действия пользователя.
         */

        /**
         * 1. Экран авторизации (AUTH):
         */
        if (currentState == AppState::AUTH) {
            std::cout << "\n=== TeslaMAX++ ===\n\n";
            std::cout << "1. Войти\n";
            std::cout << "2. Зарегистрироваться\n";
            std::cout << "0. Выйти\n\n";
            std::cout << "Выберите действие: ";

            int choice = readInt();

            if (choice == 0) {
                break; // Выход из приложения
            }

            if (choice != 1 && choice != 2) {
                std::cout << "Неверный выбор. Попробуйте снова.\n";
                pressEnterToContinue();
                continue;
            }

            std::string login, password;
            std::cout << "Введите логин: ";
            std::getline(std::cin, login);
            std::cout << "Введите пароль: ";
            std::getline(std::cin, password);

            if (login.empty() || password.empty()) {
                std::cout << "Логин и пароль не могут быть пустыми.\n";
                pressEnterToContinue();
                continue;
            }

            // Формируем и отправляем пакет
            Packet requestPacket;
            requestPacket.type = (choice == 1) ? PacketType::LOGIN : PacketType::REGISTER;
            requestPacket.data["username"] = login;
            requestPacket.data["password"] = password;
            client.sendMessage(requestPacket.serialize());
            
            // Получаем ответ
            std::string responseData = client.receiveData();
            if (responseData.empty()) {
                std::cout << "Сервер не отвечает.\n";
                pressEnterToContinue();
                continue;
            }

            Packet resp = Packet::deserialize(responseData);

            // Если вход успешен, переходим в главное меню
            if (resp.type == PacketType::SUCCESS_RESPONSE) {
                if (choice == 1) { // LOGIN
                    myUserId = resp.data["user_id"];
                    std::cout << "Успех! Ваш ID: " << myUserId << "\n";
                    pressEnterToContinue();
                    currentState = AppState::MAIN_MENU; // Переходим в главное меню
                } else {
                    std::cout << "Регистрация прошла успешно! Теперь вы можете войти.\n";
                    pressEnterToContinue();
                }
            } else if (resp.type == PacketType::ERROR_RESPONSE) {
                std::cout << "Ошибка: " << resp.data["message"].get<std::string>() << "\n";
                pressEnterToContinue();
            }

        /**
         * 2. Главное меню (MAIN_MENU):
         */
        } else if (currentState == AppState::MAIN_MENU) {
            std::cout << "\n=== Главное меню ===\n\n";

            // Запрашиваем список чатов
            Packet requestPacket;
            requestPacket.type = PacketType::GET_CHATS;
            requestPacket.data["user_id"] = myUserId;
            client.sendMessage(requestPacket.serialize());

            // Получаем ответ
            std::string responseData = client.receiveData();
            if (responseData.empty()) {
                std::cout << "Сервер не отвечает.\n";
                pressEnterToContinue();
                continue;
            }

            Packet resp = Packet::deserialize(responseData);

            int optionNumber = 1;
            std::vector<int> chatIds;      // Параллельный вектор с ID чатов
            std::vector<std::string> chatNames; // Параллельный вектор с именами чатов

            if (resp.type == PacketType::CHAT_LIST_RESPONSE) {
                auto chats = resp.data["chats"];
                if (!chats.empty()) {
                    std::cout << "--- Ваши чаты ---\n";
                    for (const auto& chat : chats) {
                        int cid = chat["chat_id"];
                        std::string cname = chat["chat_name"];
                        std::cout << optionNumber << ". " << cname << "\n";
                        chatIds.push_back(cid);
                        chatNames.push_back(cname);
                        optionNumber++;
                    }
                } else {
                    std::cout << "У вас пока нет чатов.\n";
                }
            }

            std::cout << "\n" << optionNumber << ". Создать новый чат\n";
            int createChatOption = optionNumber;
            optionNumber++;
            std::cout << optionNumber << ". Выйти из аккаунта\n";
            int logoutOption = optionNumber;
            std::cout << "0. Выйти из приложения\n\n";
            std::cout << "Выберите действие: ";

            int choice = readInt();

            if (choice == 0) {
                break;
            }

            if (choice == logoutOption) {
                myUserId = -1;
                myChatId = -1;
                currentChatName.clear();
                currentState = AppState::AUTH;
                continue;
            }

            if (choice == createChatOption) {
                std::string targetUsername;
                std::cout << "Логин собеседника: ";
                std::getline(std::cin, targetUsername);

                if (targetUsername.empty()) continue;

                Packet req;
                req.type = PacketType::CREATE_CHAT;
                req.data["sender_id"] = myUserId;
                req.data["target_username"] = targetUsername;
                client.sendMessage(req.serialize());

                std::string respData = client.receiveData();
                if (respData.empty()) continue;

                Packet createResp = Packet::deserialize(respData);
                if (createResp.type == PacketType::SUCCESS_RESPONSE) {
                    myChatId = createResp.data["chat_id"];
                    currentChatName = targetUsername;
                    currentState = AppState::IN_CHAT;
                } else {
                    std::cout << "Ошибка: " << createResp.data["message"].get<std::string>() << "\n";
                    pressEnterToContinue();
                }
                continue;
            }

            // Выбор существующего чата
            int chatIndex = choice - 1;
            if (chatIndex >= 0 && chatIndex < static_cast<int>(chatIds.size())) {
                myChatId = chatIds[chatIndex];
                currentChatName = chatNames[chatIndex];
                currentState = AppState::IN_CHAT;
            }

        /**
         * 3. Экран внутри чата (IN_CHAT):
         */
        } else if (currentState == AppState::IN_CHAT) {
            // Запрашиваем историю
            Packet reqHistory;
            reqHistory.type = PacketType::GET_CHAT_HISTORY;
            reqHistory.data["chat_id"] = myChatId;
            client.sendMessage(reqHistory.serialize());

            std::string responseData = client.receiveData();
            if (responseData.empty()) {
                std::cout << "Сервер не отвечает.\n";
                pressEnterToContinue();
                currentState = AppState::MAIN_MENU;
                continue;
            }

            Packet resp = Packet::deserialize(responseData);

            std::cout << "\n=== " << currentChatName << " ===\n\n";

            if (resp.type == PacketType::HISTORY_RESPONSE) {
                auto history = resp.data["history"];
                if (!history.empty()) {
                    for (const auto& msg : history) {
                        int sender = msg["sender_id"];
                        std::string text = msg["content"];
                        std::string time = msg["timestamp"];
                        std::string author = (sender == myUserId) ? "Вы" : currentChatName;
                        std::cout << "[" << time << "] " << author << ": " << text << "\n";
                    }
                }
            }

            std::cout << "\n----------------------------------------\n";
            std::cout << "Введите сообщение ( /back — назад, /update — обновить )\n> ";

            std::string text;
            std::getline(std::cin, text);

            if (text == "/back") {
                myChatId = -1;
                currentChatName.clear();
                currentState = AppState::MAIN_MENU;
                continue;
            }

            if (text == "/update") {
                continue;  // Просто перерисуем историю
            }

            if (text.empty()) continue;

            // Отправляем сообщение
            Packet reqSend;
            reqSend.type = PacketType::SEND_MESSAGE;
            reqSend.data["sender_id"] = myUserId;
            reqSend.data["chat_id"] = myChatId;
            reqSend.data["message"] = text;
            client.sendMessage(reqSend.serialize());

            // Ждём подтверждение
            client.receiveData();
        }
    }
            
    // После обработки ответа от сервера, цикл повторяется, и пользователь может выбрать следующее действие
    client.disconnect();
    return 0;
}