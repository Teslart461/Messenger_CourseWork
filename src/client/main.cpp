#include "common/Logger.h"
#include "client/Client.h"
#include <string>
#include <iostream>

int main() {
    Client client;

    // Подключаемся
    if (!client.connectToServer("127.0.0.1", 8080)) {
        return 1;
    }

    // Читаем приветственное сообщение от сервера
    std::string welcome = client.receiveData();
    std::cout << "\nСообщение от сервера: \n" << welcome << std::endl;

    // Отправляем сообщение на сервер
    client.sendMessage("Hello, Server!");
    
    client.disconnect();

    return 0;
}