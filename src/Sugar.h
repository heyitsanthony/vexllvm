#ifndef __SUGAR__
#define __SUGAR__

#include <cstring>

// Lame C++ sugar.  I'm pretty sure there better definitions.

#define let(_lhs, _rhs)  typeof(_rhs) _lhs = _rhs

#define foreach(_i, _b, _e) \
	  for(typeof(_b) _i = _b, _i ## end = _e; _i != _i ## end;  ++ _i)


struct ltstr
{
    bool operator()(const char* str1, const char* str2) const
    {
        return std::strcmp(str1, str2) < 0;
    }
};


#endif /* __SUGAR__ */

