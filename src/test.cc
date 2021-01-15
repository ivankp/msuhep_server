#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "server.hh"
#include "debug.hh"

using std::cout;
using std::string;
using namespace ivanp;

int main(int argc, char* argv[]) {
  server::port_t server_port = 80;
  unsigned nthreads = std::thread::hardware_concurrency();
  unsigned epoll_buffer_size = nthreads*2;
  int epoll_timeout = -1;
  size_t thread_buffer_size = 1<<13;

  { std::ifstream f(argc>1 ? argv[1] : "config/server");
    for (string line; getline(f,line);) {
      if (line.empty() || line[0]=='#') continue;
      std::stringstream ss(std::move(line));
      string var;
      ss >> var;
      if (var=="port") ss >> server_port;
      else if (var=="epoll_buffer_size") ss >> epoll_buffer_size;
      else if (var=="epoll_timeout") ss >> epoll_timeout;
      else if (var=="nthreads") ss >> nthreads;
      else if (var=="thread_buffer_size") ss >> thread_buffer_size;
    }
  }

  server server(server_port,epoll_buffer_size,epoll_timeout);
  cout << "Listening on port " << server_port <<'\n'<< std::endl;

  server(nthreads, thread_buffer_size,
  [](socket sock, char* buffer, size_t buffer_size){
    TEST(sock)
    sock <<
    ( "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=UTF-8\r\n"
      "Content-Length: 100000\r\n"
      "Connection: close\r\n\r\n" + std::string(100000,'\n') );
    cout << "sent" << std::endl;
    // sock.close();
  });

  server.loop();
}

