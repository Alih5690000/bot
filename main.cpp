#include <iostream>
#include <chrono>
#include "main.hpp"

int main() {
    std::cout<<"token "<<std::getenv("TOKEN")<<std::endl;
    Bot bot(std::getenv("TOKEN"), "cacert.pem");
    bot.addHandler("/start", [&](long long chat_id) {
        bot.sendMessage(chat_id, "Hello! Welcome to the bot.");
    });
    bot.addHandler("gimme image mf", [&](long long chat_id) {
        bot.sendPhoto(chat_id, "asdasdasdasd.jfif", "jfif");
    });
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        bot.Update();
    }
    return 0;
}