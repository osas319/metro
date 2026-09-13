#pragma once
// Bağımlılıksız, renkli, printf-tarzı basit logger.
// İleride Tracy/event bus entegre olduğunda da arayüz aynı kalabilir.
#include <cstdarg>
#include <cstdio>

namespace metro::core {

enum class LogLevel : int { Info, Warn, Error };

inline void log(LogLevel level, const char* fmt, ...) noexcept {
  static const char* tags[] = {
      "\033[36m[INFO] \033[0m",
      "\033[33m[WARN] \033[0m",
      "\033[31m[ERROR]\033[0m",
  };
  std::fputs(tags[static_cast<int>(level)], stderr);
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);
  std::fputc('\n', stderr);
  std::fflush(stderr);
}

} // namespace metro::core

#define METRO_INFO(...)  ::metro::core::log(::metro::core::LogLevel::Info, __VA_ARGS__)
#define METRO_WARN(...)  ::metro::core::log(::metro::core::LogLevel::Warn, __VA_ARGS__)
#define METRO_ERROR(...) ::metro::core::log(::metro::core::LogLevel::Error, __VA_ARGS__)

// Değişmez ihlalleri: mesajı yaz ve düşür — sessiz bozuk duruma geçme.
#include <cstdlib>
#define METRO_ASSERT(cond, ...)                                        \
  do {                                                                 \
    if (!(cond)) {                                                     \
      METRO_ERROR("ASSERT (%s) %s:%d — ", #cond, __FILE__, __LINE__);  \
      METRO_ERROR(__VA_ARGS__);                                        \
      std::abort();                                                    \
    }                                                                  \
  } while (false)
