#ifndef __SUGAR2__
#define __SUGAR2__

#include <cstring>

#ifndef foreach
#define foreach(_i, _b, _e) \
	  for(auto _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)
#endif

#if 	(defined(__clang__) || defined (__GNUC__))
#define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
#define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif

#include <vector>
#include <list>
#include <memory>
template<typename T> using ptr_vec_t = std::vector<std::unique_ptr<T>>;
template<typename T> using ptr_list_t = std::list<std::unique_ptr<T>>;

#endif /* __SUGAR2__ */

