#pragma once

#define OPCASE(i) case i: os << p##i; break;
#define OPSWITCH1 OPCASE(0)
#define OPSWITCH2 OPSWITCH1 OPCASE(1)
#define OPSWITCH3 OPSWITCH2 OPCASE(2)
#define OPSWITCH4 OPSWITCH3 OPCASE(3)
#define OPSWITCH5 OPSWITCH4 OPCASE(4)
#define OPSWITCH6 OPSWITCH5 OPCASE(5)
#define OPSWITCH7 OPSWITCH6 OPCASE(6)
#define OPSWITCH8 OPSWITCH7 OPCASE(7)
#define OPSWITCH9 OPSWITCH8 OPCASE(8)
#define OPSWITCH10 OPSWITCH9 OPCASE(9)
#define OPSWITCH(c) switch (a) { OPSWITCH##c  }

#define OPCASEW(i) case i: width = _oprintf_width(p##i); break;
#define OPSWITCHW1 OPCASEW(0)
#define OPSWITCHW2 OPSWITCHW1 OPCASEW(1)
#define OPSWITCHW3 OPSWITCHW2 OPCASEW(2)
#define OPSWITCHW4 OPSWITCHW3 OPCASEW(3)
#define OPSWITCHW5 OPSWITCHW4 OPCASEW(4)
#define OPSWITCHW6 OPSWITCHW5 OPCASEW(5)
#define OPSWITCHW7 OPSWITCHW6 OPCASEW(6)
#define OPSWITCHW8 OPSWITCHW7 OPCASEW(7)
#define OPSWITCHW9 OPSWITCHW8 OPCASEW(8)
#define OPSWITCHW10 OPSWITCHW9 OPCASEW(9)
#define OPWIDTH(c) switch(a) { OPSWITCHW##c }

#define OPrintf(argc)                                               \
    const char * p = fmt, *q = fmt;                                 \
    for (unsigned a = 0;a < argc;++q) {                             \
        switch (*q) {                                               \
            case 0:                                                 \
                os.write(p,q-p);                                    \
                return os;                                          \
                                                                    \
            case '%':                                               \
            {                                                       \
                if (*(q+1) == '%') {                                \
                    os.write(p,q-p);                                \
                    p = ++q; continue;                              \
                }                                                   \
                os.write(p,q-p);                                    \
                                                                    \
                int width = 0;                                      \
                int precision = 6;                                  \
                std::ios_base::fmtflags flags = os.flags();         \
                char fill = ' ';                                    \
                bool alternate = false;                             \
                                                                    \
                for (;;) {                                          \
                    switch (*++q) {                                 \
                        case '+':                                   \
                            flags |= std::ios::showpos;             \
                            continue;                               \
                        case '-':                                   \
                            flags |= std::ios::left;                \
                            continue;                               \
                        case '0':                                   \
                            flags |= std::ios::internal;            \
                            fill = '0';                             \
                            continue;                               \
                        case '#':                                   \
                            alternate = true;                       \
                            continue;                               \
                        case '*':                                   \
                            OPWIDTH(argc);                          \
                            if (++a == argc) return os;             \
                            continue;                               \
                        default:                                    \
                            break;                                  \
                    }                                               \
                    break;                                          \
                }                                                   \
                                                                    \
                if (*q >= '0' && *q <= '9') {                       \
                    width = atoi(q);                                \
                    for (;*q >= '0' && *q <= '9';++q){}             \
                }                                                   \
                                                                    \
                if (*q == '.') {                                    \
                    precision = atoi(++q);                          \
                    for (;*q >= '0' && *q <= '9';++q){}             \
                }                                                   \
                                                                    \
                switch (*q) {                                       \
                    case 'p':                                       \
                    case 's':                                       \
                    case 'c':                                       \
                    case 'C':                                       \
                        break;                                      \
                                                                    \
                    case 'i':                                       \
                    case 'u':                                       \
                    case 'd':                                       \
                        flags |= std::ios::dec;                     \
                        break;                                      \
                                                                    \
                    case 'f':                                       \
                        flags |= std::ios::fixed;                   \
                        if (alternate) flags |= std::ios::showpoint;\
                        break;                                      \
                                                                    \
                    case 'E':                                       \
                        flags |= std::ios::uppercase;               \
                    case 'e':                                       \
                        flags |= std::ios::scientific;              \
                        if (alternate) flags |= std::ios::showpoint;\
                        break;                                      \
                                                                    \
                    case 'X':                                       \
                        flags |= std::ios::uppercase;               \
                    case 'x':                                       \
                    case 'o':                                       \
                        flags |= std::ios::hex;                     \
                        if (alternate) flags |= std::ios::showbase; \
                        break;                                      \
                                                                    \
                    case 'G':                                       \
                        flags |= std::ios::uppercase;               \
                    case 'g':                                       \
                        if (alternate) flags |= std::ios::showpoint;\
                        break;                                      \
                                                                    \
                    default:                                        \
                        OPSWITCH(argc)                              \
                        ++a; p = q+1;                               \
                        continue;                                   \
                }                                                   \
                                                                    \
                os.unsetf(std::ios::adjustfield                     \
                    | std::ios::basefield | std::ios::floatfield);  \
                os.setf(flags);                                     \
                os.width(width);                                    \
                os.precision(precision);                            \
                os.fill(fill);                                      \
                                                                    \
                OPSWITCH(argc)                                      \
                ++a; p = q+1;                                       \
                break;                                              \
            }                                                       \
                                                                    \
            default: break;                                         \
        }                                                           \
    }                                                               \
                                                                    \
    os << p;                                                        \
    return os;

template<typename C>
inline int _oprintf_width(const C&) { return 0; }
template<>
inline int _oprintf_width<int>(const int& param) { return param; }
template<>
inline int _oprintf_width<unsigned>(const unsigned& param) { return (int)param; }
template<>
inline int _oprintf_width<double>(const double& param) { return (int)param; }

template<typename T, typename P0>
T& oprintf(T& os, const char * fmt, const P0& p0) 
{ OPrintf(1); } 

template<typename T, typename P0, typename P1>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1) 
{ OPrintf(2); }

template<typename T, typename P0, typename P1, typename P2>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2) 
{ OPrintf(3); }

template<typename T, typename P0, typename P1, typename P2, typename P3>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3) 
{ OPrintf(4); }

template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3, const P4& p4) 
{ OPrintf(5); }

template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5) 
{ OPrintf(6); }

template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6) 
{ OPrintf(7); }

template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6, const P7& p7) 
{ OPrintf(8); }

template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6, const P7& p7, const P8& p8) 
{ OPrintf(9); }

template<typename T, typename P0, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
T& oprintf(T& os, const char * fmt, const P0& p0, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6, const P7& p7, const P8& p8, const P9& p9) 
{ OPrintf(10); }
