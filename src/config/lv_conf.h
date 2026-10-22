#ifndef LV_CONF_H
#define LV_CONF_H

#if USE_DESKTOP
#include "lv_conf_desktop.h"
#else
#include "lv_conf_cm0.h"
#endif

#define LV_USE_QRCODE 1

#endif // LV_CONF_H
