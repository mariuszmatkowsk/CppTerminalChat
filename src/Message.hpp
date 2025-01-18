#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <variant>
#include <vector>

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

using SerializedMessage = std::vector<uint8_t>;

SerializedMessage serialize(const MessageHeader& header);
bool deserialize(const SerializedMessage& buffer, MessageHeader& header);

constexpr size_t MessageHeaderSize = sizeof(MessageHeader);

struct ConnectMessage {
    std::string nick;
};

SerializedMessage serialize(const ConnectMessage& msg);
bool deserialize(const SerializedMessage& buffer, ConnectMessage& msg);

struct DisconnectMessage {
    std::string nick;
};

SerializedMessage serialize(const DisconnectMessage& msg);
bool deserialize(const SerializedMessage& buffer, DisconnectMessage& msg);

struct TextMessage {
    std::string from;
    std::string message;
};

SerializedMessage serialize(const TextMessage& msg);
bool deserialize(const SerializedMessage& buffer, TextMessage& msg);

struct PrivateMessage {
    std::string from;
    std::string to;
    std::string message;
};

SerializedMessage serialize(const PrivateMessage& msg);
bool deserialize(const SerializedMessage& buffer, PrivateMessage& msg);

using Message = std::variant<ConnectMessage, TextMessage, DisconnectMessage, PrivateMessage>;

SerializedMessage serialize(const Message& msg);
