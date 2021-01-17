#include <iostream>
#include <fstream>
#include <cstdint>
#include <algorithm>

#include "file_cache.hh"
#include "server.hh"
#include "http.hh"
#include "error.hh"
#include "debug.hh"

#include "req/hist.hh"

using std::cout;
using std::endl;
using std::string;
using std::string_view;
using namespace ivanp;

int main(int argc, char* argv[]) {
  server::port_t server_port = 80;
  unsigned nthreads = std::thread::hardware_concurrency();
  unsigned epoll_buffer_size = std::max(nthreads*2,(unsigned)8);
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
      else if (var=="file_cache_default_max_size")
        ss >> ivanp::file_cache_default_max_size;
    }
  }

  server server(server_port,epoll_buffer_size,epoll_timeout);
  cout << "Listening on port " << server_port <<'\n'<< endl;

  server(nthreads, thread_buffer_size,
  [](socket sock, char* buffer, size_t buffer_size){
    // HTTP *********************************************************
    try {
      INFO("35;1","HTTP");
      http::request req(sock, buffer, buffer_size);
      if (!req) return;
      const auto g = req.get_params();

      if (strncmp(req.protocol,"HTTP/",5))
        HTTP_ERROR(400,"not HTTP protocol");

      bool keep_alive = atof(req.protocol+5) >= 1.1;
      for (auto [it,end] = req["Connection"]; it!=end; ++it) {
        keep_alive = !strcmp(it->second,"keep-alive");
      }

#ifndef NDEBUG
      cout << req.method << '\n' << req.path << '\n' << req.protocol << '\n';
      for (const auto& [key, val]: req.header)
        cout << key << ": " << val << '\n';
      cout << '\n';
      cout << g.path() << '\n';
      for (const auto& [key, val]: g.params)
        cout << (key ? key : "NULL") << ": " << (val ? val : "NULL") << '\n';
      cout << endl;
#endif

      const char* path = g.path()+1;
      if (!strcmp(req.method,"GET")) { // ===========================
        if (*path=='\0') { // serve index page ----------------------
          http::send_file(sock,"pages/index.html",
            req.qvalue("Accept-Encoding","gzip"));

        } else if (!strcmp(path,"hist")) {
          req_get_hist(sock, req);

        } else { // serve any allowed file --------------------------
          // disallow arbitrary path
          if (const char c = *path; c == '/' || c == '\\')
            HTTP_ERROR(404,"path \"",path,"\" starts with \'",c,'\'');
          if (const char c = path[strlen(path)-1]; c == '/' || c == '\\')
            HTTP_ERROR(404,"path \"",path,"\" ends with \'",c,'\'');
          for (const char* p=path; ; ++p) {
            if (const char c = *p) {
              // allow only - . / _ 09 AZ az
              if (!( ('-'<=c && c<='9') || ('A'<=c && c<='Z')
                  || c=='_' || ('a'<=c && c<='z') )) {
                HTTP_ERROR(404,"path \"",path,"\" contains \'",c,"\'");
              } else if (c=='.' && (p==path || *(p-1)=='/')) { // disallow ..
                while (*++p=='.') { }
                if (*p=='/' || *p=='\0') {
                  HTTP_ERROR(404,
                    "path \"",path,"\" contains \"",
                    string_view((p==path ? p : p-1),(*p ? p+1 : p)),"\"");
                } else --p;
              }
            } else break;
          }
          // serve a file
          http::send_file(sock,cat("files/",path).c_str(),
            req.qvalue("Accept-Encoding","gzip"));
        }
      } else if (!strcmp(req.method,"POST")) { // ===================
        if (!strcmp(path,"hist")) {
          req_post_hist(sock, req);
        } else {
          HTTP_ERROR(400,"POST with unexpected path \"",path,'\"');
        }
      } else {
        HTTP_ERROR(501,req.method," method not implemented");
      }

      // must close the socket if not keep-alive
      if (!keep_alive) sock.close();
      // must close socket on any error
    } catch (const http::error& e) {
      sock << e;
      sock.close();
      throw;
    } catch (...) {
      sock << http::status_codes.at(500);
      sock.close();
      throw;
    }
    // ******************************************************************
  });

  server.loop();
}
