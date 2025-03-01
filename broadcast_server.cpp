#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>
#include <set>

#include <stdlib.h>

#include "json.hpp"

/*#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>*/
#include <websocketpp/common/thread.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

// to come in from the environment
std::string iAmPod = "1";
std::string iAmA = "copilot";

enum action_type {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action {
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
      : type(t), hdl(h), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};

class broadcast_server {
public:
    broadcast_server() {
        // Initialize Asio Transport
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open,this,::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close,this,::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message,this,::_1,::_2));
    }

    void run(uint16_t port) {
        // listen on specified port
        m_server.listen(port);

        // Start the server accept loop
        m_server.start_accept();

        // Start the ASIO io_service run loop
        try {
            m_server.run();
        } catch (const std::exception & e) {
            std::cout << e.what() << std::endl;
        }
    }

    void on_open(connection_hdl hdl) {
        {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_open" << std::endl;
            m_actions.push(action(SUBSCRIBE,hdl));
        }
        m_action_cond.notify_one();
    }

    void on_close(connection_hdl hdl) {
        {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_close" << std::endl;
            m_actions.push(action(UNSUBSCRIBE,hdl));
        }
        m_action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        // queue message up for sending by processing thread
        {
            lock_guard<mutex> guard(m_action_lock);
            //std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE,hdl,msg));
        }
        m_action_cond.notify_one();
    }

    void process_messages() {
        while(1) {
            unique_lock<mutex> lock(m_action_lock);

            while(m_actions.empty()) {
                m_action_cond.wait(lock);
            }

            action a = m_actions.front();
            m_actions.pop();

            lock.unlock();

            // a message of some type has been recieved
            if (a.type == SUBSCRIBE) {
                lock_guard<mutex> guard(m_connection_lock);
                m_connections.insert(a.hdl);
            } else if (a.type == UNSUBSCRIBE) {
                lock_guard<mutex> guard(m_connection_lock);
                m_connections.erase(a.hdl);
            } else if (a.type == MESSAGE) {
                lock_guard<mutex> guard(m_connection_lock);
                   // These messages need to process quickly
                   // Expect to move this into it's own function and
                   // perhaps its own thread eventually
                   std::string S = a.msg->get_payload();
                   std::cout << "the string is "<< S << std::endl;
                   //sending {"id":"2","cmd":"stop"} chat.html:175:13
                   //check the message for the string "start" "stop" "shutdown"
                   nlohmann::json j = nlohmann::json::parse(S);
                   if (j.contains("cmd"))
                   {
                      std::cout << j["cmd"] << " is the cmd " << std::endl;
                      // for testing the idea
                      if (j["cmd"]=="stop" && j["id"] == "1" )
                      {
                          std::cout << "command matches" << std::endl;
                          if (j.contains("cmd_text"))
                          {
                          //std::string S = "/bin/bash echo " +std::string(j["cmd_text"]) + "&";
                              std::string S="winexe -U ADMIN%1234 //169.254.112.122 ";
                              S+=std::string("cmd /k C:\\Users\\admin\\Desktop\\") + std::string("\"Run MoH\"") + std::string("\\Stop_MoH.bat");


                          //winexe -U ADMIN%1234 //169.254.112.122 'cmd /k C:\Users\admin\Desktop\"Run MoH\"\\Stop_MoH.bat'
                          //std::string S = "/bin/bash -c wsexe ";
                          int stat = system(S.c_str());
                          std::cout << "system command with " + S << std::endl;
                          std::cout << "return value is " << stat << std::endl;
                       }
                   }
                   con_list::iterator it;
                   for (it = m_connections.begin(); it != m_connections.end(); ++it) {
                       m_server.send(*it,a.msg);
                   }
            }
            }
            else {
                // undefined.
            }
        }
    }
private:
    typedef std::set<connection_hdl,std::owner_less<connection_hdl> > con_list;

    server m_server;
    con_list m_connections;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
};

int main(int argc,char* argv[]) {
    // read cmd arguments
#if 0
    if (argc>0)
    {
        std::cout << argv[0] << "  " << argv[1] << std::endl;
    }
    std::string pod;
#endif
    try {
    broadcast_server server_instance;

    // Start a thread to run the processing loop
    thread t(bind(&broadcast_server::process_messages,&server_instance));

    // Run the asio loop with the main thread
    server_instance.run(9000);

    t.join();

    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    }
}
