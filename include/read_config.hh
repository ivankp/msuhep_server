#ifndef IVANP_READ_CONFIG_HH
#define IVANP_READ_CONFIG_HH

template <typename... T>
void parse_config(const char* s, std::pair<const char*,T&&>... args) {
  for (char c;;) {
    while ((c=*s)
    ( [auto& [name,f] = args]{

      }() || ... );
  }
}

#endif
