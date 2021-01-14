#ifndef IVANP_SCOPE_GUARD_HH
#define IVANP_SCOPE_GUARD_HH

template <typename F>
class scope_guard {
  F f;
public:
  explicit scope_guard(F&& f): f(std::forward<F>(f)) { }
  scope_guard() = delete;
  scope_guard(const scope_guard&) = delete;
  scope_guard(scope_guard&&) = delete;
  scope_guard& operator=(const scope_guard&) = delete;
  scope_guard& operator=(scope_guard&&) = delete;
  ~scope_guard() { f(); }
};

#endif
