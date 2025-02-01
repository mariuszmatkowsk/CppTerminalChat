#include "../Message.hpp"
#include "Connection.hpp"

#include <asio.hpp>
#include <asio/error_code.hpp>
#include <asio/executor_work_guard.hpp>
#include <chrono>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <print>
#include <variant>
#include <tuple>

using namespace std::chrono_literals;

namespace command {
    constexpr const char* Join{"join"};
    constexpr const char* Leave{"leave"};
    constexpr const char* PrivateMsg{"private"};
    constexpr const char* Help{"help"};
} // namespace command

struct ChatMessage {
    std::string nick;
    std::string message;
};

using ChatMessages = std::vector<ChatMessage>;
using ChatUsers = std::vector<std::string>;

std::tuple<std::string, std::string> parse_command(const std::string& input) {
    std::string command{};

    auto first_space_index = input.find_first_of(' ');
    if (first_space_index == std::string::npos) {
        return {input.substr(1, input.length() - 1), ""};
    }
    command = input.substr(1, first_space_index - 1);

    return {command, input.substr(first_space_index + 1, input.size() - first_space_index)};
}

enum class ChatViewState {
    Disconnected,
    Messages,
    NotSupportedCommand,
    MissingCommandArgument,
    Help,
    WrongCommandUsageAlreadyDisconnected,
    WrongCommandUsageAlreadyConnected,
    TryingJoinToOfflineServer,
};

void process_input(
        Connection& connection,
        ChatMessages& chat_messages,
        ChatUsers& chat_users,
        std::string input_text,
        ChatViewState& chat_view_state) {
    if (input_text.starts_with('/')) {
        auto [command, rest] = parse_command(input_text);
        if (command == command::Join) {
            if (connection.is_server_online()) {
                if (!connection.is_connected()) {
                    const auto& nick = rest;
                    chat_users.push_back(nick);
                    connection.join(nick);
                    chat_view_state = ChatViewState::Messages;
                } else {
                    chat_view_state = ChatViewState::WrongCommandUsageAlreadyConnected;
                    return;
                }
            } else {
                chat_view_state = ChatViewState::TryingJoinToOfflineServer;
            }
        } else if (command == command::Leave) {
            if (connection.is_connected()) {
                connection.leave();
                chat_users.clear();
                chat_view_state = ChatViewState::Disconnected;
            } else {
                chat_view_state = ChatViewState::WrongCommandUsageAlreadyDisconnected;
                return;
            }
        } else if (command == command::Help) {
            chat_view_state = ChatViewState::Help;
        } else if (command == command::PrivateMsg && connection.is_connected()) {
            auto first_space_index = rest.find_first_of(' ');
            if (first_space_index == std::string::npos) {
                chat_view_state = ChatViewState::MissingCommandArgument;
                return;
            }
            auto to = rest.substr(0, first_space_index);
            connection.send(PrivateMessage{
                .from = connection.get_nick(), .to = std::move(to),
                .message =
                    rest.substr(first_space_index + 1, rest.size() - first_space_index)
            });
            chat_view_state = ChatViewState::Messages;
        } else {
            chat_view_state = ChatViewState::NotSupportedCommand;
        }
    } else {
        if (connection.is_connected()) {
            chat_view_state = ChatViewState::Messages;
            chat_messages.emplace_back(connection.get_nick(), input_text);
            connection.send(
                TextMessage{
                    .from = connection.get_nick(),
                    .message = std::move(input_text)
            });
        }
    }
}

