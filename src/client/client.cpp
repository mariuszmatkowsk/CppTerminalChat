#include "../Message.hpp"
#include "Connection.hpp"

#include <asio.hpp>
#include <print>

using namespace std::chrono_literals;

int main(int argc, char** argv) {
    std::println("Hello from client");

    asio::io_context io_context{};
    asio::ip::tcp::resolver resolver{io_context};
    asio::ip::tcp::resolver::results_type endpoints{
        resolver.resolve("127.0.0.1", "9999")};

    Connection connection{io_context, std::move(endpoints)};

    std::thread t{[&] { io_context.run(); }};

    std::this_thread::sleep_for(5s);

    ConnectMessage connect_msg{.nick = "Mariusz"};

    Message msg_to_send =
        create_message(MessageType::Connect, ConnectMessage{.nick = "Mariusz"});

    connection.send(msg_to_send);

    t.join();

    return 0;
}
