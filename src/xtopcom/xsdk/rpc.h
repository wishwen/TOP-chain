#ifndef RPC_INTERFACE_H
#define RPC_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stddef.h>


#ifndef RPC_API
# if defined(_WIN32)
#  ifdef RPC_BUILD
#   define RPC_API __declspec(dllexport)
#  else
#   define RPC_API
#  endif
# elif defined(__GNUC__) && defined(RPC_BUILD)
#  define RPC_API __attribute__ ((visibility ("default")))
# else
#  define RPC_API
# endif
#endif

    RPC_API int verify_raw_elect_block(char* buf, size_t buf_len);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // RPC_INTERFACE_H
