//for c and posix
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

//for c++
#include <string>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>

namespace cnc
{
    class ThreadPool
    {
    public:
        ThreadPool(size_t);

        template<class F, class... Args>
        auto enqueue(F &&f, Args &&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

        ~ThreadPool();

    private:
        // need to keep track of threads so we can join them
        std::vector<std::thread> workers;
        // the task queue
        std::queue<std::function<void()>> tasks;

        // synchronization
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
    };

    // the constructor just launches some amount of workers
    inline ThreadPool::ThreadPool(size_t threads)
            : stop(false)
    {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back(
                    [this]
                    {
                        for (; ;)
                        {
                            std::function<void()> task;

                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex);
                                this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                                if (this->stop && this->tasks.empty())
                                    return;
                                task = std::move(this->tasks.front());
                                this->tasks.pop();
                            }

                            task();
                        }
                    }
            );
    }

    // add new work item to the pool
    template<class F, class... Args>
    auto ThreadPool::enqueue(F &&f, Args &&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>
        (
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // don't allow enqueueing after stopping the pool
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    // the destructor joins all threads
    inline ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker: workers) worker.join();
    }

    template<typename Socket>
    void send_file(std::string const &filename, Socket const &socket)
    {
        std::ifstream ifs{filename};
        for (std::string str; ifs >> str;)
        {
            auto msg = str + "\r\n";
            send(socket, msg.c_str(), msg.size(), 0);
        }
    }

    template <typename StatusCode>
    auto make_reply_header(StatusCode code) -> std::string
    {
        const static std::unordered_map<StatusCode, std::string> status
                {
                        {200, "200 OK"},
                        {404, "404 Not Found"},
                        {501, "501 Not Implemented"}
                };

        return "HTTP/1.1 "  + status.at(code) + "\r\nContent-Type: text/html\r\n\r\n";
    }

    template <typename Socket>
    void enable_port_reusable(Socket soc)
    {
        auto optval = 1;
        setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
    }

    template <typename Socket, typename Integer>
    class Server
    {
    public:
        explicit Server(int port_no, std::size_t incoming_limit, Integer max_times = 9999) :
                _socket{ init_socket(port_no) },
                _port{ port_no },
                _limit{ incoming_limit },
                _pool( incoming_limit ),
                _max_times{ max_times }
        {
            listen(_socket, (int)incoming_limit);
        }

        auto start( int delay ) -> Integer
        {
           return run(_max_times, delay);
        }

        ~Server()
        {
            close(_socket);
        }

    private:
        Socket const _socket;
        int const _port;
        std::size_t const _limit;
        ThreadPool _pool;
        Integer const _max_times;

        auto init_socket(std::size_t port) const -> Socket
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;
            memset( addr.sin_zero, '\0', sizeof(addr.sin_zero) );

            // Create a socket and bind it the port PORT
            auto const soc = socket(PF_INET,SOCK_STREAM, 0);

            cnc::enable_port_reusable(soc);
            auto bind_result = bind(soc, (struct sockaddr *)&addr, sizeof(addr));
            if (bind_result != 0)
                std::cout << errno << std::endl;

            return soc;
        }

        auto run(Integer const max, int delay) -> Integer
        {
            auto curr = max;
            for(auto const header = cnc::make_reply_header(200); curr != 0; --curr)
            {
                auto request_handler = [&](Socket socket){
                    std::cout << "log -> Thread[" << std::this_thread::get_id()
                              << "] is working on socket ["  << socket << "]" << std::endl;

                    char data[512];
                    char filename[256];
                    auto size = recv(socket, data, 512, 0);                 // recieve the request using fd
                    data[size] = 0;                                         // NUL terminate it
                    sscanf(data, "GET /%s ", filename);                     // get the name of the file
                    send(socket, header.c_str(), header.size(), 0);
                    std::this_thread::sleep_for(std::chrono::seconds(delay));
                    cnc::send_file(filename, socket);
                    close(socket);                                          // close the socket
                };

                _pool.enqueue(request_handler, accept(_socket, NULL, NULL));
            }

            return  max - curr;
        }
    };

}// end of namespace cnc

int main()
{
    auto port = 3490;
    auto limit = 10u;
    cnc::Server< int, long long > server{ port, limit };
    std::cout << "log -> Server configed with port number [" << port << "] and at most [" << limit << "] connections\n";

    auto delay = 5u;
    std::cout << "log -> Server started with [" << delay << "] in each thread\n";
    server.start(delay);
}
