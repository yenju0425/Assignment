#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include "connection_metadata.h"
#include "exchange.h"

using namespace std;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

connection_metadata::connection_metadata(int id, websocketpp::connection_hdl hdl, string uri, Exchange* exchange)
    : m_id(id), m_hdl(hdl), m_status("Connecting"), m_uri(uri), m_server("N/A"), m_exchange(exchange) {

    // Create and open the file for writing
    string path = __fs::filesystem::current_path().string(); // name followed by '::' must be a class or namespace name !!!
    string filename = path + "/" + exchange->getName() + "_" + to_string(id) + ".txt"; // pointer to incomplete class type "Exchange" is not allowed !!!!!!!
    
    m_message_file.open(filename, ios::out);
}

context_ptr connection_metadata::on_tls_init(client* c, websocketpp::connection_hdl hdl) {
    context_ptr ctx = make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                        boost::asio::ssl::context::no_sslv2 |
                        boost::asio::ssl::context::no_sslv3 |
                        boost::asio::ssl::context::single_dh_use);
    } catch (exception &e) {
        cout << "Error in context pointer: " << e.what() << endl;
    }
    return ctx;
}

void connection_metadata::on_open(client* c, websocketpp::connection_hdl hdl) {
    m_status = "Open";
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    m_server = con->get_response_header("Server");
}

void connection_metadata::on_fail(client* c, websocketpp::connection_hdl hdl) {
    m_status = "Failed";

    client::connection_ptr con = c->get_con_from_hdl(hdl);
    m_server = con->get_response_header("Server");
    m_error_reason = con->get_ec().message();
}

void connection_metadata::on_close(client* c, websocketpp::connection_hdl hdl) {
    m_status = "Closed";
    client::connection_ptr con = c->get_con_from_hdl(hdl);
    stringstream s;
    s << "close code: " << con->get_remote_close_code() << " ("
        << websocketpp::close::status::get_string(con->get_remote_close_code())
        << "), close reason: " << con->get_remote_close_reason();
    m_error_reason = s.str();
}

void connection_metadata::on_message(websocketpp::connection_hdl, client::message_ptr msg) {
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
        if (m_message_file.is_open()) {
            if (msg->get_payload() != "{\"event\":\"heartbeat\"}") { // ignore heartbeat message
                auto now = chrono::system_clock::now();
                time_t time_now = chrono::system_clock::to_time_t(now);

                char timestamp_buffer[80];
                strftime(timestamp_buffer, sizeof(timestamp_buffer), "[%Y-%m-%d %H:%M:%S] ", localtime(&time_now));

                m_message_file << timestamp_buffer << "<< " + msg->get_payload() << endl;
                // m_message_file.flush();
            }
        }
        else {
            cout << "Error: file is not open" << endl;
        }
    } else {
        m_messages.push_back("<< " + websocketpp::utility::to_hex(msg->get_payload()));
    }
}

websocketpp::connection_hdl connection_metadata::get_hdl() const {
    return m_hdl;
}

int connection_metadata::get_id() const {
    return m_id;
}

string connection_metadata::get_status() const {
    return m_status;
}

void connection_metadata::record_sent_message(string message) {
    m_messages.push_back(">> " + message);
}

ostream & operator<< (ostream & out, connection_metadata const & data) {
    out << "> URI: " << data.m_uri << "\n"
        << "> Status: " << data.m_status << "\n"
        << "> Remote Server: " << (data.m_server.empty() ? "None Specified" : data.m_server) << "\n"
        << "> Error/close reason: " << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason) << "\n";
    out << "> Messages Processed: (" << data.m_messages.size() << ") \n";

    vector<string>::const_iterator it;
    for (it = data.m_messages.begin(); it != data.m_messages.end(); ++it) {
        out << *it << "\n";
    }

    return out;
}