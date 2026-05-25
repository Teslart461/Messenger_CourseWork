#include "common/Logger.h"
#include "client/Client.h"
#include "common/Packet.h"

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

    // Имитируем отправку JSON пакета на сервер
    std::cout << "Формируем JSON пакет для отправки..." << std::endl;

    // Создаем пакет с данными для отправки на сервер
    Packet packet;
    packet.type = PacketType::SEND_MESSAGE;
    packet.data["text"] = "Привет, сервер! Это клиент! Hello)";
    packet.data["sender_id"] = 17; // Пример ID отправителя
    packet.data["chat_id"] = 67; // Пример ID чата

    // Сериализуем пакет в строку JSON для отправки
    std::string serializedPacket = packet.serialize();
    std::cout << "Отправляем пакет на сервер: " << serializedPacket << std::endl;

    client.sendMessage(serializedPacket);

    // Получаем ответ от сервера
    std::string response = client.receiveData();
    Packet responsePacket;
    try {
        responsePacket = Packet::deserialize(response);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при десериализации ответа от сервера: " << e.what() << std::endl;
        return 1;
    } 
    std::cout << "\nОтвет от сервера: \n" << responsePacket.data["info"] << std::endl;


    client.disconnect();

    return 0;
}