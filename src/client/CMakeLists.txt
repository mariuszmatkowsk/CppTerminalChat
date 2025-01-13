include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)

# you need crete git folder to use this library
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

add_executable(
    client
    client.cpp
    # Connection.cpp
)

target_link_libraries(
    client
    PRIVATE asio
    # PRIVATE ftxui::screen
    # PRIVATE ftxui::dom
    # PRIVATE ftxui::component
)
