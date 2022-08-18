#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>
#include <networking/networking.hpp>
#include <chrono>
#include <toolkit/render_window.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Sleep.hpp>
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

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
            //std::cout << "Connected\n";

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

int main()
{
    render_settings sett;
    sett.width = 800;
    sett.height = 600;
    sett.is_srgb = false;

    render_window window(sett, "hello");

    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    atlas->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LCD | ImGuiFreeTypeBuilderFlags_FILTER_DEFAULT | ImGuiFreeTypeBuilderFlags_LoadColor;

    ImFontConfig font_cfg;
    font_cfg.GlyphExtraSpacing = ImVec2(0, 0);
    font_cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LCD | ImGuiFreeTypeBuilderFlags_FILTER_DEFAULT | ImGuiFreeTypeBuilderFlags_LoadColor;

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF("DosFont.ttf", 16, &font_cfg);

    async_queue<std::string> client_messages;
    async_queue<nlohmann::json> server_messages;

    std::thread(connection_thread, std::ref(server_messages), std::ref(client_messages)).detach();

    std::vector<std::string> text_history;

    std::string command;

    while(1)
    {
        window.poll();

        ImGui::Begin("Hi there", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        //ImGui::Text("Test Text");

        while(server_messages.has_front())
        {
            nlohmann::json js = server_messages.pop_front();

            std::string msg = js["msg"];

            text_history.push_back(msg);
        }

        for(std::string& txt : text_history)
        {
            ImGui::Text("%s\n", txt.c_str());
        }

        if(ImGui::InputText("Input", &command, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            client_messages.push(command);
            command.clear();
        }

        ImGui::End();

        window.display();

        sf::sleep(sf::milliseconds(1));
    }

    return 0;
}
