#ifndef WEATHERNUM_H
#define WEATHERNUM_H

#include <TFT_eSPI.h> 

#include "img/tianqi/t0.h"
#include "img/tianqi/t1.h"
#include "img/tianqi/t2.h"
#include "img/tianqi/t3.h"
#include "img/tianqi/t4.h"
#include "img/tianqi/t5.h"
#include "img/tianqi/t6.h"
#include "img/tianqi/t7.h"
#include "img/tianqi/t9.h"
#include "img/tianqi/t11.h"
#include "img/tianqi/t13.h"
#include "img/tianqi/t14.h"
#include "img/tianqi/t15.h"
#include "img/tianqi/t16.h"
#include "img/tianqi/t18.h"
#include "img/tianqi/t19.h"
#include "img/tianqi/t20.h"
#include "img/tianqi/t26.h"
#include "img/tianqi/t29.h"
#include "img/tianqi/t30.h"
#include "img/tianqi/t31.h"
#include "img/tianqi/t53.h"
#include "img/tianqi/t99.h"


class WeatherNum
{
public:
  void draw(int16_t x, int16_t y, int weatherCode);
  void printfweather(int numx, int numy, int numw) { draw(numx, numy, numw); }
};


#endif
