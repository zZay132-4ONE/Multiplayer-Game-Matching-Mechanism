#include "match_server/Match.h"
#include "save_client/Save.h"

#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TSocket.h>
#include <thrift/TToString.h>

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
        // Check if two users match
        bool check_match(uint32_t i, uint32_t j) {
            auto a = users[i], b = users[j];
            int delta = abs(a.score - b.score);
            int a_max_diff = waiting_time[i] * 50;
            int b_max_diff = waiting_time[j] * 50;
            return delta <= a_max_diff && delta <= b_max_diff;
        }

        // Match two users
        void match() {
            // Update every user's waiting time
            for (uint32_t i = 0; i < waiting_time.size(); i++) {
                waiting_time[i]++;
            }
            // Match
            while (users.size() > 1) {
                bool flag = true;
                for (uint32_t i = 0; i < users.size(); i++) {
                    for (uint32_t j = i + 1; j < users.size(); j++) {
                        if (check_match(i, j)) {
                            // Remember to erase the latter one first, or it will affect the order
                            auto a = users[i], b = users[j];
                            users.erase(users.begin() + j);
                            users.erase(users.begin() + i);
                            waiting_time.erase(waiting_time.begin() + j);
                            waiting_time.erase(waiting_time.begin() + i);
                            save_result(a.id, b.id);
                            flag = false;
                            break;
                        }
                    }
                    if (!flag) break;
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
            waiting_time.push_back(0);
        }

        // Remove a user from the pool
        void remove(User user) {
            for (uint32_t i = 0; i < users.size(); i++) {
                if (users[i].id == user.id) {
                    users.erase(users.begin() + i);
                    waiting_time.erase(waiting_time.begin() + i);
                    break;
                }
            }
        }

    private:
        // Users in the pool
        vector<User> users;
        // Match waiting time
        vector<int> waiting_time;
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
            return 0;
        }

        // Remove a user
        int32_t remove_user(const User& user, const std::string& info) {
            unique_lock<mutex> lck(message_queue.m);
            printf("remove_user\n");
            message_queue.q.push({user, "remove"});
            message_queue.cv.notify_all();
            return 0;
        }
};

class MatchCloneFactory : virtual public MatchIfFactory {
    public:
        ~MatchCloneFactory() override = default;
        MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
        {
            std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
            cout << "Incoming connection\n";
            cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
            cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
            cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
            cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";
            return new MatchHandler;
        }
        void releaseHandler(MatchIf* handler) override {
            delete handler;
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
        }
    }
}

int main(int argc, char **argv) {
    TThreadedServer server(
            std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
            std::make_shared<TServerSocket>(9090),
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>());

    cout << "Start Match Server" << endl;

    // Start a thread for consumers to keep matching
    thread matching_thread(consume_task);

    server.serve();
    return 0;
}

