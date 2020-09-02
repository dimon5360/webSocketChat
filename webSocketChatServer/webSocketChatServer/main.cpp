#include <iostream>
#include <uwebsockets/App.h>
#include <thread>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string>
#include <regex>
#include <unordered_map>

using namespace std;

struct UserConnection {
    unsigned long user_id;
    string user_name;
};


// Client -> Server
// SET_NAME=Mike - установить им€
// MESSAGE_TO=id,message - отправить сообщение пользователю ID=id

// Server -> Client
// NEW_USER,Mike,1221
// USER_LIST=

#define DEFAULT_USER_ID     10
#define CHAT_BOT_USER_ID    1

template <class K, class V, class Compare = std::less<K>, class Allocator = std::allocator<std::pair<const K, V> > >
class guarded_map {
private:
    std::map<K, V, Compare, Allocator> _map;
    std::mutex _m;

public:
    void set(K key, V value) {
        std::lock_guard<std::mutex> lk(this->_m);
        this->_map[key] = value;
    }

    V& get(K key) {
        std::lock_guard<std::mutex> lk(this->_m);
        return this->_map[key];
    }

    bool empty() {
        std::lock_guard<std::mutex> lk(this->_m);
        return this->_map.empty();
    }

    vector<string> getNames() {
        vector<string> result = { "List of connected users:" };
        for (auto entry : this->_map) {
            result.push_back("User name : " + entry.second + ", used ID" + to_string(entry.first));
        }
        return result;
    }

    // other public methods you need to implement
};
guarded_map<long, string> names;

int main()
{
    atomic_ulong latest_user_id = DEFAULT_USER_ID;

    vector<thread*> threads(thread::hardware_concurrency());

    transform(threads.begin(), threads.end(), threads.begin(), [&latest_user_id](auto* thr) {
        return new thread([&latest_user_id]() {
            uWS::App().ws<UserConnection>("/*", {
                // socket opening
                .open = [&latest_user_id](auto* ws) {
                    // „то делать при подключении пользовател€
                    UserConnection* data = (UserConnection*)ws->getUserData();
                    cout << "Total connected users numder is " << (latest_user_id - DEFAULT_USER_ID) << endl;
                    data->user_id = latest_user_id++;
                    cout << "New user connected, id " << data->user_id << endl;
                    ws->subscribe("broadcast");
                    ws->subscribe("user#" + to_string(data->user_id));
                },
                // msg processing
                .message = [&latest_user_id](auto* ws, string_view message, uWS::OpCode opCode) {
                    // „то делать при получении сообщени€
                   UserConnection* data = (UserConnection*)ws->getUserData();
                   cout << "New message " << message << " User ID " << data->user_id << endl;
                   auto beginning = message.substr(0, 9);
                   if (beginning.compare("SET_NAME=") == 0) {
                       // ѕользователь прислал свое им€
                       auto name = message.substr(9);
                       // check 'name' is valid
                       if (name.find(",") == string_view::npos && name.size() < 255) {
                           data->user_name = string(name);
                           cout << "User set their name ID = " << data->user_id << " Name = " << data->user_name << endl;
                           string broadcast_message = "NEW_USER," + data->user_name + "," + to_string(data->user_id);
                           ws->publish("broadcast", string_view(broadcast_message), opCode, false);
                           for (string mess : names.getNames()) {
                               ws->send("USER_LIST=" + mess, uWS::OpCode::TEXT, false);
                           }
                           names.set(data->user_id, data->user_name);
                       }
                   }
                   auto is_message_to = message.substr(0, 11);
                   if (is_message_to.compare("MESSAGE_TO=") == 0) {
                       // кто-то послал сообщение
                       auto rest = message.substr(11); // id,message
                       int position = rest.find(",");
                       if (position != -1) {
                           auto id_string = rest.substr(0, position);
                           int dst_user_id = atoi(id_string.data());
                           /* msg to defined user */
                           if (dst_user_id >= DEFAULT_USER_ID && dst_user_id < latest_user_id) {
                               auto user_message = rest.substr(position + 1);
                               ws->publish("user#" + string(id_string), user_message, opCode, false);
                           }
                           /* msg to bot */
                           else if (dst_user_id == CHAT_BOT_USER_ID) {
                               auto user_message = rest.substr(position + 1);
                               string resp = bot_question(string(user_message));
                               ws->send(string_view(resp), opCode, false);
                           }
                           /* msg to defined user */
                           else {
                               ws->send(string_view("Error, there is no user with ID " + string(id_string)), opCode, false);
                           }
                       }
                   }
               }
                }).listen(9999,
                    [](auto* token) {
                        if (token) {
                            cout << "Server started and listening on port 9999" << endl;
                        }
                        else {
                            cout << "Server failed to start :(" << endl;
                        }
                    }).run();
            });
        });

    for_each(threads.begin(), threads.end(), [](auto* thr) {thr->join(); });
}

void to_lower(string& str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
}

string user(string& question) {
    cout << "[USER]: " << question << endl;
    to_lower(question);
    return question;
}

void bot(string text) {
    cout << "[BOT]: " << text << endl;
}

string bot_question(string bot_query)
{
    unordered_map<string, string> database = {
        {"hello", "Oh, hello to you, hooman"},
        {"how are you", "I'm good"},
        {"what is your name", "AUTOMATED SUPERBOT 3000"},
    };

    bot("Hello, welcome to AUTOMATED SUPERBOT 3000");

    string question = user(bot_query), failResp = "[BOT]: I don't understand you :(";
    for (auto entry : database) {
        auto expression = regex(".*" + entry.first + ".*");
        if (regex_match(question, expression)) {
            return entry.second;
        }
    }

    return failResp;
}
