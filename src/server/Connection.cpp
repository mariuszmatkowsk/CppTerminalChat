#include "Connection.hpp"
#include "../Message.hpp"
#include "ConnectionsManager.hpp"

#include <asio.hpp>
#include <print>
#include <ranges>

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
    logger::info(std::format("New client connected: {}", connection_info_));
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
        if (!ec && bytes_read == MessageHeaderSize) {
            MessageHeader header{};
            if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                            header)) {

                switch (header.type) {
                    case MessageType::Text:
                    case MessageType::Connect:
                    case MessageType::Disconnect:
                    case MessageType::PrivateMessage: {
                        do_read_body(std::move(header));
                        break;
                    }
                    case MessageType::PingServer: {
                        do_read_header();
                        break;
                    }
                    case MessageType::ChatUsers: {
                        logger::error("Not supporter message type: ChatUsers");
                        do_read_header();
                        break;
                    }
                }
            } else {
                logger::error("Could not deserialize MessageHeader");
            }
        } else {
            if (ec == asio::error::eof) {
                logger::info(
                    std::format("Client: {} disconnected.", connection_info_));
                auto nick = connections_manager_.unset_nick(shared_from_this());
                if (nick) {
                    broadcast_message(
                        DisconnectMessage{.nick = std::move(*nick)});
                }
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
            if (bytes_read == header.body_size) {
                const auto handler = dispatcher_.find(header.type);
                if (handler != std::end(dispatcher_)) {
                    handler->second(header, bytes_read);
                } else {
                    logger::error("Could not find handler for message");
                }
                do_read_header();
            } else {
                logger::error("Could not read whole message body");
            }
        } else {
            if (ec == asio::error::eof) {
                logger::info(
                    std::format("Client: {} disconnected.", connection_info_));
                auto nick = connections_manager_.unset_nick(shared_from_this());
                if (nick) {
                    broadcast_message(
                        DisconnectMessage{.nick = std::move(*nick)});
                }
            }
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(header.body_size),
                     handle_body_read);
}

void Connection::do_send_chat_users() {
    timer_.expires_after(1s);
    timer_.async_wait([self = shared_from_this(), this](asio::error_code ec) {
        if (!ec) {
            std::vector<std::string> users{};
            if (connections_manager_.get_nick(self)) {
                const auto& connections =
                    connections_manager_.get_connections();

                users = std::ranges::to<std::vector>(
                    connections | std::views::values |
                    std::views::filter(
                        [](const auto& u) { return !u.empty(); }));
            }

            auto serialized_msg =
                serialize(Message{ChatUsersMessage{.users = std::move(users)}});
            auto msg_to_send =
                std::make_shared<SerializedMessage>(serialized_msg);

            socket_.async_send(
                asio::buffer(*msg_to_send),
                [self, this, msg_to_send](asio::error_code ec, size_t bytes) {
                    if (!ec) {
                        do_send_chat_users();
                    }
                });
        }
    });
}

void Connection::broadcast_message(Message msg) {
    const auto message_data =
        std::make_shared<SerializedMessage>(serialize(msg));
    auto self = shared_from_this();
    for (auto& [connection, nick] : connections_manager_.get_connections()) {
        if (connection == self || nick.empty()) {
            continue;
        }

        connection->get_socket().async_send(
            asio::buffer(*message_data),
            [self, connection, message_data](asio::error_code ec,
                                             size_t bytes_send) {});
    }
}

void Connection::setup_dispatcher() {
    dispatcher_.insert(
        {MessageType::Connect, [this](MessageHeader header, size_t bytes_read) {
             logger::info("New connect message");
             handle_connect_message(header, bytes_read);
         }});
    dispatcher_.insert({MessageType::Disconnect,
                        [this](MessageHeader header, size_t bytes_read) {
                            handle_disconnect_message(header, bytes_read);
                        }});
    dispatcher_.insert(
        {MessageType::Text, [this](MessageHeader header, size_t bytes_read) {
             handle_text_message(header, bytes_read);
         }});
    dispatcher_.insert({MessageType::PrivateMessage,
                        [this](MessageHeader header, size_t bytes_read) {
                            handle_private_message(header, bytes_read);
                        }});
}

void Connection::handle_connect_message(MessageHeader header,
                                        size_t bytes_read) {
    if (bytes_read == header.body_size) {
        ConnectMessage connect_message;
        if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                        connect_message)) {

            connections_manager_.set_nick(shared_from_this(),
                                          connect_message.nick);
            logger::info(
                std::format("{} joined the chat.", connect_message.nick));
            do_send_chat_users();
        } else {
            logger::error("Could not deserialize ConnectMessage");
        }
    } else {
        logger::error(
            std::format("Not all ConnectMessage body was read from client: {}",
                        connection_info_));
    }
}

void Connection::handle_disconnect_message(MessageHeader header,
                                           size_t bytes_read) {
    if (bytes_read == header.body_size) {
        DisconnectMessage disconnect_message;
        if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                        disconnect_message)) {
            logger::info(
                std::format("{} left the chat.", disconnect_message.nick));
            auto _ = connections_manager_.unset_nick(shared_from_this());
        } else {
            logger::error("Could not deserialize DisconnectMessage");
        }
    } else {
        logger::error(std::format(
            "Not all DisconnectMessage body was read from client: {}",
            connection_info_));
    }
}

void Connection::handle_text_message(MessageHeader header, size_t bytes_read) {
    if (bytes_read == header.body_size) {
        TextMessage text_message;
        if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                        text_message)) {
            broadcast_message(text_message);
        } else {
            logger::error("Could not deserialize TextMessage");
        }
    } else {
        logger::error(
            std::format("Not all TextMessage body was read from client: {}",
                        connection_info_));
    }
}

void Connection::handle_private_message(MessageHeader header,
                                        size_t bytes_read) {
    if (bytes_read == header.body_size) {
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
                        asio::error_code, size_t) {});
            } else {
                logger::error(
                    std::format("Client {} trying send message to not "
                                "connected client {}",
                                private_message.from, private_message.to));
            }
        } else {
            logger::error("Could not deserialize PrivateMessage");
        }
    } else {
        logger::error(
            std::format("Not all PrivateMessage body was read from client: {}",
                        connection_info_));
    }
}
