// 引入标准输入输出库，用于基本的输入输出操作，如 printf、scanf 等
#include <stdio.h>
// 引入系统类型定义库，包含了一些系统相关的数据类型定义
#include <sys/types.h>
// 引入系统文件状态库，用于文件状态的操作和获取，如文件权限等
#include <sys/stat.h>
// 引入文件控制库，提供了文件操作的函数，如 open 函数
#include <fcntl.h>
// 引入 Unix 标准库，包含了许多 Unix 系统的基本函数，如 read、write 等
#include <unistd.h>
// 引入字符串处理库，用于字符串的操作，如 strlen、strcpy 等
#include <string.h>
// 引入内存映射库，用于将文件或设备映射到内存中
#include <sys/mman.h>
// 引入标准库，包含了一些常用的函数，如 malloc、free 等
#include <stdlib.h>
// 引入布尔类型定义库，允许使用布尔类型（true 和 false）
#include <stdbool.h>
// 引入 Linux 输入事件库，用于处理输入设备的事件，如触摸屏事件
#include <linux/input.h>
// 引入数学库，提供了数学运算的函数，如 sin、cos 等
#include <math.h>
// 引入线程库，用于创建和管理线程

// 引入自定义的 LCD 字体处理头文件，用于在 LCD 屏幕上显示字体
#include "lcd_font.h"

void keyboard() {
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
    }
    lcd_clear(COLOR_BLACK);
    lcd_draw_filled_rectangle(0, 170, 1024, 600, COLOR_WHITE);  

    lcd_render_text("Q", 50, 200, COLOR_BLACK, 50);
    lcd_render_text("W", 150, 200, COLOR_BLACK, 50);
    lcd_render_text("E", 250, 200, COLOR_BLACK, 50);
    lcd_render_text("R", 350, 200, COLOR_BLACK, 50);
    lcd_render_text("T", 450, 200, COLOR_BLACK, 50);
    lcd_render_text("Y", 550, 200, COLOR_BLACK, 50);
    lcd_render_text("U", 650, 200, COLOR_BLACK, 50);
    lcd_render_text("I", 750, 200, COLOR_BLACK, 50);
    lcd_render_text("O", 850, 200, COLOR_BLACK, 50);
    lcd_render_text("P", 950, 200, COLOR_BLACK, 50);

    lcd_render_text("A", 100, 300, COLOR_BLACK, 50);
    lcd_render_text("S", 200, 300, COLOR_BLACK, 50);
    lcd_render_text("D", 300, 300, COLOR_BLACK, 50);
    lcd_render_text("F", 400, 300, COLOR_BLACK, 50);
    lcd_render_text("G", 500, 300, COLOR_BLACK, 50);
    lcd_render_text("H", 600, 300, COLOR_BLACK, 50);
    lcd_render_text("J", 700, 300, COLOR_BLACK, 50);
    lcd_render_text("K", 800, 300, COLOR_BLACK, 50);
    lcd_render_text("L", 900, 300, COLOR_BLACK, 50);

    lcd_render_text("删", 50, 400, COLOR_BLACK, 50);
    lcd_render_text("Z", 150, 400, COLOR_BLACK, 50);
    lcd_render_text("X", 250, 400, COLOR_BLACK, 50);
    lcd_render_text("C", 350, 400, COLOR_BLACK, 50);
    lcd_render_text("V", 450, 400, COLOR_BLACK, 50);
    lcd_render_text("B", 550, 400, COLOR_BLACK, 50);
    lcd_render_text("N", 650, 400, COLOR_BLACK, 50);
    lcd_render_text("M", 750, 400, COLOR_BLACK, 50);
    lcd_render_text("确", 900, 400, COLOR_BLACK, 50);

    lcd_draw_filled_rectangle(200, 500, 630, 100, COLOR_LIGHTGRAY);
    lcd_render_text("除", 50, 500, COLOR_BLACK, 50);
    lcd_render_text("认", 900, 500, COLOR_BLACK, 50);
    lcd_render_text("空格", 450, 525, COLOR_BLACK, 50);
}

int main() {
    keyboard();
    return 0;
}