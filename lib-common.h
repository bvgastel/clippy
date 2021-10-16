
#define ENSURE(x) if (!(x)) { ::abort(); }
#define USING(x) ((void)(x))
#if defined(__clang__) || defined(__GNUC__)
#define FORMAT(x, y, z) __attribute__ ((format (x, y, z)))
#define SIGN_CONVERSION(x) \
      _Pragma("clang diagnostic push") \
      _Pragma("clang diagnostic ignored \"-Wsign-conversion\"") \
      x \
      _Pragma("clang diagnostic pop")
#define OLD_STYLE_CAST(x) \
      _Pragma("clang diagnostic push") \
      _Pragma("clang diagnostic ignored \"-Wold-style-cast\"") \
      x \
      _Pragma("clang diagnostic pop")
#else
#define FORMAT(x, y, z)
#define SIGN_CONVERSION(x) x
#define OLD_STYLE_CAST(x) x
#endif

