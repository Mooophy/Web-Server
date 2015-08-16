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
    listen(s,10);
    std::cout << "listenning\n";

    while(1)
    {
        fd=accept(s,NULL,NULL);             // wait for a request
        std::cout << "accepted a request\n";
        std::thread request_handler
        {
                [&]{
                    std::cout << "inside thread\n";

                    char data[512];
                    char filename[256];
                    auto n = recv(fd,data,512,0);              // recieve the request using fd
                    data[n] = 0;                          // NUL terminate it
                    sscanf(data,"GET /%s ",filename);   // get the name of the file
                    send(fd,header,strlen(header),0);   // send the header

                    std::this_thread::sleep_for(std::chrono::seconds(3));

                    send_file(filename, fd);

                    close(fd);                    // close the socket
                }
        };

        std::cout << "Thread created wit id = " << request_handler.get_id() << std::endl;
        request_handler.detach();
//        fd=accept(s,NULL,NULL);             // wait for a request
//
//        std::cout << "\na request accepted at:";
//        system("date");
//        std::cout << std::endl;
//
//        n=recv(fd,data,512,0);              // recieve the request using fd
//        data[n]=0;                          // NUL terminate it
//        sscanf(data,"GET /%s ",filename);   // get the name of the file
//        f=fopen(filename,"rb");             // open the file (might be binary)
//        send(fd,header,strlen(header),0);   // send the header
//        //
//        // send the file
//        //
//        send_file(filename, fd);
//        close(fd);                    // close the socket
    }
}
