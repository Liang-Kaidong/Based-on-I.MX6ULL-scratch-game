// touchscreen_input.h
#ifndef TOUCHSCREEN_INPUT_H
#define TOUCHSCREEN_INPUT_H

#include <sys/types.h>
#include <linux/input.h>

// 全局变量声明
extern int input_x, input_y;

// 获取触摸屏输入的函数声明
void get_touchscreen_input();

#endif