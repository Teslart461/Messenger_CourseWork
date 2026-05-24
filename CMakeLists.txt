cmake_minimum_required(VERSION 3.10)
project(TeslaMAX++)

# Стандарт C++17 для современных фич
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути к заголовочным файлам
include_directories(include)

# Для работы с потоками
find_package(Threads REQUIRED)

# Поиск библиотеки SQLite3
find_package(SQLite3 REQUIRED)

# Серверная часть
add_executable(server
    src/server/main.cpp
    src/common/Logger.cpp
    src/server/DataBaseManager.cpp
)
target_link_libraries(server Threads::Threads)

# Клиентская часть
add_executable(client
    src/client/main.cpp
    src/common/Logger.cpp
)
target_link_libraries(client Threads::Threads)


target_link_libraries(server SQLite::SQLite3)