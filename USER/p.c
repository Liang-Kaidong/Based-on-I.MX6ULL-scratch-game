/*************************************************************************************************************
File name: p.c
Author: KD
Version: V_2.3
Build date: 2024-06-30
Description: NONE
Others: Usage requires preservation of original author attribution.
Log: 1.新增首次登录页面密码显示隐藏开关
     2.移除实体键盘输入账号与密码的功能
     3.修复部分输入的逻辑错误
     4.修复登录账号的已知逻辑错误
     5.修复因未初始化字库导致屏幕提示异常
     6.修复密码显示隐藏开关，正确显示隐藏密码
bug: 1.账号登陆与注册判断逻辑部分缺乏屏幕提示
     2.账号注册页面暂时未实现账号注册，在下一个版本更进更新
*************************************************************************************************************/

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
#include <math.h>
#include <pthread.h>
#include "show_bmp_to_lcd.h"
#include "lcd_font.h"

/* 全局变量定义 */
int input_x, input_y;   /* 触摸点 x 和 y 坐标 */
int stop_show_or_hide_password_while = 0; /* 终止隐藏密码的持续循环 标记位 1：成功 */
bool confirm_clicked = false;  /* 确认键是否被点击的标志 */

// 定义包含账号和密码数组的结构体
typedef struct 
{
    char account_number_buf[128];       /* 用于存储账号的数组 */
    char password_number_buf[128];      /* 用于存储密码的数组 */
    char hide_password_number_buf[128]; /* 用于隐藏密码的数组 */
} UserInfo;
UserInfo user_info = {{0}, {0}}; // 初始化用户信息结构体 

/* 全局向前声明各函数 */ 
void input_account_box();
void input_password_box();
void register_account_box();
void register_password_box();
void login_fun();
void login_boot();
void login_judgment();
void register_judgment();
void game_start_home();

void ts_fun()
{   
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

/* 未点击到游戏文本提示 */
void not_open_game_notification() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf")!= 0) {
        printf("初始化失败。\n");
        return;
    } 

    /* 测试圆角文本框（不同半径） */ 
    lcd_render_text_with_box(
        "未点击到游戏，请重试！",   /* 文本内容 */
        350, 400,                 /* 起始坐标 (x, y) */
        COLOR_WHITE,              /* 文本颜色 */
        COLOR_LIGHTGRAY,          /* 文本框背景颜色 */
        10,                       /* 文本与文本框边缘的间距 */
        BOX_STYLE_ROUNDED,        /* 圆角样式 */
        15,                       /* 圆角半径 */
        30,                       /* 字体大小 */
        0,                        /* 文本框宽度，为 0 时，文本框大小依照文字大小与文本量大小调整，文字居中对齐 */
        0                         /* 文本框高度，为 0 时，文本框大小依照文字大小与文本量大小调整，文字居中对齐 */
    );

    /* 清理资源 */ 
    lcd_cleanup();
}

/* 调用虚拟键盘 */
void keyboard() 
{
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }

    lcd_draw_filled_rectangle(0, 130, 1024, 600, COLOR_WHITE);  

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

    lcd_draw_filled_rounded_rectangle(200, 480, 630, 100, 15, COLOR_LIGHTGRAY);
    lcd_render_text("除", 50, 500, COLOR_BLACK, 50);
    lcd_render_text("认", 900, 500, COLOR_BLACK, 50);
    lcd_render_text("空格", 450, 505, COLOR_WHITE, 50);
}

/* 账号与密码提示文本渲染 */
void account_password_background_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf")!= 0) {
        printf("初始化失败。\n");
        return;
    }

    /* 绘制账号与密码背景文本框 */
    /* 账号背景文本框 */
    lcd_draw_filled_rectangle(
        104, 248,       /* 左上角坐标 (x, y) */
        386, 50,        /* 矩形宽度和高度 */
        COLOR_WHITE     /* 填充颜色 */
    );
    lcd_render_text(
        "请输入账号",                       /* 文本内容 */
        104, 260,                           /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,                    /* 文本颜色 */
        25                                  /* 字体大小 */
    );
    /* 密码背景文本框 */
    lcd_draw_filled_rectangle(
        104, 310,       /* 左上角坐标 (x, y) */
        386, 50,        /* 矩形宽度和高度 */
        COLOR_WHITE     /* 填充颜色 */
    );
    lcd_render_text(
        "请输入密码",            /* 文本内容 */
        104, 323,                           /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,                    /* 文本颜色 */
        25                                  /* 字体大小 */
    );

    /* 清理资源 */
    lcd_cleanup(); 
}

/* 注册界面渲染 */
void register_background_box()
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf")!= 0) {
        printf("初始化失败。\n");
        return;
    }

    /* 绘制账号与密码背景文本框 */
    /* 账号背景文本框 */
    lcd_draw_filled_rectangle(
        104, 248,       /* 左上角坐标 (x, y) */
        386, 50,        /* 矩形宽度和高度 */
        COLOR_WHITE     /* 填充颜色 */
    );
    lcd_render_text(
        "请输入要注册的账号",             /* 文本内容 */
        104, 260,                           /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,                    /* 文本颜色 */
        25                                  /* 字体大小 */
    );
    /* 密码背景文本框 */
    lcd_draw_filled_rectangle(
        104, 310,       /* 左上角坐标 (x, y) */
        386, 50,        /* 矩形宽度和高度 */
        COLOR_WHITE     /* 填充颜色 */
    );
    lcd_render_text(
        "请输入不少于8位数的密码",            /* 文本内容 */
        104, 323,                           /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,                    /* 文本颜色 */
        25                                  /* 字体大小 */
    );

    /* 清理资源 */
    lcd_cleanup();    
}