int main(int, char**) {
    asio::io_context io_context{};
    asio::ip::tcp::resolver resolver{io_context};
    asio::ip::tcp::resolver::results_type endpoints{
        resolver.resolve("127.0.0.1", "9999")};

    std::queue<Message> received_messages;

    Connection connection{io_context, std::move(endpoints), received_messages};
    auto work_guard = asio::make_work_guard(io_context);

    asio::steady_timer pool_messages_timer(io_context);

    std::thread t{[&] { io_context.run(); }};

    // ---------------------- ftxui -------------------
    ChatUsers chat_users;
    ChatMessages chat_messages;
    std::string input_text;
    ChatViewState chat_view_state{ChatViewState::Disconnected};
    auto input_message =
        ftxui::Input(&input_text, "Type a message...") | ftxui::CatchEvent([&](ftxui::Event event) {
            if (event == ftxui::Event::Return && !input_text.empty()) {
                process_input(
                    connection,
                    chat_messages,
                    chat_users,
                    std::move(input_text),
                    chat_view_state);
                input_text.clear();
                return true;
            }

            if (event == ftxui::Event::Escape) {
                chat_view_state = ChatViewState::Messages;
                return true;
            }

            return false;
        });
    
    auto chat = ftxui::Renderer([&] {
        if (chat_view_state == ChatViewState::Messages) {
            ftxui::Elements elements;
            std::ranges::transform(chat_messages, std::back_inserter(elements), [&] (const auto& chat_message) {
                const auto& [message_nick, message] = chat_message;
                if (message_nick == connection.get_nick()) {
                    return ftxui::text("You: " + message) | ftxui::border | ftxui::align_right | ftxui::color(ftxui::Color::Green);
                }
                return ftxui::text(std::format("{}: {}", message_nick, message)) | ftxui::border;
            });
            return ftxui::window(ftxui::text("Chat messages:") | ftxui::bold | ftxui::center,
                    ftxui::vbox(std::move(elements))
            );
        }

        ftxui::Element element;
        std::string window_title{"Help"};
        switch (chat_view_state) {
            case ChatViewState::Help:
            case ChatViewState::Disconnected: {
                element = connection.is_connected() ?
                    ftxui::text("You are now connected!!!") | ftxui::color(ftxui::Color::Green) :
                    ftxui::text("You are not connected to the server!!!") | ftxui::color(ftxui::Color::Red);
                break;
            }
            case ChatViewState::WrongCommandUsageAlreadyConnected: {
                element = ftxui::text("You are already connected. Wrong command usage!!!") | ftxui::color(ftxui::Color::Red);
                window_title = "Error:";
                break;
            }
            case ChatViewState::WrongCommandUsageAlreadyDisconnected: {
                element = ftxui::text("You are already disconnected. Wrong command usage!!!") | ftxui::color(ftxui::Color::Red);
                window_title = "Error:";
                break;
            }
            case ChatViewState::NotSupportedCommand: {
                element = ftxui::text("Command not supported.") | ftxui::color(ftxui::Color::Red);
                window_title = "Error:";
                break;
            }
            case ChatViewState::TryingJoinToOfflineServer: {
                element = ftxui::text("Could not join to the chat. Server is offline.") | ftxui::color(ftxui::Color::Red);
                window_title = "Error:";
                break;
            }
            case ChatViewState::MissingCommandArgument: {
                element = ftxui::text("Missing command argument.") | ftxui::color(ftxui::Color::Red);
                window_title = "Error:";
                break;
            }
            case ChatViewState::Messages: {
                break;
            }
        }

        return ftxui::window(ftxui::text(std::move(window_title)) | ftxui::bold | ftxui::center,
            ftxui::vbox(
                std::move(element),
                ftxui::emptyElement(),
                ftxui::text("Usage:") | ftxui::bold,
                ftxui::text("       /join <nick>                - join the chat with nick"),
                ftxui::text("       /leave                      - leave the chat"),
                ftxui::text("       /private <nick> <message>   - send private message to other connected user"),
                ftxui::text("       /help                       - show help")
        ));
    });

    auto server_status = ftxui::Renderer([&] {
        auto status = connection.is_server_online()
            ? ftxui::text("Online") | ftxui::color(ftxui::Color::Green)
            : ftxui::text("Offline") | ftxui::color(ftxui::Color::Red);

        return ftxui::window(ftxui::text("Server status:") | ftxui::bold | ftxui::center,
            status | ftxui::center);
    });

    auto users = ftxui::Renderer([&] {
        ftxui::Elements elements;
        std::ranges::transform(chat_users, std::back_inserter(elements), [](const auto& user) {
            return ftxui::text(user) | ftxui::color(ftxui::Color::SeaGreen2); });

        return ftxui::window(ftxui::text("Chat users:") | ftxui::bold | ftxui::center,
            ftxui::vbox(std::move(elements))
        );
    });

    auto left_panel = ftxui::Renderer([&] {
            return ftxui::vbox(server_status->Render(), users->Render() | ftxui::flex);
    });

    auto send_button = ftxui::Button("Send", [&] {
        if (!input_text.empty()) {
            process_input(
                connection,
                chat_messages,
                chat_users,
                std::move(input_text),
                chat_view_state);
            input_text.clear();
        }
    });

    auto input_container = ftxui::Container::Horizontal({
        input_message,
        send_button,
    });

    auto input_renderer = ftxui::Renderer(input_container, [&] {
        return ftxui::hbox({
            input_message->Render() | ftxui::flex,
            send_button->Render(),
        });
    });

    int users_panel_size{30};
    int input_panel_size{3};

    auto container = ftxui::ResizableSplitLeft(left_panel, chat, &users_panel_size);
    container = ftxui::ResizableSplitBottom(input_renderer, container, &input_panel_size);

    auto main_container = ftxui::Container::Vertical({
        input_message,
        send_button,
        container,
    });

    auto renderer = ftxui::Renderer(main_container, [&] { return container->Render(); });

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    std::function<void(asio::error_code)> handle_pool_messages = [&] (asio::error_code ec) {
        if (!ec) {
            while (connection.is_connected() && !received_messages.empty()) {
                Message message = std::move(received_messages.front());
                received_messages.pop();

                auto visitor = [&] <typename MsgType> (const MsgType& msg) {
                    if constexpr (std::is_same_v<MsgType, ConnectMessage>) {
                        chat_users.push_back(msg.nick);
                    } else if constexpr (std::is_same_v<MsgType, TextMessage>) {
                        chat_messages.push_back({.nick = msg.from, .message = msg.message});
                    } else if constexpr (std::is_same_v<MsgType, PrivateMessage>) {
                        chat_messages.push_back({.nick = msg.from, .message = msg.message});
                    } else if constexpr (std::is_same_v<MsgType, DisconnectMessage>) {
                        const auto it = std::ranges::find(chat_users, msg.nick);
                        if (it != std::ranges::end(chat_users)) {
                            chat_users.erase(it);
                        }
                    }  else {
                        chat_messages.push_back({.nick = "Not supported", .message = "Not supported"});
                    }
                };

                std::visit(visitor, message);
            }
            screen.PostEvent(ftxui::Event::Custom);
            pool_messages_timer.expires_after(std::chrono::milliseconds(200ms));
            pool_messages_timer.async_wait(handle_pool_messages);
        }
    };
    
    pool_messages_timer.expires_after(std::chrono::milliseconds(200ms));
    pool_messages_timer.async_wait(handle_pool_messages);

    screen.Loop(renderer);
    // ------------------------------------------------

    t.join();

    return 0;
}
