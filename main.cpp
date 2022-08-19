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

int main()
{
    connection_settings csett;

    connection_send_data send(csett);
    connection_received_data recv;

    connection conn;
    conn.connect("127.0.0.1", 6600, connection_type::SSL);

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

    std::vector<std::string> client_messages;
    std::vector<nlohmann::json> server_messages;

    std::vector<std::string> text_history;

    bool connected = false;

    sf::Clock clk;

    std::string command;

    while(1)
    {
        if(!connected && conn.client_connected_to_server)
        {
            nlohmann::json js;
            js["type"] = 0;
            js["msg"] = "connected";

            server_messages.push_back(js);

            connected = true;
        }

        conn.receive_bulk(recv);

        for(auto& [id, datas] : recv.websocket_read_queue)
        {
            for(const write_data& dat : datas)
            {
                nlohmann::json js = nlohmann::json::parse(dat.data);

                server_messages.push_back(js);
            }
        }

        while(client_messages.size() > 0)
        {
            std::string dat = client_messages.front();
            client_messages.erase(client_messages.begin());

            nlohmann::json js;
            js["type"] = 0;
            js["msg"] = dat;

            write_data wdata;
            wdata.id = -1;
            wdata.data = js.dump();

            send.write_to_websocket(wdata);
        }

        conn.send_bulk(send);

        window.poll();

        ImGui::Begin("Hi there", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        //ImGui::Text("Test Text");

        while(server_messages.size() > 0)
        {
            nlohmann::json js = server_messages.front();
            server_messages.erase(server_messages.begin());

            std::string msg = js["msg"];

            text_history.push_back(msg);
        }

        for(std::string& txt : text_history)
        {
            ImGui::Text("%s\n", txt.c_str());
        }

        if(ImGui::InputText("Input", &command, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            client_messages.push_back(command);
            command.clear();
        }

        ImGui::End();

        window.display();

        sf::sleep(sf::milliseconds(1));
    }

    return 0;
}
