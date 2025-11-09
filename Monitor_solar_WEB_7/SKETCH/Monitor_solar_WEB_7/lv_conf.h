#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_STDLIB_MALLOC    1
#define LV_USE_STDLIB_STRING    1
#define LV_USE_STDLIB_SPRINTF   1

#define LV_MEM_SIZE             (64U * 1024U)
#define LV_MEM_ADR              0

#define LV_USE_USER_DATA        1
#define LV_USE_LOG              0
#define LV_LOG_LEVEL            LV_LOG_LEVEL_NONE

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE      1
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0

#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "esp32-hal-timer.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

#define LV_USE_OBJ_REALIGN      1
#define LV_USE_OBJ_SCROLLABLE   1
#define LV_USE_OBJ_STYLE_CACHE  1

#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_LABEL            1
#define LV_USE_OBJ              1
#define LV_USE_STYLE            1

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_38 1

#define LV_USE_FONT_PLACEHOLDER 1

#define LV_USE_FLEX             1
#define LV_USE_GRID             1

#define LV_USE_LOG              0
#define LV_USE_LOG_TRACE_MEM    0
#define LV_USE_LOG_TRACE_TIMER    0
#define LV_USE_LOG_TRACE_INDEV    0
#define LV_USE_LOG_TRACE_DISP_REFR 0

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE     1

#define LV_USE_OBJ_REALIGN      1
#define LV_USE_OBJ_SCROLLABLE   1
#define LV_USE_OBJ_STYLE_CACHE  1

#define LV_USE_DISP_ROTATE      0
#define LV_USE_DISP_SCALE       0

#define LV_USE_USER_DATA        1

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE     1

#define LV_USE_OBJ_REALIGN      1
#define LV_USE_OBJ_SCROLLABLE   1
#define LV_USE_OBJ_STYLE_CACHE  1

#define LV_USE_DISP_ROTATE      0
#define LV_USE_DISP_SCALE       0

#define LV_USE_USER_DATA        1

#endif