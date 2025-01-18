add_executable(
    server
    server.cpp
    ChatServer.cpp
    Connection.cpp
    ../Message.cpp
)

target_link_libraries(
    server
    PRIVATE asio
)
