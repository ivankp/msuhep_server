#include "req/hist.hh"

#include <string>
#include <map>
#include <filesystem>
#include <mutex>
#include <shared_mutex>

#include <sys/stat.h>
#include <sys/time.h>

#include <nlohmann/json.hpp>

#include "sqlite3/sqlite.hh"
#include "file_cache.hh"
#include "lex_str_sort.hh"
#include "error.hh"
#include "debug.hh"

using nlohmann::json;
using namespace ivanp;

void req_get_hist(ivanp::socket sock, const ivanp::http::request& req) {
  if (const auto f = file_cache("pages/hist/page.html")) {
    http::send_str(
      sock, replace_first(*f,"<!-- @VARS -->",[]{
        std::vector<std::string> names;
        for (const auto& x :
          std::filesystem::directory_iterator("files/hist/data")
        ) {
          auto name = x.path().filename().string();
          if (!name.ends_with(".cols")) continue;
          name.erase(name.size()-5);
          names.emplace_back(std::move(name));
        }

        lex_str_sort(names);

        std::stringstream ss;
        ss << "\nconst dbs = [";
        for (bool first=true; const auto& name : names) {
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
std::shared_mutex mx_db_map;

sqlite& get_db(const std::string& name) {
  const std::string fname = "files/hist/data/"+name+".db";
  struct stat s;
  PCALL(stat)(fname.c_str(),&s);
  if (!S_ISREG(s.st_mode)) ERROR(fname," is not a regular file");
  const std::pair mtime(
    s.st_mtim.tv_sec,
    s.st_mtim.tv_nsec
  );
  { std::shared_lock lock(mx_db_map);
    if (auto it = db_map.find(name);
      it != db_map.end() && mtime == it->second.second
    ) return it->second.first;
  }
  { std::unique_lock lock(mx_db_map);
    return db_map.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(name),
      std::forward_as_tuple(fname,mtime)
    ).first->second.first;
  }
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

  std::vector<std::string> cols;
  bool blob = false;
  db("pragma table_info(hist)",
  [&](int ncol, char** row, char** cols_names) mutable {
    for (int i=0; i<ncol; ++i) {
      cols.emplace_back(row[1]);
      if (!blob && !strcmp(row[1],"_data") && !strcmp(row[2],"BLOB"))
        blob = true;
    }
  });

  const auto& labels = jreq["labels"];
  // TEST(labels)
  for (const auto& [key,val] : labels.items()) {
    for (const auto& col : cols)
      if (key==col) goto key_ok;
    HTTP_ERROR(400,"invalid table column \"",key,"\"");
key_ok: ;
  }

  std::stringstream out;
  out << '[';

  if (blob) {
    std::stringstream sql;
    sql << "SELECT uniform, edges, _head, _data, var1";
    for (const auto& [key,val] : labels.items())
      if (val.size() > 1)
        sql << ", " << key;
    sql << " FROM hist"
      " INNER JOIN axis_dict ON hist._axis = axis_dict.rowid"
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

    { bool first = true;
      auto stmt = db.prepare(sql.str().c_str());
      while (stmt.step()) {
        if (first) first = false;
        else out << ',';
        out << '[';

        const bool njets = starts_with(stmt.column_text(4),"Njets_");

        if (stmt.column_int(0)) { // uniform axis
          const auto axis = json::parse(stmt.column_text(1));
          const int nbins = axis[0];
          out << '[';
          if (njets) {
            for (int n=nbins+2, i=0; i<n; ++i) {
              if (i) out << ',';
              out << i;
            }
          } else {
            const double a = axis[1], b = axis[2];
            const double width = (b-a)/nbins;
            for (int n=nbins+1, i=0; i<n; ++i) {
              if (i) out << ',';
              out << (a + i*width);
            }
          }
          out << ']';
        } else {
          out << stmt.column_text(1);
        }

        if (strcmp(stmt.column_text(2),"f8,f8,u4"))
          ERROR("unexpected hist bin format ",stmt.column_text(2));

        out << ",[";
        if (njets) {
          out << "0,0,";
        }
        { int data_len = stmt.column_bytes(3);
          const char* data =
            reinterpret_cast<const char*>(stmt.column_blob(3));
          bool first_bin = true;
          double b[2];
          while (data_len) {
            if (data_len < 0) ERROR("wrong hist data length");
            if (first_bin) first_bin = false;
            else out << ',';
            memcpy(b,data,16);
            out << b[0] << ',' << b[1];
            data += 20;
            data_len -= 20;
          }
        }
        out << "],\"";
        const int ncol = stmt.column_count();
        for (int i=5; i<ncol; ++i) {
          if (i) out << ' ';
          const char* x = stmt.column_text(i);
          out << (*x=='\0' ? "*" : x);
        }
        out << "\"]";
      }
    }

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

    db(sql.str().c_str(),
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
    // TEST(out.str())
  }

  out << ']';
  // TEST(out)
  http::send_str(sock, out.str(), "json",
    req.qvalue("Accept-Encoding","gzip"));
}
