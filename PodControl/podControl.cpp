#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_STL_

#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <fstream>
#include <iostream>
#include <set>

#include <stdlib.h>

#include "json.hpp"

#include <websocketpp/common/thread.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

using websocketpp::lib::condition_variable;
using websocketpp::lib::lock_guard;
using websocketpp::lib::mutex;
using websocketpp::lib::thread;
using websocketpp::lib::unique_lock;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

// to come in from the environment
std::string iAmPod = "1";
std::string m_docroot = "PodControl/";

enum action_type
{
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action
{
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
        : type(t), hdl(h), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};

class broadcast_server
{
public:
    broadcast_server()
    {
        // Initialize Asio Transport
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message, this, ::_1, ::_2));
        m_server.set_http_handler(bind(&broadcast_server::on_http, this, ::_1));

        // echo_server.set_http_handler(bind(&on_http,&echo_server,::_1));
    }

    void run(uint16_t port)
    {
        // listen on specified port
        m_server.listen(port);

        // Start the server accept loop
        m_server.start_accept();

        // Start the ASIO io_service run loop
        try
        {
            m_server.run();
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    void on_http(connection_hdl hdl)
    {
        // Upgrade our connection handle to a full connection_ptr
        server::connection_ptr con = m_server.get_con_from_hdl(hdl);

        std::ifstream file;
        std::string filename = con->get_resource();
        std::string response;
        //std::string m_docroot = "PodControl/";
        m_server.get_alog().write(websocketpp::log::alevel::app,
                                  "http request1: " + filename);

        if (filename == "/")
        {
            filename = m_docroot + "index.html";
        }
        else
        {
            filename = m_docroot + filename.substr(1);
        }

        m_server.get_alog().write(websocketpp::log::alevel::app,
                                  "http request2: " + filename);

        file.open(filename.c_str(), std::ios::in);
        if (!file)
        {
            // 404 error
            std::stringstream ss;

            ss << "<!doctype html><html><head>"
               << "<title>Error 404 (Resource not found)</title><body>"
               << "<h1>Error 404</h1>"
               << "<p>The requested URL " << filename << " was not found on this server.</p>"
               << "</body></head></html>";

            con->set_body(ss.str());
            con->set_status(websocketpp::http::status_code::not_found);
            return;
        }

        file.seekg(0, std::ios::end);
        response.reserve(file.tellg());
        file.seekg(0, std::ios::beg);

        response.assign((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

        con->set_body(response);
        con->set_status(websocketpp::http::status_code::ok);
    }

    void on_open(connection_hdl hdl)
    {
        {
            lock_guard<mutex> guard(m_action_lock);
            // std::cout << "on_open" << std::endl;
            m_actions.push(action(SUBSCRIBE, hdl));
        }
        m_action_cond.notify_one();
    }

    void on_close(connection_hdl hdl)
    {
        {
            lock_guard<mutex> guard(m_action_lock);
            // std::cout << "on_close" << std::endl;
            m_actions.push(action(UNSUBSCRIBE, hdl));
        }
        m_action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg)
    {
        // queue message up for sending by processing thread
        {
            lock_guard<mutex> guard(m_action_lock);
            // std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE, hdl, msg));
        }
        m_action_cond.notify_one();
    }

    void process_messages()
    {
        while (1)
        {
            unique_lock<mutex> lock(m_action_lock);

            while (m_actions.empty())
            {
                m_action_cond.wait(lock);
            }

            action a = m_actions.front();
            m_actions.pop();

            lock.unlock();

            // a message of some type has been recieved
            if (a.type == SUBSCRIBE)
            {
                lock_guard<mutex> guard(m_connection_lock);
                m_connections.insert(a.hdl);
            }
            else if (a.type == UNSUBSCRIBE)
            {
                lock_guard<mutex> guard(m_connection_lock);
                m_connections.erase(a.hdl);
            }
            else if (a.type == MESSAGE)
            {
                lock_guard<mutex> guard(m_connection_lock);
                // These messages need to process quickly
                // Expect to move this into it's own function and
                // perhaps its own thread eventually
                std::string S = a.msg->get_payload();
                std::cout << "the string is " << S << std::endl;
                // check the message for the string "start" "stop" "shutdown"
                nlohmann::json j = nlohmann::json::parse(S);
                // First, reject if this does not match our 'pod'
                if (j.contains("pod") && j["pod"] == "1")
                {
                    // right now, all of these commands work the same way
                    // it may not be that way in the future
                    // all of the commands from the control page
                    // contain all of the information they need, and
                    // one message == on command on the UE5 master system
                    if (j.contains("cmd"))
                    {
                        if (j["cmd"] == "stop" || j["cmd"] == "start" || j["cmd"] == "shutdown")
                        {
                            if (j.contains("cmd_text"))
                            {
                                std::string S = j["cmd_text"];
                                std::cout << "the system command is " << S << std::endl;

                                system(S.c_str());
                            } // valid cmd_text
                        }
                        else
                        {
                            std::cout << "cmd " << j["cmd"] << "not valid" << std::endl;
                        }
                    }
                } // pod matches

                // no matter what, we want to send out the message received to
                // everyone
                con_list::iterator it;
                for (it = m_connections.begin(); it != m_connections.end(); ++it)
                {
                    m_server.send(*it, a.msg);
                }
            }
            else
            {
                // undefined.
            }
        }
    }

private:
    typedef std::set<connection_hdl, std::owner_less<connection_hdl>> con_list;

    server m_server;
    con_list m_connections;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
};

int main(int argc, char *argv[])
{

    std::string myPod = "1";
    std::string myComputer = "1";

    std::cout << "This is the podControl program" << std::endl;
    std::cout << "     it takes three arguments, the pod, the computer and the html root" << std::endl;
    std::cout << "     podControl 1  1 ./PodControl " << std::endl;
    std::cout << std::endl;
    std::cout << "     each argument is a number between 1 and 3." << std::endl;
    std::cout << "     1 == medic   2 == crewchief 3 == copilot" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    // read cmd arguments
    if (argc == 4)
    {
        myPod = std::string(argv[1]);
        myComputer = std::string(argv[2]);
        m_docroot = std::string(argv[3]);
    }
    else
    {
        std::cout << "either not enough or too many arguments" << std::endl;
        return 1;
    }

    std::cout << "pod = " << myPod << " computer = " << myComputer << std::endl;

    try
    {
        broadcast_server server_instance;

        // Start a thread to run the processing loop
        thread t(bind(&broadcast_server::process_messages, &server_instance));

        // Run the asio loop with the main thread
        server_instance.run(9000);

        t.join();
    }
    catch (websocketpp::exception const &e)
    {
        std::cout << e.what() << std::endl;
    }
}
