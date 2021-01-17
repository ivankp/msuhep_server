#include "req/hist.hh"

#include <string>
#include <map>
#include <filesystem>

#include <sys/stat.h>
#include <sys/time.h>

#include <nlohmann/json.hpp>

#include "sqlite3/sqlite.hh"
#include "file_cache.hh"
#include "error.hh"
#include "debug.hh"

using nlohmann::json;
using namespace ivanp;

void req_get_hist(ivanp::socket sock, const ivanp::http::request& req) {
  if (const auto f = file_cache("pages/hist/page.html")) {
    http::send_str(
      sock, replace_first(*f,"<!-- @VARS -->",[]{
        std::stringstream ss;
        ss << "\nconst dbs = [";
        for (bool first=true; const auto& x :
          std::filesystem::directory_iterator("files/hist/data")
        ) {
          auto name = x.path().filename().string();
          if (!name.ends_with(".db")) continue;
          name.erase(name.size()-3);
          if (first) first = false;
          else ss << ',';
          ss << std::quoted(name);
        }
        ss << "];\n"
          "const page = \"hist\";\n";
        return std::move(ss).str();
      }()), "html",
      req.qvalue("Accept-Encoding","gzip")
    );
  } else {
    // TODO: send file without caching
    HTTP_ERROR(500,"page cannot be cached");
  }
}

std::map<std::string,std::pair<
  sqlite,
  std::pair<
    decltype(std::declval<struct stat>().st_mtim.tv_sec),
    decltype(std::declval<struct stat>().st_mtim.tv_nsec)
  >
>> db_map;

sqlite& get_db(const std::string& name) {
  const std::string fname = "files/hist/data/"+name+".db";
  struct stat s;
  PCALL(stat)(fname.c_str(),&s);
  if (!S_ISREG(s.st_mode)) ERROR(fname," is not a regular file");
  const std::pair mtime(
    s.st_mtim.tv_sec,
    s.st_mtim.tv_nsec
  );
  if (auto it = db_map.find(name);
    it != db_map.end() && mtime < it->second.second
  ) return it->second.first;
  return db_map.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(name),
    std::forward_as_tuple(fname,mtime)
  ).first->second.first;
}

void req_post_hist(ivanp::socket sock, const ivanp::http::request& req) {
  const auto jreq = json::parse(req.data);
  sqlite& db = [&]() -> auto& {
    try {
      return get_db(jreq["db"].get_ref<const json::string_t&>());
    } catch (const std::exception& e) {
      throw http::error(404,e.what());
    }
  }();

  static auto stmt = db.prepare("pragma table_info(hist)", true);
  std::vector<std::string> cols;
  bool blob = false;
  while (stmt.step()) {
    const char* col = stmt.column_text(1);
    if (!blob && !strcmp(col,"_data") && !strcmp(stmt.column_text(2),"BLOB"))
      blob = true;
    cols.emplace_back(col);
  }

  const auto& labels = jreq["labels"];
  // TEST(labels)
  for (const auto& [key,val] : labels.items()) {
    for (const auto& col : cols)
      if (key==col) goto key_ok;
    HTTP_ERROR(400,"invalid table column \"",key,"\"");
key_ok: ;
  }

  std::string resp;

  if (blob) {
    // TODO
  } else {
    std::stringstream sql;
    sql << "SELECT edges, bins";
    for (const auto& [key,val] : labels.items())
      if (val.size() > 1)
        sql << ", " << key;
    sql << " FROM hist"
      " INNER JOIN axes ON hist.axis = axes.id"
      " WHERE ";
    bool first = true;
    for (const auto& [key,xs] : labels.items()) {
      if (first) first = false;
      else sql << " AND ";
      sql << '(';
      { bool first = true;
        for (const auto& x : xs) {
          if (first) first = false;
          else sql << " OR ";
          sql << key << '=' << x;
        }
      }
      sql << ')';
    }
    // TEST(sql.str())

    std::stringstream out;
    out << '[';
    db.exec(sql.str().c_str(),
    [&,first=true](int ncol, char** row, char** cols_names) mutable {
      if (first) first = false;
      else out << ',';
      out << "[[" << row[0] << "],[" << row[1] << "],\"";
      ncol -= 2;
      row += 2;
      for (int i=0; i<ncol; ++i) {
        if (i) out << ' ';
        out << (*row[i]=='\0' ? "*" : row[i]);
      }
      out << "\"]";
    });
    out << ']';
    // TEST(out.str())
    resp = std::move(out).str();
  }

  // TEST(resp)
  http::send_str(sock, resp, "json", req.qvalue("Accept-Encoding","gzip"));
}
