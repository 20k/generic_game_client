#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <networking/networking.hpp>
#include <chrono>

template<typename T>
struct async_queue
{
    std::mutex mut;
    std::vector<T> data;

    void push(const T& d)
    {
        std::lock_guard guard(mut);
        data.push_back(d);
    }

    bool has_front()
    {
        std::lock_guard guard(mut);
        return data.size() > 0;
    }

    T pop_front()
    {
        std::lock_guard guard(mut);
        T val = data.front();
        data.erase(data.begin());
        return val;
    }
};

void connection_thread(async_queue<nlohmann::json>& server_messages, async_queue<std::string>& client_messages)
{
    connection_settings sett;

    connection_send_data send(sett);
    connection_received_data recv;

    connection conn;
    conn.connect("127.0.0.1", 6600, connection_type::SSL);

    bool connected = false;

    while(1)
    {
        if(!connected && conn.connection_pending())
        {
            std::cout << "Connected\n";

            nlohmann::json js;
            js["type"] = 0;
            js["msg"] = "connected";

            server_messages.push(js);

            connected = true;
        }

        conn.receive_bulk(recv);

        for(auto& [id, datas] : recv.websocket_read_queue)
        {
            for(const write_data& dat : datas)
            {
                nlohmann::json js = nlohmann::json::parse(dat.data);

                server_messages.push(js);
            }
        }

        while(client_messages.has_front())
        {
            std::string dat = client_messages.pop_front();

            nlohmann::json js;
            js["type"] = 0;
            js["msg"] = dat;

            write_data wdata;
            wdata.id = -1;
            wdata.data = js.dump();

            send.write_to_websocket(wdata);
        }

        conn.send_bulk(send);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void async_client_input(async_queue<std::string>& client_messages)
{
    while(1)
    {
        std::string str;
        std::getline(std::cin, str);

        client_messages.push(str);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void async_client_output(async_queue<nlohmann::json>& server_messages)
{
    while(1)
    {
        while(server_messages.has_front())
        {
            nlohmann::json js = server_messages.pop_front();

            std::cout << (std::string)js["msg"] << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main()
{
    async_queue<std::string> client_messages;
    async_queue<nlohmann::json> server_messages;

    std::thread(connection_thread, std::ref(server_messages), std::ref(client_messages)).detach();
    std::thread(async_client_input, std::ref(client_messages)).detach();
    std::thread(async_client_output, std::ref(server_messages)).detach();

    while(1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1024));
    }

    return 0;
}
