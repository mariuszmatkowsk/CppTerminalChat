#include "Connection.hpp"
#include "../Message.hpp"
#include "ConnectionsManager.hpp"

#include <asio.hpp>
#include <asio/completion_condition.hpp>
#include <print>

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionsManager& connections_manager)
    : socket_(std::move(socket)), connections_manager_(connections_manager),
      connection_info_(ConnectionInfo{
          .address = socket_.remote_endpoint().address().to_string(),
          .port = socket_.remote_endpoint().port()}) {
    std::println("New connection: {}", connection_info_);
}

void Connection::start() {
    do_read();
}

void Connection::stop() {
    socket_.close();
}

void Connection::do_read() {
    auto handle_read = [self = shared_from_this(), this](asio::error_code ec,
                                                         size_t bytes_read) {
        if (!ec) {
            MessageHeader header{};
            if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                            header)) {
                if (header.type == MessageType::Connect) {
                    std::println("New ConnectMessage, with body_size = {}",
                                 header.body_size);
                } else if (header.type == MessageType::Text) {
                    std::println("New TextMessage, with body_size = {}",
                                 header.body_size);
                } else if (header.type == MessageType::Disconnect) {
                    std::println("New DisconnectMessage, with body_size = {}",
                                 header.body_size);
                }

                if (header.body_size > 0) {
                    do_read_body(std::move(header));
                } else {
                    do_read();
                }
            }
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(MessageHeaderSize), handle_read);
}

void Connection::do_read_body(MessageHeader header) {
    auto handle_body_read = [&header, self = shared_from_this(),
                             this](asio::error_code ec, size_t bytes_read) {
        if (!ec) {
            if (header.type == MessageType::Connect) {
                ConnectMessage connect_message;
                if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                                connect_message)) {
                    std::println("{} want to connect.", connect_message.nick);
                } else {
                    std::println("Something goes wrong during deserialization "
                                 "of ConnectMessage");
                    return;
                }
            } else if (header.type == MessageType::Text) {
                TextMessage text_message;
                if (deserialize({buffer_.begin(), buffer_.begin() + bytes_read},
                                text_message)) {
                    std::println("New TextMessage from: {} with content: {}",
                                 text_message.from, text_message.message);
                }
            }
            do_read();
        } else {
            std::println("Something goes wrong during read message body");
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(header.body_size),
                     handle_body_read);
}

void Connection::do_write() {
}
