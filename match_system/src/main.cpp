#include "match_server/Match.h"
#include "save_client/Save.h"

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>

#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::match_service;
using namespace ::save_service;

using namespace std;

// Task
struct Task {
    User user;
    string type;
};

// Message Queue
struct MessageQueue {
    queue<Task> q;
    mutex m;
    condition_variable cv;
} message_queue;

// User matching pool
class Pool {
    public:
        // Match two users
        void match() {
            while (users.size() > 1) {
                sort(users.begin(), users.end(), [&](User& a, User b) {
                        return a.score < b.score;
                        });
                bool flag = true;
                for (uint32_t i = 1; i < users.size(); i++) {
                    auto a = users[i - 1], b = users[i];
                    if (b.score - a.score <= 50) {
                        users.erase(users.begin() + i - 1, users.begin() + i + 1);
                        save_result(a.id, b.id);
                        flag = false;
                        break;
                    }
                }
                if (flag) break;
            }
        }

        // Save result
        void save_result(int a, int b) {
            const int PORT = 9090;
            const string HOST = "123.57.47.211";
            const string USERNAME = "acs_5646";
            const string PASSWORD = "ca9189d9";
            
            printf("Match Result: %d %d\n", a, b);
            std::shared_ptr<TTransport> socket(new TSocket(HOST, PORT));
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);
            try {
                transport->open();

                int res = client.save_data(USERNAME, PASSWORD, a, b);
                if (!res) {
                    puts("Success");
                } else {
                    puts("Failed");
                }

                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }
        }

        // Add a user into the pool
        void add(User user) {
            users.push_back(user);
        }

        // Remove a user from the pool
        void remove(User user) {
            for (uint32_t i = 0; i < users.size(); i++) {
                if (users[i].id == user.id) {
                    users.erase(users.begin() + i);
                    break;
                }
            }
        }

    private:
        // Users in the pool
        vector<User> users;
}pool;

class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
        }

        // Add a user
        int32_t add_user(const User& user, const std::string& info) {
            unique_lock<mutex> lck(message_queue.m);
            printf("add_user\n");
            message_queue.q.push({user, "add"});
            message_queue.cv.notify_all();
        }

        // Remove a user
        int32_t remove_user(const User& user, const std::string& info) {
            unique_lock<mutex> lck(message_queue.m);
            printf("remove_user\n");
            message_queue.q.push({user, "remove"});
            message_queue.cv.notify_all();
        }
};

// The task of consumers
void consume_task() {
    while (true) {
        unique_lock<mutex> lck(message_queue.m);
        // If the message queue is empty, continue loop
        if (message_queue.q.empty()) {
            lck.unlock();
            pool.match();
            sleep(1);
        } else {
            // Execute a task popping from the message queue
            auto task = message_queue.q.front();
            message_queue.q.pop();
            lck.unlock();
            if (task.type == "add") {
                pool.add(task.user);
            } else if (task.type == "remove") {
                pool.remove(task.user);
            } 
            pool.match();
        }
    }
}

int main(int argc, char **argv) {
    int port = 9090;
    ::std::shared_ptr<MatchHandler> handler(new MatchHandler());
    ::std::shared_ptr<TProcessor> processor(new MatchProcessor(handler));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

    cout << "Start Match Server" << endl;

    // Start a thread for consumers to keep matching
    thread matching_thread(consume_task);

    server.serve();
    return 0;
}

