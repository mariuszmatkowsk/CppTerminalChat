#include "Message.hpp"

#include <vector>

SerializedMessage serialize(const MessageHeader& header) {
    SerializedMessage buffer(MessageHeaderSize);
    std::memcpy(buffer.data(), &header, MessageHeaderSize);
    return buffer;
}

bool deserialize(const SerializedMessage& buffer, MessageHeader& header) {
    if (buffer.size() == MessageHeaderSize) {
        std::memcpy(&header, buffer.data(), buffer.size());
        return true;
    }
    return false;
}

SerializedMessage serialize(const ConnectMessage& msg) {
    unsigned long nick_length = msg.nick.length();
    constexpr size_t nick_length_size = sizeof(nick_length);

    SerializedMessage buffer(nick_length + nick_length_size);

    std::memcpy(buffer.data(), &nick_length, nick_length_size);
    std::memcpy(buffer.data() + nick_length_size, msg.nick.data(), nick_length);

    return buffer;
}

bool deserialize(const SerializedMessage& buffer, ConnectMessage& msg) {
    unsigned long nick_length{0};
    constexpr size_t nick_length_size = sizeof(nick_length);
    std::memcpy(&nick_length, buffer.data(), nick_length_size);

    if (buffer.size() != nick_length_size + nick_length) {
        return false;
    }

    msg.nick.assign(buffer.begin() + nick_length_size, buffer.end());
    return true;
}

SerializedMessage serialize(const DisconnectMessage& msg) {
    unsigned long nick_length = msg.nick.length();
    constexpr size_t nick_length_size = sizeof(nick_length);

    SerializedMessage buffer(nick_length + nick_length_size);

    std::memcpy(buffer.data(), &nick_length, nick_length_size);
    std::memcpy(buffer.data() + nick_length_size, msg.nick.data(), nick_length);

    return buffer;
}

bool deserialize(const SerializedMessage& buffer, DisconnectMessage& msg) {
    unsigned long nick_length{0};
    constexpr size_t nick_length_size = sizeof(nick_length);
    std::memcpy(&nick_length, buffer.data(), nick_length_size);

    if (buffer.size() != nick_length_size + nick_length) {
        return false;
    }

    msg.nick.assign(buffer.begin() + nick_length_size, buffer.end());
    return true;
}

SerializedMessage serialize(const TextMessage& msg) {
    unsigned long from_length = msg.from.length();
    unsigned long message_length = msg.message.length();

    constexpr size_t from_length_size = sizeof(from_length);
    constexpr size_t message_length_size = sizeof(message_length);

    SerializedMessage buffer(from_length + message_length +
                                from_length_size + message_length_size);

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

bool deserialize(const SerializedMessage& buffer, TextMessage& msg) {
    unsigned long from_length{0};
    unsigned long message_length{0};

    constexpr size_t from_length_size = sizeof(from_length);
    constexpr size_t message_length_size = sizeof(message_length);

    unsigned offset{0};
    std::memcpy(&from_length, buffer.data() + offset, from_length_size);
    offset += from_length_size;
    msg.from.assign(buffer.begin() + offset,
                    buffer.begin() + offset + from_length);
    offset += from_length;
    std::memcpy(&message_length, buffer.data() + offset, message_length_size);
    offset += message_length_size;
    msg.message.assign(buffer.begin() + offset, buffer.end());

    if (buffer.size() !=
        from_length_size + from_length + message_length_size + message_length) {
        return false;
    }

    return true;
}

SerializedMessage serialize(const PrivateMessage& msg) {
    unsigned long from_length = msg.from.length();
    unsigned long to_length = msg.to.length();
    unsigned long message_length = msg.message.length();

    constexpr size_t from_length_size = sizeof(from_length);
    constexpr size_t to_length_size = sizeof(to_length);
    constexpr size_t message_length_size = sizeof(message_length);

    SerializedMessage buffer(from_length + message_length + to_length +
                                to_length_size + from_length_size +
                                message_length_size);

    unsigned offset{0};
    std::memcpy(buffer.data() + offset, &from_length, from_length_size);
    offset += from_length_size;
    std::memcpy(buffer.data() + offset, msg.from.data(), from_length);
    offset += from_length;
    std::memcpy(buffer.data() + offset, &to_length, to_length_size);
    offset += to_length_size;
    std::memcpy(buffer.data() + offset, msg.to.data(), to_length);
    offset += to_length;
    std::memcpy(buffer.data() + offset, &message_length, message_length_size);
    offset += message_length_size;
    std::memcpy(buffer.data() + offset, msg.message.data(), message_length);

    return buffer;
}

bool deserialize(const SerializedMessage& buffer, PrivateMessage& msg) {
    unsigned long from_length = msg.from.length();
    unsigned long to_length = msg.to.length();
    unsigned long message_length = msg.message.length();

    constexpr size_t from_length_size = sizeof(from_length);
    constexpr size_t to_length_size = sizeof(to_length);
    constexpr size_t message_length_size = sizeof(message_length);

    unsigned offset{0};
    std::memcpy(&from_length, buffer.data() + offset, from_length_size);
    offset += from_length_size;
    msg.from.assign(buffer.begin() + offset,
                    buffer.begin() + offset + from_length);
    offset += from_length;
    std::memcpy(&to_length, buffer.data() + offset, to_length_size);
    offset += to_length_size;
    msg.to.assign(buffer.begin() + offset, buffer.begin() + offset + to_length);
    offset += to_length;
    std::memcpy(&message_length, buffer.data() + offset, message_length_size);
    offset += message_length_size;
    msg.message.assign(buffer.begin() + offset, buffer.end());

    if (buffer.size() != from_length_size + from_length + to_length_size +
                             to_length + message_length_size + message_length) {
        return false;
    }

    return true;
}

SerializedMessage serialize(const Message& msg) {
    MessageHeader header;
    SerializedMessage serialized_message;

    auto visitor = [&]<typename MsgType>(const MsgType& msg) {
        serialized_message = serialize(msg);

        MessageType type;
        if constexpr (std::is_same_v<MsgType, ConnectMessage>) {
            type = MessageType::Connect;
        } else if constexpr (std::is_same_v<MsgType, DisconnectMessage>) {
            type = MessageType::Disconnect;
        } else if constexpr (std::is_same_v<MsgType, TextMessage>) {
            type = MessageType::Text;
        } else if constexpr (std::is_same_v<MsgType, PrivateMessage>) {
            type = MessageType::PrivateMessage;
        }

        header = {.type = std::move(type),
                  .body_size =
                      static_cast<uint32_t>(serialized_message.size())};
    };

    std::visit(visitor, msg);

    SerializedMessage buffer;
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
