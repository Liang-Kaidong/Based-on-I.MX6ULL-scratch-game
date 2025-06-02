// touchscreen_input.c
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include <linux/input.h>
#include "touchscreen_input.h"

// 全局变量定义
int input_x, input_y;

// 获取触摸屏输入的函数实现
void get_touchscreen_input() {
    /* 打开触摸屏文件 */
    int input_fd = open("/dev/input/event1", O_RDWR);
    struct input_event input_buf;
    while(1) {
        /* 读取触摸屏数据: input_buf */
        read(input_fd, &input_buf, sizeof(input_buf));
        /* 判断是否是触摸屏事件 */
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            /* 获取触摸屏的x坐标事件 */
            input_x = input_buf.value;
        }
        /* 判断是否是触摸屏事件 */
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            /* 获取触摸屏的y坐标事件 */
            input_y = input_buf.value;
        }
        /* 判断是否是触摸屏按下事件 */
        if (input_buf.type == EV_KEY &&
            input_buf.code == BTN_TOUCH &&
            input_buf.value == 0) {
            /* 打印坐标值 */
            printf("x = %d, y = %d\n", input_x, input_y);
            break;
        }
    }
    close(input_fd);
}