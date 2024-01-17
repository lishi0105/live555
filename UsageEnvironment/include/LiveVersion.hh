/*
 * @Description:
 * @Version:
 * @Autor: 李石
 * @Date: 2023-10-19 11:40:32
 * @LastEditors: 李石
 * @LastEditTime: 2023-11-09 09:23:05
 */
#ifndef __LIVE_VERSION_HH__
#define __LIVE_VERSION_HH__

/****************************
v0.0.1.211111
1.
*/

#define LIVE_CLIENT_VERSION_MAJOR 0
#define LIVE_CLIENT_VERSION_MINOR 1

#define LIVE_SERVER_VERSION_MAJOR 0
#define LIVE_SERVER_VERSION_MINOR 1

#define STRINGIFY0(x) #x
#define STRINGIFY(x) STRINGIFY0(x)

#define LIVE_CLIENT_VERSION_INT \
    (LIVE_CLIENT_VERSION_MAJOR * 100 + LIVE_CLIENT_VERSION_MINOR)
#define LIVE_CLIENT_VERSION_STRING       \
    STRINGIFY(LIVE_CLIENT_VERSION_MAJOR) \
    "." STRINGIFY(LIVE_CLIENT_VERSION_MINOR)

#define LIVE_SERVER_VERSION_INT \
    (LIVE_CLIENT_VERSION_MAJOR * 100 + LIVE_CLIENT_VERSION_MINOR)
#define LIVE_SERVER_VERSION_STRING       \
    STRINGIFY(LIVE_CLIENT_VERSION_MAJOR) \
    "." STRINGIFY(LIVE_CLIENT_VERSION_MINOR)

// 使用编译时间作为版本build号
/**
__DATE__ 程序被编译的日期, 以"Mmm dd yyyy"格式的字符串标注.
__TIME__ 程序被编译的时间, 以"hh:mm:ss"格式的字符串标注, 该时间由asctime返回.
 */
#define YEAR                                                  \
    ((((__DATE__[7] - '0') * 10 + (__DATE__[8] - '0')) * 10 + \
      (__DATE__[9] - '0')) *                                  \
         10 +                                                 \
     (__DATE__[10] - '0'))

/**月份简写
January (Jan)---Febuary (Feb)---March (Mar)---April (Apr)---May (May)---June
(Jun)-- July (Jul)---August (Aug)---September (Sep)---October (Oct)---November
(Nov)---SDecember (Dec)--
*/
#define MONTH                                            \
    (__DATE__[2] == 'n'   ? (__DATE__[1] == 'a' ? 1 : 6) \
     : __DATE__[2] == 'b' ? 2                            \
     : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 3 : 4) \
     : __DATE__[2] == 'y' ? 5                            \
     : __DATE__[2] == 'l' ? 7                            \
     : __DATE__[2] == 'g' ? 8                            \
     : __DATE__[2] == 'p' ? 9                            \
     : __DATE__[2] == 't' ? 10                           \
     : __DATE__[2] == 'v' ? 11                           \
                          : 12)

#define DAY \
    ((__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))

#define TIME                                                    \
    ((((((__TIME__[0] - '0') * 10 + (__TIME__[1] - '0')) * 10 + \
        (__TIME__[3] - '0')) *                                  \
           10 +                                                 \
       (__TIME__[4] - '0')) *                                   \
          10 +                                                  \
      (__TIME__[5] - '0')) *                                    \
         10 +                                                   \
     (__TIME__[6] - '0'))

#define LIVE_CLIENT_BUILD_TIME \
    ((YEAR - 2000) * 10000000000 + MONTH * 100000000 + DAY * 1000000 + TIME)
#define LIVE_CLIENT_BUILD_DATE (YEAR * 10000 + MONTH * 100 + DAY)

#define LIVE_SERVER_BUILD_TIME \
    ((YEAR - 2000) * 10000000000 + MONTH * 100000000 + DAY * 1000000 + TIME)
#define LIVE_SERVER_BUILD_DATE (YEAR * 10000 + MONTH * 100 + DAY)

#endif  // !__LIVE_VERSION_HH__