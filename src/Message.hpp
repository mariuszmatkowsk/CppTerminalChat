#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
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

struct TextMessage {
    std::string from;
    std::string message;
};

enum class MessageType {
    Connect,
    Disconnect,
    TextMessage,
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

struct Message {
    MessageHeader header;
    std::vector<uint8_t> body;
};

inline std::vector<uint8_t> serialize(const Message& msg) {
    std::vector<uint8_t> buffer;
    buffer.reserve(msg.header.body_size + MessageHeaderSize);
    auto serialized_header = serialize(msg.header);
    buffer.insert(buffer.end(),
                  std::make_move_iterator(serialized_header.begin()),
                  std::make_move_iterator(serialized_header.end()));
    buffer.insert(buffer.end(), msg.body.begin(), msg.body.end());
    return buffer;
}

template <typename Msg>
Message create_message(MessageType type, const Msg& message) {
    auto serialized_message = serialize(message);

    return Message{.header = MessageHeader{.type = MessageType::Connect,
                                           .body_size = static_cast<uint32_t>(
                                               serialized_message.size())},
                   .body = std::move(serialized_message)};
}
