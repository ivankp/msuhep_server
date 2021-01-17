#ifndef IVANP_SERVER_REQ_HIST_HH
#define IVANP_SERVER_REQ_HIST_HH

#include <string_view>
#include "http.hh"

void req_post_hist(ivanp::socket sock, const ivanp::http::request& req);
void req_get_hist(ivanp::socket sock, const ivanp::http::request& req);

#endif
