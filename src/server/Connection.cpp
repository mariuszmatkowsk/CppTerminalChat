#include "Connection.hpp"
#include "../Message.hpp"
#include "ConnectionsManager.hpp"

#include <asio.hpp>
#include <asio/completion_condition.hpp>
#include <asio/error_code.hpp>
#include <asio/steady_timer.hpp>
#include <netinet/tcp.h>
#include <print>

using namespace std::chrono_literals;

namespace logger {
void error(const std::string& msg) {
    std::println("Error: {}", msg);
}

void info(const std::string& msg) {
    std::println("Info: {}", msg);
}
} // namespace logger

Connection::Connection(asio::io_context& io_context,
                       asio::ip::tcp::socket socket,
                       ConnectionsManager& connections_manager)
    : io_context_(io_context), socket_(std::move(socket)),
      connections_manager_(connections_manager), timer_(io_context_),
      connection_info_(ConnectionInfo{
          .address = socket_.remote_endpoint().address().to_string(),
          .port = socket_.remote_endpoint().port()}) {
    logger::info(std::format("new connection: {}", connection_info_));
    setup_dispatcher();
}

void Connection::start() {
    do_read_header();
}

void Connection::stop() {
    socket_.close();
}

void Connection::do_read_header() {
    auto handle_read_header = [self = shared_from_this(),
                               this](asio::error_code ec, size_t bytes_read) {
        if (!ec) {
            MessageHeader header{};
            if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                            header)) {

                header.body_size > 0 ? do_read_body(std::move(header))
                                     : do_read_header();
            } else {
                logger::error("could not deserialize MessageHeader");
            }
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(MessageHeaderSize),
                     handle_read_header);
}

void Connection::do_read_body(MessageHeader header) {
    auto handle_body_read = [header, self = shared_from_this(),
                             this](asio::error_code ec, size_t bytes_read) {
        if (!ec) {
            const auto handler = dispatcher_.find(header.type);
            if (handler != std::end(dispatcher_)) {
                handler->second(header, bytes_read);
            } else {
                logger::error("could not find handler for message");
            }
            do_read_header();
        } else {
            logger::error("could not read whole message body");
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(header.body_size),
                     handle_body_read);
}

void Connection::broadcast_message(Message msg) {
    const auto message_data =
        std::make_shared<SerializedMessage>(serialize(msg));
    for (auto& [connection, nick] : connections_manager_.get_connections()) {
        if (connection == shared_from_this() || nick.empty()) {
            continue;
        }

        connection->get_socket().async_send(
            asio::buffer(*message_data),
            [connection, message_data](asio::error_code ec, size_t bytes) {
                // TODO: handle failure. Maybe add some logger ????
            });
    }
}

void Connection::setup_dispatcher() {
    dispatcher_.insert(
        {MessageType::Connect,
        [this](MessageHeader header, size_t bytes_read) {
             handle_connect_message(header, bytes_read);
         }});
    dispatcher_.insert(
        {MessageType::Disconnect,
        [this](MessageHeader header, size_t bytes_read) {
            handle_disconnect_message(header, bytes_read);
        }});
    dispatcher_.insert(
        {MessageType::Text,
        [this](MessageHeader header, size_t bytes_read) {
            handle_text_message(header, bytes_read);
        }});
    dispatcher_.insert(
        {MessageType::PrivateMessage,
        [this](MessageHeader header, size_t bytes_read) {
            handle_private_message(header, bytes_read);
        }});
}

void Connection::handle_connect_message(MessageHeader header,
                                        size_t bytes_read) {
    ConnectMessage connect_message;
    if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                    connect_message)) {

        connections_manager_.set_nick(shared_from_this(), connect_message.nick);
        broadcast_message(connect_message);

        for (const auto& [connection, nick] :
             connections_manager_.get_connections()) {
            if (connection == shared_from_this() || nick.empty())
                continue;

            auto nick_to_send = std::make_shared<SerializedMessage>(
                serialize(Message{ConnectMessage{.nick = nick}}));
            socket_.async_send(asio::buffer(*nick_to_send),
                               [self = shared_from_this(), nick_to_send](
                                   asio::error_code ec, size_t bytes) {
                                   // TODO: handle error
                               });
        }
    } else {
        logger::error("could not deserialize ConnectMessage");
    }
}

void Connection::handle_disconnect_message(MessageHeader header,
                                           size_t bytes_read) {
    DisconnectMessage disconnect_message;
    if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                    disconnect_message)) {
        std::println("New DisconnectMessage from: {}", disconnect_message.nick);
        broadcast_message(disconnect_message);
    } else {
        logger::error("could not deserialize DisconnectMessage");
    }
}

void Connection::handle_text_message(MessageHeader header, size_t bytes_read) {
    TextMessage text_message;
    if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                    text_message)) {
        // TODO: unset nick
        broadcast_message(text_message);
    } else {
        logger::error("could not deserialize TextMessage");
    }
}

void Connection::handle_private_message(MessageHeader header,
                                        size_t bytes_read) {
    PrivateMessage private_message;
    if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                    private_message)) {

        auto connection =
            connections_manager_.get_connection_by_nick(private_message.to);
        if (connection) {
            auto serialized_message = std::make_shared<SerializedMessage>(
                serialize(Message{private_message}));
            connection->get()->get_socket().async_send(
                asio::buffer(*serialized_message),
                [serialized_message, connection = connection->get()](
                    asio::error_code ec, size_t bytes) {
                    // TODO: handle error send
                });
        } else {
            logger::error(std::format("client {} trying send message to not "
                                      "connected client {}",
                                      private_message.from,
                                      private_message.to));
        }
    } else {
        logger::error("could not deserialize PrivateMessage");
    }
}
