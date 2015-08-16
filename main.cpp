#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
            -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
        :   stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
                [this]
                {
                    for(;;)
                    {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                                 [this]{ return this->stop || !this->tasks.empty(); });
                            if(this->stop && this->tasks.empty())
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
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
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
    for(std::thread &worker: workers)
        worker.join();
}

template <typename Socket>
void send_file(std::string const& filename, Socket const& socket)
{
    std::ifstream ifs{ filename };
    for(std::string s; ifs >> s; )
    {
        auto msg = s + "\r\n";
        send(socket, msg.c_str(), msg.size(), 0);
    }
}

// Define the port number to identify this process
#define MYPORT 3490

int main()
{
    int s;
    unsigned fd;
    struct sockaddr_in my_addr;
    const char *header="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";

    FILE *f;

    // Construct address information
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYPORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero) );

    // Create a socket and bind it the port MYPORT
    s=socket(PF_INET,SOCK_STREAM, 0);
    bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
    printf("%d", my_addr.sin_port);

    // Allow up to 10 incoming connections
    const auto limit = 10;
    ThreadPool pool{ 10 };
    listen(s,limit);
    std::cout << "listenning\n";

    while(1)
    {
        fd=accept(s,NULL,NULL);             // wait for a request
        std::cout << "accepted a request\n";

        auto request_handler = [&] {
            std::cout << "inside thread\n";

            char data[512];
            char filename[256];
            auto n = recv(fd, data, 512, 0);              // recieve the request using fd
            data[n] = 0;                          // NUL terminate it
            sscanf(data, "GET /%s ", filename);   // get the name of the file
            send(fd, header, strlen(header), 0);   // send the header
            std::this_thread::sleep_for(std::chrono::seconds(3));
            send_file(filename, fd);
            close(fd);                    // close the socket
        };
        pool.enqueue(request_handler);
    }
}
