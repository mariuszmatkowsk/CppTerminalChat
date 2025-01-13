add_executable(
    server
    server.cpp
    ChatServer.cpp
    Connection.cpp
)

target_link_libraries(
    server
    PRIVATE asio
)
