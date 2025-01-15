#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

struct ConnectMessage {
    std::string nick;
};

inline std::vector<uint8_t> serialize(const ConnectMessage& msg) {
    unsigned long nick_length = msg.nick.length();
    constexpr size_t nick_length_size = sizeof(nick_length);

    std::vector<uint8_t> buffer(nick_length + nick_length_size);

    std::memcpy(buffer.data(), &nick_length, nick_length_size);
    std::memcpy(buffer.data() + nick_length_size, msg.nick.data(), nick_length);

    return buffer;
}

inline bool deserialize(const std::vector<uint8_t>& buffer,
                        ConnectMessage& msg) {
    unsigned long nick_length{0};
    constexpr size_t nick_length_size = sizeof(nick_length);
    std::memcpy(&nick_length, buffer.data(), nick_length_size);

    if (buffer.size() != nick_length_size + nick_length) {
        return false;
    }

    msg.nick.assign(buffer.begin() + nick_length_size, buffer.end());
    return true;
}

struct DisconnectMessage {};

struct TextMessage {
    std::string from;
    std::string message;
};

inline std::vector<uint8_t> serialize(const TextMessage& msg) {
    unsigned long from_length = msg.from.length();
    unsigned long message_length = msg.message.length();

    constexpr size_t from_length_size = sizeof(from_length);
    constexpr size_t message_length_size = sizeof(message_length);

    std::vector<uint8_t> buffer(from_length + message_length + from_length_size + message_length_size);

    unsigned offset{0};
    std::memcpy(buffer.data() + offset, &from_length, from_length_size);
    offset += from_length_size;
    std::memcpy(buffer.data() + offset, msg.from.data(), from_length);
    offset += from_length;
    std::memcpy(buffer.data() + offset, &message_length, message_length_size);
    offset += message_length_size;
    std::memcpy(buffer.data() + offset, msg.message.data(), message_length);

    return buffer;
}

inline bool deserialize(const std::vector<uint8_t>& buffer, TextMessage& msg) {
    unsigned long from_length{0};
    unsigned long message_length{0};

    constexpr size_t from_length_size = sizeof(from_length);
    constexpr size_t message_length_size = sizeof(message_length);

    unsigned offset{0};
    std::memcpy(&from_length, buffer.data() + offset, from_length_size);
    offset += from_length_size;
    msg.from.assign(buffer.begin() + offset, buffer.begin() + offset + from_length);
    offset += from_length;
    std::memcpy(&message_length, buffer.data() + offset, message_length_size);
    offset += message_length_size;
    msg.message.assign(buffer.begin() + offset, buffer.end());

    if (buffer.size() != from_length_size + from_length + message_length_size + message_length) {
        return false;
    }

    return true;
}

enum class MessageType {
    Connect,
    Disconnect,
    Text,
    PrivateMessage,
};

struct MessageHeader {
    MessageType type;
    uint32_t body_size;
};

constexpr size_t MessageHeaderSize = sizeof(MessageHeader);

inline std::vector<uint8_t> serialize(const MessageHeader& header) {
    std::vector<uint8_t> buffer(MessageHeaderSize);
    std::memcpy(buffer.data(), &header, MessageHeaderSize);
    return buffer;
}

inline bool deserialize(const std::vector<uint8_t>& buffer,
                        MessageHeader& header) {
    if (buffer.size() != MessageHeaderSize) {
        return false;
    }

    std::memcpy(&header, buffer.data(), buffer.size());

    return true;
}

using Message = std::variant<ConnectMessage, TextMessage, DisconnectMessage>;

template <typename... Ts>
struct overloads : Ts... {
    using Ts::operator()...;
};

inline std::vector<uint8_t> serialize(const Message& msg) {
    MessageHeader header{};
    std::vector<uint8_t> serialized_message;

    auto visitor = overloads{
        [&](const ConnectMessage& connect_message) {
            serialized_message = serialize(connect_message);
            header = {.type = MessageType::Connect,
                      .body_size =
                          static_cast<uint32_t>(serialized_message.size())};
        },
        [&](const TextMessage& text_message) {
            serialized_message = serialize(text_message);
            header = {.type = MessageType::Text,
                      .body_size =
                          static_cast<uint32_t>(serialized_message.size())};
        },
        [&](const DisconnectMessage& disconnect_message) {
            header = {.type = MessageType::Disconnect, .body_size = 0};
        }
    };

    std::visit(visitor, msg);

    std::vector<uint8_t> buffer;
    buffer.reserve(header.body_size + MessageHeaderSize);

    auto serialized_header = serialize(header);
    buffer.insert(buffer.end(),
                  std::make_move_iterator(serialized_header.begin()),
                  std::make_move_iterator(serialized_header.end()));

    if (!serialized_message.empty()) {
        buffer.insert(buffer.end(),
                      std::make_move_iterator(serialized_message.begin()),
                      std::make_move_iterator(serialized_message.end()));
    }

    return buffer;
}

