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
#include <js_imgui/js_imgui_client.hpp>

struct ui_window
{
    ui_stack stk;
    int server_id = 1;
    uint32_t acked_sequence_id = 0;
};

void process_server_ui(ui_window& run, nlohmann::json& in)
{
    int id = in["id"];

    run.server_id = id;

    std::map<std::string, ui_element> existing_elements;

    for(const ui_element& e : run.stk.elements)
    {
        if(e.element_id == "")
            continue;

        existing_elements[e.element_id] = e;
    }

    run.stk = ui_stack();

    std::vector<std::string> typelist = in["typeidx"];
    std::vector<int> typeargc = in["typeargc"];

    if(in.count("client_seq_ack") > 0)
        run.acked_sequence_id = in["client_seq_ack"];

    int num = in["types"].size();
    int current_argument_idx = 0;

    for(int i=0; i < num; i++)
    {
        int idx = in["types"][i];

        std::vector<nlohmann::json> arguments;

        std::string val = typelist.at(idx);
        int argument_count = typeargc.at(idx);

        for(int kk=0; kk < argument_count; kk++)
        {
            arguments.push_back(in["arguments"][kk + current_argument_idx]);
        }

        current_argument_idx += argument_count;

        //std::vector<nlohmann::json> arguments = (std::vector<nlohmann::json>)(in["arguments"][i]);

        std::string element_id = get_element_id(val, arguments);

        ui_element elem;

        if(auto it = existing_elements.find(element_id); it != existing_elements.end())
        {
            elem = it->second;
        }

        elem.element_id = element_id;

        elem.type = val;

        ///if the sequence id of us is > than the current acked id, it means we're client authoritative for a bit
        if((int)elem.arguments.size() != argument_count || run.acked_sequence_id >= elem.authoritative_until_sequence_id)
            elem.arguments = arguments;

        if((int)elem.arguments.size() != argument_count)
            throw std::runtime_error("Bad argument count somehow");

        run.stk.elements.push_back(elem);
    }
}

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

    ui_window ui;
    uint64_t sequence_id_in = 0;

    while(!window.should_close())
    {
        if(!connected && conn.client_connected_to_server)
        {
            /*nlohmann::json js;
            js["type"] = 0;
            js["msg"] = "connected";

            server_messages.push_back(js);*/

            printf("Connected\n");

            connected = true;
        }

        if(!conn.client_connected_to_server && !conn.connection_in_progress && clk.getElapsedTime().asSeconds() > 1)
        {
            conn.connect("127.0.0.1", 6600, connection_type::SSL);
            clk.restart();

            connected = false;
        }

        conn.receive_bulk(recv);

        for(auto& [id, datas] : recv.websocket_read_queue)
        {
            for(const write_data& dat : datas)
            {
                nlohmann::json js = nlohmann::json::parse(dat.data);

                if(js["type"] == "command_realtime_ui")
                {
                    process_server_ui(ui, js);
                }
                else
                {
                    server_messages.push_back(js);
                }
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

        render_ui_stack(send, sequence_id_in, ui.stk, 0, true);

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