/* 账号输入界面刷新线程函数 */
void* account_input_refresh(void* arg) 
{
    while (!confirm_clicked) {
        /* 初始化字库 */
        if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
            printf("初始化失败。\n");
            continue;
        }

        /* 置顶账号输入框，以解决闪烁问题 */
        show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
        /* 绘制账号输入文本框背景 */
        lcd_draw_filled_rectangle(
            0, 0,               /* 左上角坐标 (x, y) */
            1024, 143,          /* 矩形宽度和高度 */
            COLOR_WHITE         /* 填充颜色 */ 
        );
        /* 绘制账号输入文本框 */
        lcd_render_text_with_box(
            user_info.account_number_buf,      /* 文本内容 */
            70, 51,                /* 起始坐标 (x, y) */
            COLOR_BLACK,             /* 文本颜色 */ 
            COLOR_WHITE,             /* 文本框背景颜色 */ 
            0,                       /* 文本与文本框边缘的间距 */ 
            BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
            0,                       /* 矩形样式不需要半径 */
            60,                      /* 字体大小 */
            0,                     /* 文本框宽度 */
            0                       /* 文本框高度 */
        );

        /* 绘制确认按钮 */
        lcd_render_text_with_box(
            "确认",      /* 文本内容 */
            800, 51,                /* 起始坐标 (x, y) */
            COLOR_WHITE,             /* 文本颜色 */ 
            COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
            0,                       /* 文本与文本框边缘的间距 */ 
            BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
            15,                       /* 矩形样式不需要半径 */
            60,                      /* 字体大小 */
            0,                       /* 文本框宽度 */
            0                        /* 文本框高度 */
        );

        lcd_cleanup();
        usleep(100000); // 每100ms刷新一次
    }
    return NULL;
}

/* 密码输入界面刷新线程函数 */
void* password_input_refresh(void* arg) 
{
    while (!confirm_clicked) {
        /* 初始化字库 */
        if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
            printf("初始化失败。\n");
            continue;
        }

        /* 置顶密码输入框，以解决闪烁问题 */
        show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
        /* 绘制密码输入文本框背景 */
        lcd_draw_filled_rectangle(
            0, 0,               /* 左上角坐标 (x, y) */
            1024, 143,          /* 矩形宽度和高度 */
            COLOR_WHITE         /* 填充颜色 */ 
        );
        /* 绘制密码输入文本框 */
        lcd_render_text_with_box(
            user_info.password_number_buf,      /* 文本内容 */
            70, 51,                /* 起始坐标 (x, y) */
            COLOR_BLACK,             /* 文本颜色 */ 
            COLOR_WHITE,             /* 文本框背景颜色 */ 
            0,                       /* 文本与文本框边缘的间距 */ 
            BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
            0,                       /* 矩形样式不需要半径 */
            60,                      /* 字体大小 */
            0,                     /* 文本框宽度 */
            0                       /* 文本框高度 */
        );

        /* 绘制确认按钮 */
        lcd_render_text_with_box(
            "确认",      /* 文本内容 */
            800, 51,                /* 起始坐标 (x, y) */
            COLOR_WHITE,             /* 文本颜色 */ 
            COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
            0,                       /* 文本与文本框边缘的间距 */ 
            BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
            15,                       /* 矩形样式不需要半径 */
            60,                      /* 字体大小 */
            0,                       /* 文本框宽度 */
            0                        /* 文本框高度 */
        );

        lcd_cleanup();
        usleep(100000); // 每100ms刷新一次
    }
    return NULL;
}

