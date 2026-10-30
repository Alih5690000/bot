#include <iostream>
#include "main.hpp"

int main() {
    std::cout<<"token "<<std::getenv("TOKEN")<<std::endl;
    Bot bot(std::getenv("TOKEN"), "cacert.pem");
    bot.addHandler("/start", [&](long long chat_id) {
        bot.sendMessage(chat_id, "Hello! Welcome to the bot.");
    });
    bot.infinite_poll();
    return 0;
}