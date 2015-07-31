//
// Assignment 1 startup code for Windows
//
#include <stdio.h>
#include <winsock.h>

// Define the port number to identify this process
#define MYPORT 3490

int main() {
  int s,n;
  unsigned fd;
  struct sockaddr_in my_addr;
  WSADATA wsaData;
  char *header="HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  char data[512];
  char filename[256];
  FILE *f;
   
  // Initialize windos sockets
  WSAStartup(MAKEWORD(1, 1), &wsaData);
 
  // Construct address information
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(MYPORT);
  my_addr.sin_addr.s_addr = INADDR_ANY; 
  memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero) );

  // Create a socket and bind it the port MYPORT
  s=socket(PF_INET,SOCK_STREAM, 0);
  bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));

  // Allow up to 10 incoming connections
  listen(s,10);

  while(1) {
    fd=accept(s,NULL,NULL);             // wait for a request 
    n=recv(fd,data,512,0);              // recieve the request using fd
    data[n]=0;                          // NUL terminate it
    sscanf(data,"GET /%s ",filename);   // get the name of the file
    f=fopen(filename,"rb");             // open the file (might be binary)
    send(fd,header,strlen(header),0);   // send the header
    //
    // send the file
    //
    closesocket(fd);                    // close the socket
   }
}