/* 账号输入实现 */
void input_account_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }

    /* 置顶账号输入框，以解决闪烁问题 */
    show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
    /* 绘制账号输入文本框背景 */
    lcd_draw_filled_rectangle(
        0, 0,               /* 左上角坐标 (x, y) */
        1024, 143,          /* 矩形宽度和高度 */
        COLOR_WHITE         /* 填充颜色 */ 
    );
    /* 绘制账号输入文本框 */
    lcd_render_text_with_box(
        user_info.account_number_buf,      /* 文本内容 */
        70, 51,                /* 起始坐标 (x, y) */
        COLOR_BLACK,             /* 文本颜色 */ 
        COLOR_WHITE,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
        0,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                     /* 文本框宽度 */
        0                       /* 文本框高度 */
    );

    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",      /* 文本内容 */
        800, 51,                /* 起始坐标 (x, y) */
        COLOR_WHITE,             /* 文本颜色 */ 
        COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
        15,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                       /* 文本框宽度 */
        0                        /* 文本框高度 */
    );
    keyboard();    /* 加载键盘 */

    // 打开触摸屏文件
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;

    while (1) {
        int input_changed = 0;  // 标记输入是否有变化

        // 读取触摸屏数据
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            // 判断是否点击了确认按钮
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  /* 确认按钮和确认键 */
                /* 处理确认按钮点击事件 */ 
                show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 账号背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 248,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入账号",                       /* 文本内容 */
                        104, 260,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );            
                } else {
                    /* 账号输入文本框 */ 
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */ 
                        COLOR_WHITE,             /* 文本框背景颜色 */ 
                        0,                       /* 文本与文本框边缘的间距 */ 
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                     /* 文本框宽度 */
                        50                       /* 文本框高度 */
                    );  
                }

                if (strlen(user_info.password_number_buf) == 0) {
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入密码",                       /* 文本内容 */
                        104, 323,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );
                } else {
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.password_number_buf,      /* 文本内容 */
                        104, 310,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */  
                        COLOR_WHITE,             /* 文本框背景颜色 */
                        0,                       /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                       /* 文本框宽度 */
                        50                        /* 文本框高度 */
                    );
                }
                break;  //debug
            }
            // 处理键盘点击事件
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { //空格键
                strcat(user_info.account_number_buf, " ");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { //Q键
                strcat(user_info.account_number_buf, "Q");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { //W键
                strcat(user_info.account_number_buf, "W");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { //E键
                strcat(user_info.account_number_buf, "E");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { //R键
                strcat(user_info.account_number_buf, "R");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { //T键
                strcat(user_info.account_number_buf, "T");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { //Y键
                strcat(user_info.account_number_buf, "Y");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { //U键
                strcat(user_info.account_number_buf, "U");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { //I键
                strcat(user_info.account_number_buf, "I");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { //O键
                strcat(user_info.account_number_buf, "O");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { //P键
                strcat(user_info.account_number_buf, "P");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { //A键
                strcat(user_info.account_number_buf, "A");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { //S键
                strcat(user_info.account_number_buf, "S");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { //D键
                strcat(user_info.account_number_buf, "D");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { //F键
                strcat(user_info.account_number_buf, "F");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                strcat(user_info.account_number_buf, "G");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { //H键
                strcat(user_info.account_number_buf, "H");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { //J键
                strcat(user_info.account_number_buf, "J");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { //K键
                strcat(user_info.account_number_buf, "K");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { //L键
                strcat(user_info.account_number_buf, "L");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { //Z键
                strcat(user_info.account_number_buf, "Z");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { //X键
                strcat(user_info.account_number_buf, "X");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { //C键
                strcat(user_info.account_number_buf, "C");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { //V键
                strcat(user_info.account_number_buf, "V");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { //B键
                strcat(user_info.account_number_buf, "B");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { //N键
                strcat(user_info.account_number_buf, "N");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { //M键
                strcat(user_info.account_number_buf, "M");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { //删除键
                if (strlen(user_info.account_number_buf) > 0) {
                    user_info.account_number_buf[strlen(user_info.account_number_buf) - 1] = '\0';
                }
                input_changed = 1;  // 标记输入有变化
            }

            if (input_changed) {
                // 绘制账号输入文本框背景
                lcd_draw_filled_rectangle(
                    0, 0,               /* 左上角坐标 (x, y) */
                    1024, 143,          /* 矩形宽度和高度 */
                    COLOR_WHITE         /* 填充颜色 */ 
                );
                /* 账号输入文本框 */ 
                lcd_render_text_with_box(
                    user_info.account_number_buf,      /* 文本内容 */
                    70, 51,                /* 起始坐标 (x, y) */
                    COLOR_BLACK,             /* 文本颜色 */ 
                    COLOR_WHITE,             /* 文本框背景颜色 */ 
                    0,                       /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                    0,                       /* 矩形样式不需要半径 */
                    60,                      /* 字体大小 */
                    0,                     /* 文本框宽度 */
                    0                       /* 文本框高度 */
                );        
                /* 确认按钮 */
                lcd_render_text_with_box(
                    "确认",      /* 文本内容 */
                    800, 51,                /* 起始坐标 (x, y) */
                    COLOR_WHITE,             /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
                    0,                       /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
                    15,                       /* 矩形样式不需要半径 */
                    60,                      /* 字体大小 */
                    0,                       /* 文本框宽度 */
                    0                        /* 文本框高度 */
                );
            }
        }
    }

    /* 返回时，可自由选择账号或密码输入 */
    while (2) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            input_account_box();
            break;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 310 && input_y <= 400) {
            input_password_box();
            break;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
            login_judgment();
            break;
        }
    }
    close(input_fd);
    lcd_cleanup();
}

void input_password_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }

    /* 置顶密码输入框，以解决闪烁问题 */
    show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
    /* 绘制密码输入文本框背景 */
    lcd_draw_filled_rectangle(
        0, 0,               /* 左上角坐标 (x, y) */
        1024, 143,          /* 矩形宽度和高度 */
        COLOR_WHITE         /* 填充颜色 */ 
    );
    /* 绘制密码输入文本框 */
    lcd_render_text_with_box(
        user_info.password_number_buf,      /* 文本内容 */
        70, 51,                /* 起始坐标 (x, y) */
        COLOR_BLACK,             /* 文本颜色 */ 
        COLOR_WHITE,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
        0,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                     /* 文本框宽度 */
        0                       /* 文本框高度 */
    );

    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",      /* 文本内容 */
        800, 51,                /* 起始坐标 (x, y) */
        COLOR_WHITE,             /* 文本颜色 */ 
        COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
        15,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                       /* 文本框宽度 */
        0                        /* 文本框高度 */
    );
    keyboard();    /* 加载键盘 */

    // 打开触摸屏文件
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;

    while (1) {
        int input_changed = 0;  // 标记输入是否有变化

        // 读取触摸屏数据
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            // 判断是否点击了确认按钮
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  /* 确认按钮和确认键 */
                /* 处理确认按钮点击事件 */ 
                show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 账号背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 248,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入账号",                       /* 文本内容 */
                        104, 260,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );            
                } else {
                    /* 账号输入文本框 */ 
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */ 
                        COLOR_WHITE,             /* 文本框背景颜色 */ 
                        0,                       /* 文本与文本框边缘的间距 */ 
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                     /* 文本框宽度 */
                        50                       /* 文本框高度 */
                    );  
                }

                /* 显示或隐藏密码开关 */
                int show_hide_password_flags = 0;
                /* 密码背景文本框 */
                lcd_draw_filled_rectangle(
                    104, 310,       /* 左上角坐标 (x, y) */
                    386, 50,        /* 矩形宽度和高度 */
                    COLOR_WHITE     /* 填充颜色 */
                );

                /* 空密码时 */
                if (strlen(user_info.password_number_buf) == 0) {
                    lcd_render_text(
                        "请输入密码",            /* 文本内容 */
                        104, 323,               /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,        /* 文本颜色 */
                        25                      /* 字体大小 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (2) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换标志变量 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                lcd_draw_filled_rectangle(
                                    104, 310,       /* 左上角坐标 (x, y) */
                                    386, 50,        /* 矩形宽度和高度 */
                                    COLOR_WHITE     /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入密码",            /* 文本内容 */
                                    104, 323,               /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,        /* 文本颜色 */
                                    25                      /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                lcd_draw_filled_rectangle(
                                    104, 310,       /* 左上角坐标 (x, y) */
                                    386, 50,        /* 矩形宽度和高度 */
                                    COLOR_WHITE     /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入密码",            /* 文本内容 */
                                    104, 323,               /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,        /* 文本颜色 */
                                    25                      /* 字体大小 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            input_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            input_password_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            login_judgment();
                            if (stop_show_or_hide_password_while == 1) {
                                return;  /* 强行退出函数，避免死循环 */
                            }
                        }
                    }
                    break;
                }

                /* 非空密码时 */
                if (strlen(user_info.password_number_buf) != 0) {
                    lcd_draw_filled_rectangle(
                        104, 310,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf,      /* 文本内容 */
                        104, 310,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */  
                        COLOR_WHITE,             /* 文本框背景颜色 */
                        0,                       /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                       /* 文本框宽度 */
                        50                        /* 文本框高度 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换标志变量 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,       /* 左上角坐标 (x, y) */
                                    386, 50,        /* 矩形宽度和高度 */
                                    COLOR_WHITE     /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,      /* 文本内容 */
                                    104, 310,                /* 起始坐标 (x, y) */
                                    COLOR_BLACK,             /* 文本颜色 */  
                                    COLOR_WHITE,             /* 文本框背景颜色 */
                                    0,                       /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                                    0,                       /* 矩形样式不需要半径 */
                                    50,                      /* 字体大小 */
                                    386,                       /* 文本框宽度 */
                                    50                        /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,       /* 左上角坐标 (x, y) */
                                    386, 50,        /* 矩形宽度和高度 */
                                    COLOR_WHITE     /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf,      /* 文本内容 */
                                    104, 310,                /* 起始坐标 (x, y) */
                                    COLOR_BLACK,             /* 文本颜色 */  
                                    COLOR_WHITE,             /* 文本框背景颜色 */
                                    0,                       /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                                    0,                       /* 矩形样式不需要半径 */
                                    50,                      /* 字体大小 */
                                    386,                       /* 文本框宽度 */
                                    50                        /* 文本框高度 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            input_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            input_password_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            login_judgment();
                            if (stop_show_or_hide_password_while == 1) {
                                return;  /* 强行退出函数，避免死循环 */ 
                            }
                        }
                    }
                }
                break;
            }
            // 处理键盘点击事件
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { //空格键
                strcat(user_info.password_number_buf, " ");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { //Q键
                strcat(user_info.password_number_buf, "Q");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { //W键
                strcat(user_info.password_number_buf, "W");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { //E键
                strcat(user_info.password_number_buf, "E");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { //R键
                strcat(user_info.password_number_buf, "R");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { //T键
                strcat(user_info.password_number_buf, "T");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { //Y键
                strcat(user_info.password_number_buf, "Y");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { //U键
                strcat(user_info.password_number_buf, "U");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { //I键
                strcat(user_info.password_number_buf, "I");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { //O键
                strcat(user_info.password_number_buf, "O");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { //P键
                strcat(user_info.password_number_buf, "P");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { //A键
                strcat(user_info.password_number_buf, "A");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { //S键
                strcat(user_info.password_number_buf, "S");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { //D键
                strcat(user_info.password_number_buf, "D");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { //F键
                strcat(user_info.password_number_buf, "F");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                strcat(user_info.password_number_buf, "G");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { //H键
                strcat(user_info.password_number_buf, "H");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { //J键
                strcat(user_info.password_number_buf, "J");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { //K键
                strcat(user_info.password_number_buf, "K");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { //L键
                strcat(user_info.password_number_buf, "L");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { //Z键
                strcat(user_info.password_number_buf, "Z");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { //X键
                strcat(user_info.password_number_buf, "X");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { //C键
                strcat(user_info.password_number_buf, "C");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { //V键
                strcat(user_info.password_number_buf, "V");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { //B键
                strcat(user_info.password_number_buf, "B");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { //N键
                strcat(user_info.password_number_buf, "N");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { //M键
                strcat(user_info.password_number_buf, "M");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { //删除键
                if (strlen(user_info.password_number_buf) > 0) {
                    user_info.password_number_buf[strlen(user_info.password_number_buf) - 1] = '\0';
                }
                if (strlen(user_info.hide_password_number_buf) > 0) {
                    user_info.hide_password_number_buf[strlen(user_info.hide_password_number_buf) - 1] = '\0';
                }
                input_changed = 1;  // 标记输入有变化
            }

            if (input_changed) {
                // 绘制密码输入文本框背景
                lcd_draw_filled_rectangle(
                    0, 0,               /* 左上角坐标 (x, y) */
                    1024, 143,          /* 矩形宽度和高度 */
                    COLOR_WHITE         /* 填充颜色 */ 
                );
                /* 密码输入文本框 */ 
                lcd_render_text_with_box(
                    user_info.password_number_buf,      /* 文本内容 */
                    70, 51,                /* 起始坐标 (x, y) */
                    COLOR_BLACK,             /* 文本颜色 */ 
                    COLOR_WHITE,             /* 文本框背景颜色 */ 
                    0,                       /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                    0,                       /* 矩形样式不需要半径 */
                    60,                      /* 字体大小 */
                    0,                     /* 文本框宽度 */
                    0                       /* 文本框高度 */
                );        
                /* 确认按钮 */
                lcd_render_text_with_box(
                    "确认",                 /* 文本内容 */
                    800, 51,                /* 起始坐标 (x, y) */
                    COLOR_WHITE,            /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,        /* 文本框背景颜色 */ 
                    0,                      /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,      /* 矩形样式 */ 
                    15,                     /* 矩形样式不需要半径 */
                    60,                     /* 字体大小 */
                    0,                      /* 文本框宽度 */
                    0                       /* 文本框高度 */
                );
            }
        }
    }

    /* 返回时，可自由选择账号或密码输入 */
    while (2) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            input_account_box();
            break;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 310 && input_y <= 400) {
            input_password_box();
            break;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
            login_judgment();
            break;
        }
    }

    close(input_fd);
    lcd_cleanup();
}

/* 注册账号实现 */
void register_account_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }

    /* 置顶账号输入框，以解决闪烁问题 */
    show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
    /* 绘制账号输入文本框背景 */
    lcd_draw_filled_rectangle(
        0, 0,               /* 左上角坐标 (x, y) */
        1024, 143,          /* 矩形宽度和高度 */
        COLOR_WHITE         /* 填充颜色 */ 
    );
    /* 绘制账号输入文本框 */
    lcd_render_text_with_box(
        user_info.account_number_buf,      /* 文本内容 */
        70, 51,                /* 起始坐标 (x, y) */
        COLOR_BLACK,             /* 文本颜色 */ 
        COLOR_WHITE,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
        0,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                     /* 文本框宽度 */
        0                       /* 文本框高度 */
    );

    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",      /* 文本内容 */
        800, 51,                /* 起始坐标 (x, y) */
        COLOR_WHITE,             /* 文本颜色 */ 
        COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
        15,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                       /* 文本框宽度 */
        0                        /* 文本框高度 */
    );
    keyboard();    /* 加载键盘 */

    // 打开触摸屏文件
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;

    while (1) {
        int input_changed = 0;  // 标记输入是否有变化

        // 读取触摸屏数据
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            // 判断是否点击了确认按钮
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  /* 确认按钮和确认键 */
                /* 处理确认按钮点击事件 */ 
                show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 账号背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 248,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入要注册的账号",                       /* 文本内容 */
                        104, 260,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );            
                } else {
                    /* 账号输入文本框 */ 
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */ 
                        COLOR_WHITE,             /* 文本框背景颜色 */ 
                        0,                       /* 文本与文本框边缘的间距 */ 
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                     /* 文本框宽度 */
                        50                       /* 文本框高度 */
                    );  
                }

                if (strlen(user_info.password_number_buf) == 0) {
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入不少于8位数的密码",                       /* 文本内容 */
                        104, 323,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );
                } else {
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.password_number_buf,      /* 文本内容 */
                        104, 310,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */  
                        COLOR_WHITE,             /* 文本框背景颜色 */
                        0,                       /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                       /* 文本框宽度 */
                        50                        /* 文本框高度 */
                    );
                }
                break;  //debug
            }
            ts_fun();   //debug
            // 处理键盘点击事件
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { //空格键
                strcat(user_info.account_number_buf, " ");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { //Q键
                strcat(user_info.account_number_buf, "Q");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { //W键
                strcat(user_info.account_number_buf, "W");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { //E键
                strcat(user_info.account_number_buf, "E");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { //R键
                strcat(user_info.account_number_buf, "R");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { //T键
                strcat(user_info.account_number_buf, "T");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { //Y键
                strcat(user_info.account_number_buf, "Y");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { //U键
                strcat(user_info.account_number_buf, "U");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { //I键
                strcat(user_info.account_number_buf, "I");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { //O键
                strcat(user_info.account_number_buf, "O");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { //P键
                strcat(user_info.account_number_buf, "P");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { //A键
                strcat(user_info.account_number_buf, "A");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { //S键
                strcat(user_info.account_number_buf, "S");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { //D键
                strcat(user_info.account_number_buf, "D");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { //F键
                strcat(user_info.account_number_buf, "F");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                strcat(user_info.account_number_buf, "G");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { //H键
                strcat(user_info.account_number_buf, "H");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { //J键
                strcat(user_info.account_number_buf, "J");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { //K键
                strcat(user_info.account_number_buf, "K");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { //L键
                strcat(user_info.account_number_buf, "L");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { //Z键
                strcat(user_info.account_number_buf, "Z");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { //X键
                strcat(user_info.account_number_buf, "X");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { //C键
                strcat(user_info.account_number_buf, "C");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { //V键
                strcat(user_info.account_number_buf, "V");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { //B键
                strcat(user_info.account_number_buf, "B");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { //N键
                strcat(user_info.account_number_buf, "N");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { //M键
                strcat(user_info.account_number_buf, "M");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { //删除键
                if (strlen(user_info.account_number_buf) > 0) {
                    user_info.account_number_buf[strlen(user_info.account_number_buf) - 1] = '\0';
                    input_changed = 1;  // 标记输入有变化
                }
            }
        }

        // 只有当输入有变化时才更新文本框显示
        if (input_changed) {
            // 绘制账号输入文本框背景
            lcd_draw_filled_rectangle(
                0, 0,               /* 左上角坐标 (x, y) */
                1024, 143,          /* 矩形宽度和高度 */
                COLOR_WHITE         /* 填充颜色 */ 
            );
            /* 账号输入文本框 */ 
            lcd_render_text_with_box(
                user_info.account_number_buf,      /* 文本内容 */
                70, 51,                /* 起始坐标 (x, y) */
                COLOR_BLACK,             /* 文本颜色 */ 
                COLOR_WHITE,             /* 文本框背景颜色 */ 
                0,                       /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                0,                       /* 矩形样式不需要半径 */
                60,                      /* 字体大小 */
                0,                     /* 文本框宽度 */
                0                       /* 文本框高度 */
            );        
            /* 确认按钮 */
            lcd_render_text_with_box(
                "确认",      /* 文本内容 */
                800, 51,                /* 起始坐标 (x, y) */
                COLOR_WHITE,             /* 文本颜色 */ 
                COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
                0,                       /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
                15,                       /* 矩形样式不需要半径 */
                60,                      /* 字体大小 */
                0,                       /* 文本框宽度 */
                0                        /* 文本框高度 */
            );
            //keyboard();    /* 加载键盘 */
        }
    }

    while (2) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            register_account_box();
            break;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 310 && input_y <= 400) {
            register_password_box();
            break;
        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
            register_judgment();
            break;
        }
    }

    // 关闭触摸屏文件
    close(input_fd);

    // 清理资源
    lcd_cleanup();
}    

/* 注册账号密码实现 */
void register_password_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }
    show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
    // 绘制账号输入文本框背景
    lcd_draw_filled_rectangle(
        0, 0,               /* 左上角坐标 (x, y) */
        1024, 143,          /* 矩形宽度和高度 */
        COLOR_WHITE         /* 填充颜色 */ 
    );
    /* 绘制密码输入文本框 */
    lcd_render_text_with_box(
        user_info.password_number_buf,      /* 文本内容 */
        70, 51,                /* 起始坐标 (x, y) */
        COLOR_BLACK,             /* 文本颜色 */ 
        COLOR_WHITE,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
        0,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                     /* 文本框宽度 */
        0                       /* 文本框高度 */
    );

    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",      /* 文本内容 */
        800, 51,                /* 起始坐标 (x, y) */
        COLOR_WHITE,             /* 文本颜色 */ 
        COLOR_LIGHTGRAY,             /* 文本框背景颜色 */ 
        0,                       /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,     /* 矩形样式 */ 
        15,                       /* 矩形样式不需要半径 */
        60,                      /* 字体大小 */
        0,                       /* 文本框宽度 */
        0                        /* 文本框高度 */
    );
    keyboard();    /* 加载键盘 */

    // 打开触摸屏文件
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;

    while (1) {
        int input_changed = 0;  // 标记输入是否有变化

        // 读取触摸屏数据
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            // 判断是否点击了确认按钮
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) ||
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {
                // 处理确认按钮点击事件
                show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);    //测试debug
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 账号输入文本框 */
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */
                        COLOR_WHITE,             /* 文本框背景颜色 */   
                        0,                       /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                     /* 文本框宽度 */
                        50                       /* 文本框高度 */
                    );
                    lcd_render_text(
                        "请输入要注册的账号",                       /* 文本内容 */
                        104, 260,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );
                } else {
                    /* 账号输入文本框 */
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */
                        COLOR_WHITE,             /* 文本框背景颜色 */   
                        0,                       /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                     /* 文本框宽度 */
                        50                       /* 文本框高度 */
                    );
                }

                if (strlen(user_info.password_number_buf) == 0) {
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入不少于8位数的密码",            /* 文本内容 */
                        104, 323,                           /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                    /* 文本颜色 */
                        25                                  /* 字体大小 */
                    );
                } else {
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.password_number_buf,      /* 文本内容 */
                        104, 310,                /* 起始坐标 (x, y) */
                        COLOR_BLACK,             /* 文本颜色 */  
                        COLOR_WHITE,             /* 文本框背景颜色 */
                        0,                       /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,     /* 矩形样式 */
                        0,                       /* 矩形样式不需要半径 */
                        50,                      /* 字体大小 */
                        386,                       /* 文本框宽度 */
                        50                        /* 文本框高度 */
                    );
                }
                break;  /* 不再刷新，结束死循环 */
            }
            ts_fun();   //debug
            // 处理键盘点击事件
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { //空格键
                strcat(user_info.password_number_buf, " ");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { //Q键
                strcat(user_info.password_number_buf, "Q");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { //W键
                strcat(user_info.password_number_buf, "W");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { //E键
                strcat(user_info.password_number_buf, "E");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { //R键
                strcat(user_info.password_number_buf, "R");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { //T键
                strcat(user_info.password_number_buf, "T");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { //Y键
                strcat(user_info.password_number_buf, "Y");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { //U键
                strcat(user_info.password_number_buf, "U");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { //I键
                strcat(user_info.password_number_buf, "I");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { //O键
                strcat(user_info.password_number_buf, "O");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { //P键
                strcat(user_info.password_number_buf, "P");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { //A键
                strcat(user_info.password_number_buf, "A");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { //S键
                strcat(user_info.password_number_buf, "S");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { //D键
                strcat(user_info.password_number_buf, "D");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { //F键
                strcat(user_info.password_number_buf, "F");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                strcat(user_info.password_number_buf, "G");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { //H键
                strcat(user_info.password_number_buf, "H");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { //J键
                strcat(user_info.password_number_buf, "J");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { //K键
                strcat(user_info.password_number_buf, "K");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { //L键
                strcat(user_info.password_number_buf, "L");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { //Z键
                strcat(user_info.password_number_buf, "Z");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { //X键
                strcat(user_info.password_number_buf, "X");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { //C键
                strcat(user_info.password_number_buf, "C");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { //V键
                strcat(user_info.password_number_buf, "V");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { //B键
                strcat(user_info.password_number_buf, "B");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { //N键
                strcat(user_info.password_number_buf, "N");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { //M键
                strcat(user_info.password_number_buf, "M");
                input_changed = 1;  // 标记输入有变化
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { //删除键
                if (strlen(user_info.password_number_buf) > 0) {
                    user_info.password_number_buf[strlen(user_info.password_number_buf) - 1] = '\0';
                }
                input_changed = 1;  // 标记输入有变化
            }
        }

        // 只有当输入有变化时才更新文本框显示
        if (input_changed) {
            // 绘制密码输入文本框背景
            lcd_draw_filled_rectangle(
                0, 0,               /* 左上角坐标 (x, y) */
                1024, 143,          /* 矩形宽度和高度 */
                COLOR_WHITE         /* 填充颜色 */ 
            );
            /* 密码输入文本框 */ 
            lcd_render_text_with_box(
                user_info.password_number_buf,      /* 文本内容 */
                70, 51,                /* 起始坐标 (x, y) */
                COLOR_BLACK,             /* 文本颜色 */ 
                COLOR_WHITE,             /* 文本框背景颜色 */ 
                0,                       /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_RECTANGLE,     /* 矩形样式 */ 
                0,                       /* 矩形样式不需要半径 */
                60,                      /* 字体大小 */
                0,                     /* 文本框宽度 */
                0                       /* 文本框高度 */
            );        
            /* 确认按钮 */
            lcd_render_text_with_box(
                "确认",                 /* 文本内容 */
                800, 51,                /* 起始坐标 (x, y) */
                COLOR_WHITE,            /* 文本颜色 */ 
                COLOR_LIGHTGRAY,        /* 文本框背景颜色 */ 
                0,                      /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_ROUNDED,      /* 矩形样式 */ 
                15,                     /* 矩形样式不需要半径 */
                60,                     /* 字体大小 */
                0,                      /* 文本框宽度 */
                0                       /* 文本框高度 */
            );
        }
    }

    while (2) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            register_account_box();
            break;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 310 && input_y <= 400) {
            register_password_box();
            break;
        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
            register_judgment();
            break;
        }
    }

    // 关闭触摸屏文件
    close(input_fd);

    // 清理资源
    lcd_cleanup();
}


void home_fun() {
    /* 显示桌面 */  
    show_bmp_to_lcd("home.bmp", 0, 0, 1024, 600);
    while(1) {
        ts_fun();
        if (input_x >= 418 && input_x <= 494 && input_y >= 259 && input_y <= 331) {
            show_bmp_to_lcd("logo.bmp", 0, 0, 1024, 600);
            usleep(1000000);
            show_bmp_to_lcd("anti_addiction.bmp", 0, 0, 1024, 600);
            usleep(1000000);
            show_bmp_to_lcd("login_1.bmp", 0, 0, 1024, 600);
            break;
        } else {
            not_open_game_notification();
            sleep(2);
            home_fun();
        }
        /* 记得写break,你也不想main()之后再执行操作死循环吧 */
        break;
    }
}

void login_fun() {
    /* 初始选择登录界面 */ 
    while(1) {
        ts_fun();
        if (input_x >= 399 && input_x <= 621 && input_y >= 385 && input_y <= 490) {
            /* 进入登录引导界面 */
            login_boot();
            break;
        } else if (input_x >= 942 && input_x <= 1024 && input_y >= 524 && input_y <= 600) {
            /* 点击到关闭按钮 */
            show_bmp_to_lcd("login_exit.bmp", 0, 0, 1024, 600);
            while(2) {
                ts_fun();
                if (input_x >= 313 && input_x <= 497 && input_y >= 363 && input_y <= 447) {
                    home_fun();
                } else if (input_x >= 534 && input_x <= 714 && input_y >= 363 && input_y <= 447) {
                    show_bmp_to_lcd("login_1.bmp", 0, 0, 1024, 600); //  刷新屏幕
                    login_fun();
                }
                break;
            }
            break;
        }
    }
}

/* 登录界面引导 */
void login_boot() 
{
    /* 进入到登录界面 */
    show_bmp_to_lcd("login_2.bmp", 0, 0, 1024, 600);
    account_password_background_box();

    // 标志变量，0 表示隐藏密码，1 表示显示密码，默认隐藏
    int show_hide_password_flags = 0; 
    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);

    while (1) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            input_account_box();    /* 点击到输入账号文本框 */    
        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 400) {
            input_password_box();   /* 点击到输入密码文本框 */
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
            login_judgment();       /* 首次空账号密码登录，跳转登陆判断 */
        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
            register_background_box();  /* 点击到注册账号按钮 */
            while (2) {
                ts_fun();
                if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                    register_account_box();
                    break;
                } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 400) {
                    register_password_box();
                    break;
                }
            }
        } else if (input_x >= 104 && input_x <= 208 && input_y >= 424 && input_y <= 484) {      
            register_judgment();   /* 点击到注册按钮 */
        } else if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
            show_hide_password_flags = !show_hide_password_flags; 
            if (show_hide_password_flags) {
                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
            } else {
                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
            }
        } else {
            continue;
        }

        /* 
        * 此处写break是因为当继续停留在login_boot()函数时，
        * 若成功进入游戏会进这里的死循环
        * 
        * 解决方法，设置登录成功标记位，若登陆成功，break;
        * 此处就是为了解决比如显示隐藏密码不与其他input函数并存并可以重复多按，
        * 若登录成功也不会陷入这里的死循环
        */
        if (stop_show_or_hide_password_while == 1) {
            break;
            stop_show_or_hide_password_while = 0;  /* 重置登录标记位 */
        }
    }
}

/* 登录判断 */
void login_judgment() {
    /* 初始化字库 */ 
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }
    /* 满足所有登录条件时 */
    if (strlen(user_info.account_number_buf) != 0 && strlen(user_info.password_number_buf) != 0) {
        printf("login sucessful.\n");
        lcd_render_text_with_box(
            "登录成功，请尽情游玩~",          /* 文本内容 */
            350, 400,                       /* 起始坐标 (x, y) */
            COLOR_WHITE,                    /* 文本颜色 */
            COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
            10,                             /* 文本与文本框边缘的间距 */
            BOX_STYLE_ROUNDED,              /* 圆角样式 */
            15,                             /* 圆角半径 */
            30,                             /* 字体大小 */
            0,                              /* 文本框宽度 */
            0                               /* 文本框高度 */
        );
        sleep(3);
        stop_show_or_hide_password_while = 1;  /* 登录成功，标记位置1 */
    } else if (strlen(user_info.account_number_buf) == 0 || strlen(user_info.password_number_buf) == 0) {
        /* 账号或密码为空 */
        lcd_render_text_with_box(
            "账号或密码为空，请重新登录",     /* 文本内容 */
            350, 400,                       /* 起始坐标 (x, y) */
            COLOR_WHITE,                    /* 文本颜色 */
            COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
            10,                             /* 文本与文本框边缘的间距 */
            BOX_STYLE_ROUNDED,              /* 圆角样式 */
            15,                             /* 圆角半径 */
            30,                             /* 字体大小 */
            0,                              /* 文本框宽度 */
            0                               /* 文本框高度 */
        );
        sleep(3);
        /* 返回上一层 */ 
        memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
        memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
        login_boot();
    }

    /* 清理资源 */ 
    lcd_cleanup();
}

/* 注册判断 */
void register_judgment() {
    /* 初始化字库 */ 
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }
    /* 满足所有注册条件时 */
    if (strlen(user_info.account_number_buf) != 0 && strlen(user_info.password_number_buf) != 0) {
        lcd_render_text_with_box(
            "登录成功，请尽情游玩~",     /* 文本内容 */
            350, 400,                       /* 起始坐标 (x, y) */
            COLOR_WHITE,                    /* 文本颜色 */
            COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
            10,                             /* 文本与文本框边缘的间距 */
            BOX_STYLE_ROUNDED,              /* 圆角样式 */
            15,                             /* 圆角半径 */
            30,                             /* 字体大小 */
            0,                              /* 文本框宽度 */
            0                               /* 文本框高度 */
        );
        printf("register sucessful.\n");
        memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
        memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
        login_boot();
    } else if (strlen(user_info.account_number_buf) == 0 || strlen(user_info.password_number_buf) == 0) {
        /* 账号或密码为空时 */
        lcd_render_text_with_box(
            "账号或密码为空，请重新注册",     /* 文本内容 */
            350, 400,                       /* 起始坐标 (x, y) */
            COLOR_WHITE,                    /* 文本颜色 */
            COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
            10,                             /* 文本与文本框边缘的间距 */
            BOX_STYLE_ROUNDED,              /* 圆角样式 */
            15,                             /* 圆角半径 */
            30,                             /* 字体大小 */
            0,                              /* 文本框宽度 */
            0                               /* 文本框高度 */
        );
        sleep(3);
        /* 返回上一层 */ 
        memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
        memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
        login_boot();
    }

    /* 清理资源 */ 
    lcd_cleanup();
}

void game_start_home() {
    show_bmp_to_lcd("1.bmp", 0, 0, 1024, 600);
}

int main() 
{
    home_fun();
    login_fun();
    /*
    * 逻辑：当在login_fun()调用login_boot()后，
    * 跳转至login_judement进行判断，若账号与密码都输入正确，会跳转到game_start_home()，
    * 绕过main函数的login_judgment()。
    * 若此时main函数再执行login_judgment()会进行重复判定，产生错误，
    * 因此注释掉login_judgment()。
    */
    //login_judement();
    game_start_home();
    return 0;
}