/*************************************************************************************************************
File name: p.c
Author: KD
Version: V_5.0
Build date: 2025-07-09
Description: Final_Version.
Others: Usage requires preservation of original author attribution.
Log: 1.新增刮刮乐游戏主体的实现
     2.新增刮刮乐游戏结束时再来一次的选项
     3.新增刮刮乐游戏结束时返回主界面的功能
     4.修复已知问题
     5.提高游戏稳定性
bug: you tell me!
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
#include <time.h>
#include "show_bmp_to_lcd.h"
#include "show_gif_to_lcd.h"
#include "lcd_font.h"

/* 全局变量定义 */
int input_x, input_y;                   /* 触摸点 x 和 y 坐标 */
int start_x = 0, start_y = 0;           // 全局起始坐标
int login_allow_flags = 0;              /* 允许登录标记位 1：允许 */
int skip_login_boot_flags = 0;          /* 停止login_boot()的死循环 */
int register_allow_flags = 0;           /* 允许注册标记位 1：允许 */
int skip_register_boot_flags = 0;       /* 停止register_boot()的死循环 */
int find_account_allow_flags = 0;       /* 允许找回标志位 1：允许*/
int code_button_visible = 1;            /* 验证码按钮可见性标记 1：可见状态*/
int code_button_clicked = 0;            /* 验证码按钮点击状态 0：未在倒计时 */
int countdown = 0;                      /* 倒计时秒数，初始为0，点击按钮后重置为60 */
int thread_running = 0;                 /* 线程运行状态标记，防止重复创建线程（解决验证码重复获取的问题） */
int skip_find_account_boot_flags = 0;   /* 停止find_account_boot()的死循环 */
char code[7];                           /* 存储生成的随机验证码，留一位给换行符 */
time_t start_time = 0;                  /* 利用系统时间戳计算精确倒计时，避免sleep累积误差 */
pthread_t code_thread;                  /* 验证码线程ID，用于管理线程生命周期 */
unsigned short *lcd_buf;                // 屏幕内存映射
unsigned short *prize_buf;              // 奖项图片缓存
unsigned char *scratched;               // 记录已刮开的像素
int scratched_count = 0;                // 已刮开的像素数量
int threshold_reached = 0;              // 是否达到阈值
float threshold_ratio = 0.20;           // 触发阈值比例（20%）
int stop_touch = 0;                     // 新增变量，用于控制触摸事件循环


/* 定义包含账号、密码与验证码的数组结构体 */ 
typedef struct 
{
    char account_number_buf[128];           /* 用于存储账号的数组 */
    char password_number_buf[128];          /* 用于存储密码的数组 */
    char hide_password_number_buf[128];     /* 用于隐藏密码的数组 */
    char verification_code_buf[128];        /* 用于存储验证码的数组 */
} UserInfo;
UserInfo user_info = {{0}, {0}, {0}, {0}};  /* 初始化用户信息结构体 */  

/* 全局向前声明各函数 */ 
void input_account_box();
void input_password_box();
void register_account_box();
void register_password_box();
void find_account_account_box();
void find_account_password_box();
void find_account_verification();
void login_fun();
void login_boot();
void login_judgment();
void register_boot();
void register_judgment();
void find_account_boot();
void find_account_judgment_1();
void find_account_judgment_2();
void game_start_home();
void game_exit();
int main();

/* 触摸功能实现 */
void ts_fun()
{   
    /* 打开触摸屏文件 */
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        printf("Failed to open touchscreen device.\n");
        return;
    }
    
    struct input_event input_buf;
    while (1) {
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

    /* 关闭文件描述符 */
    close(input_fd);
}

/* 未点击到游戏文本提示 */
void not_open_game_notification() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    } 

    /* LCD屏幕提示 */ 
    lcd_render_text_with_box(
        "未点击到游戏，请重试！",   /* 文本内容 */
        350, 400,                 /* 起始坐标 (x, y) */
        COLOR_WHITE,              /* 文本颜色 */
        COLOR_LIGHTGRAY,          /* 文本框背景颜色 */
        10,                       /* 文本与文本框边缘的间距 */
        BOX_STYLE_ROUNDED,        /* 圆角矩形样式 */
        15,                       /* 圆角矩形半径 */
        30,                       /* 字体大小 */
        0,                        /* 文本框宽度，为0时，文本框大小依照文字大小与文本量大小调整，文字居中对齐 */
        0                         /* 文本框高度，为0时，文本框大小依照文字大小与文本量大小调整，文字居中对齐 */
    );

    /* 清理资源 */ 
    lcd_cleanup();
}

/* 调用虚拟键盘 */
void keyboard() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 渲染虚拟键盘背景 */
    lcd_draw_filled_rectangle(0, 130, 1024, 600, COLOR_WHITE);  
    /* 渲染虚拟键盘文字排布 */
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
    lcd_render_text("除", 50, 500, COLOR_BLACK, 50);
    lcd_render_text("Z", 150, 400, COLOR_BLACK, 50);
    lcd_render_text("X", 250, 400, COLOR_BLACK, 50);
    lcd_render_text("C", 350, 400, COLOR_BLACK, 50);
    lcd_render_text("V", 450, 400, COLOR_BLACK, 50);
    lcd_render_text("B", 550, 400, COLOR_BLACK, 50);
    lcd_render_text("N", 650, 400, COLOR_BLACK, 50);
    lcd_render_text("M", 750, 400, COLOR_BLACK, 50);
    lcd_render_text("确", 900, 400, COLOR_BLACK, 50);
    lcd_render_text("认", 900, 500, COLOR_BLACK, 50);
    
    lcd_draw_filled_rounded_rectangle(200, 480, 630, 100, 15, COLOR_LIGHTGRAY);
    lcd_render_text("空格", 450, 505, COLOR_WHITE, 50);

    /* 此处不能清理资源，因为键盘需要一直渲染，否则会卡住 */
    //lcd_cleanup();
}

/* 登陆界面提示文本渲染 */
void account_password_background_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf")!= 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 渲染账号背景文本框 */
    lcd_draw_filled_rectangle(
        104, 248,               /* 左上角坐标 (x, y) */
        386, 50,                /* 矩形宽度和高度 */
        COLOR_WHITE             /* 填充颜色 */
    );
    lcd_render_text(
        "请输入账号",            /* 文本内容 */
        104, 260,               /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,        /* 文本颜色 */
        25                      /* 字体大小 */
    );
    /* 渲染密码背景文本框 */
    lcd_draw_filled_rectangle(
        104, 310,               /* 左上角坐标 (x, y) */
        386, 50,                /* 矩形宽度和高度 */
        COLOR_WHITE             /* 填充颜色 */
    );
    lcd_render_text(
        "请输入密码",            /* 文本内容 */
        104, 323,               /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,        /* 文本颜色 */
        25                      /* 字体大小 */
    );

    /* 清理资源 */
    lcd_cleanup(); 
}

/* 注册界面提示文本渲染 */
void register_background_box()
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf")!= 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 渲染账号背景文本框 */
    lcd_draw_filled_rectangle(
        104, 248,                 /* 左上角坐标 (x, y) */
        386, 50,                  /* 矩形宽度和高度 */
        COLOR_WHITE               /* 填充颜色 */
    );
    lcd_render_text(
        "请输入要注册的账号",       /* 文本内容 */
        104, 260,                 /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,          /* 文本颜色 */
        25                        /* 字体大小 */
    );
    /* 渲染密码背景文本框 */
    lcd_draw_filled_rectangle(
        104, 310,                 /* 左上角坐标 (x, y) */
        386, 50,                  /* 矩形宽度和高度 */
        COLOR_WHITE               /* 填充颜色 */
    );
    lcd_render_text(
        "请输入不少于8位数的密码",  /* 文本内容 */
        104, 323,                 /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,          /* 文本颜色 */
        25                        /* 字体大小 */
    );

    /* 清理资源 */
    lcd_cleanup();    
}

/* 账号找回界面提示文本渲染 */
void find_account_background_box()
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf")!= 0) {
        printf("Font library initialization failed.\n");
        return;
    }
 
    /* 渲染账号背景文本框 */
    lcd_draw_filled_rectangle(
        104, 248,                     /* 左上角坐标 (x, y) */
        386, 50,                      /* 矩形宽度和高度 */
        COLOR_WHITE                   /* 填充颜色 */
    );
    lcd_render_text(
        "请输入要找回的账号",           /* 文本内容 */
        104, 260,                     /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,              /* 文本颜色 */
        25                            /* 字体大小 */
    );
    /* 渲染密码背景文本框 */
    lcd_draw_filled_rectangle(
        104, 310,                     /* 左上角坐标 (x, y) */
        386, 50,                      /* 矩形宽度和高度 */
        COLOR_WHITE                   /* 填充颜色 */
    );
    lcd_render_text(
        "请输入要修改的密码(8-12位)",   /* 文本内容 */
        104, 323,                     /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,              /* 文本颜色 */
        25                            /* 字体大小 */
    );

    /* 清理资源 */
    lcd_cleanup();
}

/* 登录界面账号输入功能实现 */
void input_account_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 加载背景图层 */
    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600); 
    
    /* 置顶账号输入框，以解决闪烁问题 */
    /* 渲染账号背景文本框 */
    lcd_draw_filled_rectangle(
        0, 0,                              /* 左上角坐标 (x, y) */
        1024, 143,                         /* 矩形宽度和高度 */
        COLOR_WHITE                        /* 填充颜色 */ 
    );
    lcd_render_text_with_box(
        user_info.account_number_buf,      /* 文本内容 */
        70, 51,                            /* 起始坐标 (x, y) */
        COLOR_BLACK,                       /* 文本颜色 */ 
        COLOR_WHITE,                       /* 文本框背景颜色 */ 
        0,                                 /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,               /* 矩形样式 */ 
        0,                                 /* 矩形样式不需要半径 */
        60,                                /* 字体大小 */
        0,                                 /* 文本框宽度 */
        0                                  /* 文本框高度 */
    );
    /* 渲染确认按钮 */
    lcd_render_text_with_box(
        "确认",                            /* 文本内容 */
        800, 51,                           /* 起始坐标 (x, y) */
        COLOR_WHITE,                       /* 文本颜色 */ 
        COLOR_LIGHTGRAY,                   /* 文本框背景颜色 */ 
        0,                                 /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,                 /* 圆角矩形样式 */ 
        15,                                /* 圆角矩形半径 */
        60,                                /* 字体大小 */
        0,                                 /* 文本框宽度 */
        0                                  /* 文本框高度 */
    );
    keyboard();    /* 加载键盘 */

    /* 为特定的确认按钮单独开一个触摸函数 */
    /* 打开触摸屏文件 */ 
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        printf("Failed to open touchscreen device.\n");
        return;
    }
    struct input_event input_buf;

    while (1) {
        int input_changed = 0;  /* 标记输入是否有变化 */ 

        /* 读取触摸屏数据 */ 
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {
                /* 点击了确认按钮和确认键 */

                /* 账号文本处理 */
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 账号输入为空 */
                    
                    /* 加载背景图层 */
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                    /* 渲染账号背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 248,               /* 左上角坐标 (x, y) */
                        386, 50,                /* 矩形宽度和高度 */
                        COLOR_WHITE             /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入账号",            /* 文本内容 */
                        104, 260,               /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,        /* 文本颜色 */
                        25                      /* 字体大小 */
                    );            
                } else if (strlen(user_info.account_number_buf) <= 12) {    
                    /* 账号不超过12位 */
                    
                    /* 加载背景图层 */
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                    /* 渲染账号背景文本框 */ 
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                          /* 起始坐标 (x, y) */
                        COLOR_BLACK,                       /* 文本颜色 */ 
                        COLOR_WHITE,                       /* 文本框背景颜色 */ 
                        0,                                 /* 文本与文本框边缘的间距 */ 
                        BOX_STYLE_RECTANGLE,               /* 矩形样式 */ 
                        0,                                 /* 矩形样式不需要半径 */
                        50,                                /* 字体大小 */
                        386,                               /* 文本框宽度 */
                        50                                 /* 文本框高度 */
                    );  
                } else {
                    lcd_render_text_with_box(
                        "账号超过12位，请重新输入！",        /* 文本内容 */
                        310, 400,                          /* 起始坐标 (x, y) */
                        COLOR_WHITE,                       /* 文本颜色 */
                        COLOR_LIGHTGRAY,                   /* 文本框背景颜色 */
                        10,                                /* 文本与文本框边缘的间距 */
                        BOX_STYLE_ROUNDED,                 /* 圆角矩形样式 */
                        15,                                /* 圆角矩形半径 */
                        30,                                /* 字体大小 */
                        0,                                 /* 文本框宽度 */
                        0                                  /* 文本框高度 */
                    );
                    sleep(3);

                    /* 重置允许登录标记位 */
                    login_allow_flags = 0;

                    /* 清空已输入的账号与密码 */
                    memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                    memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                    memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));

                    /* 重新输入账号 */
                    input_account_box(); 
                    return;
                }

                /*
                * 特色:密码显示与隐藏，与登陆判断在同一页，
                * 这意味着无论是否输入密码，均可以同时响应密码显示与隐藏或登录，
                * 并且可以在操作期间任意切换账号或密码输入。
                * 实现在操作期间任意切换账号或密码输入、密码显示与隐藏与登录，规避许多逻辑错误，
                * 优化整体结构。
                */

                /* 密码文本处理 */
                /* 显示或隐藏密码开关 0：隐藏 1：显示 */
                int show_hide_password_flags = 0;

                /* 渲染密码背景文本框 */
                lcd_draw_filled_rectangle(
                    104, 310,                   /* 左上角坐标 (x, y) */
                    386, 50,                    /* 矩形宽度和高度 */
                    COLOR_WHITE                 /* 填充颜色 */
                );

                /* 空密码时 */
                if (strlen(user_info.password_number_buf) == 0) {
                    lcd_render_text(
                        "请输入密码",            /* 文本内容 */
                        104, 323,               /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,        /* 文本颜色 */
                        25                      /* 字体大小 */
                    );
                    /* 默认隐藏密码 */
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,               /* 左上角坐标 (x, y) */
                                    386, 50,                /* 矩形宽度和高度 */
                                    COLOR_WHITE             /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入密码",            /* 文本内容 */
                                    104, 323,               /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,        /* 文本颜色 */
                                    25                      /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,               /* 左上角坐标 (x, y) */
                                    386, 50,                /* 矩形宽度和高度 */
                                    COLOR_WHITE             /* 填充颜色 */
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
                            /* 点击到账号文本框 */
                            input_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            /* 点击到密码文本框 */
                            input_password_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            /* 点击到登录按钮 */
                            login_judgment();
                            return;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 点击到注册按钮 */

                            /* 清空已输入的账号与密码 */
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            register_boot();
                            return;
                        }
                    }
                    break;
                }
                /* 非空密码时 */
                if (strlen(user_info.password_number_buf) != 0) {
                    lcd_draw_filled_rectangle(
                        104, 310,                           /* 左上角坐标 (x, y) */
                        386, 50,                            /* 矩形宽度和高度 */
                        COLOR_WHITE                         /* 填充颜色 */
                    );
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf, /* 文本内容 */
                        104, 310,                           /* 起始坐标 (x, y) */
                        COLOR_BLACK,                        /* 文本颜色 */  
                        COLOR_WHITE,                        /* 文本框背景颜色 */
                        0,                                  /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                        0,                                  /* 矩形样式不需要半径 */
                        50,                                 /* 字体大小 */
                        386,                                /* 文本框宽度 */
                        50                                  /* 文本框高度 */
                    );
                    
                    /* 默认隐藏密码 */
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (4) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                           /* 左上角坐标 (x, y) */
                                    386, 50,                            /* 矩形宽度和高度 */
                                    COLOR_WHITE                         /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,      /* 文本内容 */
                                    104, 310,                           /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                        /* 文本颜色 */  
                                    COLOR_WHITE,                        /* 文本框背景颜色 */
                                    0,                                  /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                                    0,                                  /* 矩形样式不需要半径 */
                                    50,                                 /* 字体大小 */
                                    386,                                /* 文本框宽度 */
                                    50                                  /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                           /* 左上角坐标 (x, y) */
                                    386, 50,                            /* 矩形宽度和高度 */
                                    COLOR_WHITE                         /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf, /* 文本内容 */
                                    104, 310,                           /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                        /* 文本颜色 */  
                                    COLOR_WHITE,                        /* 文本框背景颜色 */
                                    0,                                  /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                                    0,                                  /* 矩形样式不需要半径 */
                                    50,                                 /* 字体大小 */
                                    386,                                /* 文本框宽度 */
                                    50                                  /* 文本框高度 */
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
                            /* 满足所有输入条件，允许登录 */
                            login_allow_flags = 1;    
                            login_judgment();
                            return;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            register_boot();
                            return;
                        }
                    }
                }
                break;  
            }

            /* 处理键盘点击事件 */ 
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) {
                /* 空格键 */ 
                strcat(user_info.account_number_buf, " ");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { 
                /* Q键 */
                strcat(user_info.account_number_buf, "Q");
                input_changed = 1;  
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { 
                /* W键 */
                strcat(user_info.account_number_buf, "W");
                input_changed = 1;  
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { 
                /* E键 */
                strcat(user_info.account_number_buf, "E");
                input_changed = 1;  
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { 
                /* R键 */
                strcat(user_info.account_number_buf, "R");
                input_changed = 1;  
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { 
                /* T键 */
                strcat(user_info.account_number_buf, "T");
                input_changed = 1;  
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { 
                /* Y键 */
                strcat(user_info.account_number_buf, "Y");
                input_changed = 1;  
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { 
                /* U键 */
                strcat(user_info.account_number_buf, "U");
                input_changed = 1;  
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { 
                /* I键 */
                strcat(user_info.account_number_buf, "I");
                input_changed = 1;  
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { 
                /* O键 */
                strcat(user_info.account_number_buf, "O");
                input_changed = 1;  
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { 
                /* P键 */
                strcat(user_info.account_number_buf, "P");
                input_changed = 1;  
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { 
                /* A键 */
                strcat(user_info.account_number_buf, "A");
                input_changed = 1;  
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { 
                /* S键 */
                strcat(user_info.account_number_buf, "S");
                input_changed = 1;  
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { 
                /* D键 */
                strcat(user_info.account_number_buf, "D");
                input_changed = 1;  
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { 
                /* F键 */
                strcat(user_info.account_number_buf, "F");
                input_changed = 1;  
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                strcat(user_info.account_number_buf, "G");
                input_changed = 1;  
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { 
                /* H键 */
                strcat(user_info.account_number_buf, "H");
                input_changed = 1;  
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { 
                /* J键 */
                strcat(user_info.account_number_buf, "J");
                input_changed = 1;  
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { 
                /* K键 */
                strcat(user_info.account_number_buf, "K");
                input_changed = 1;  
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { 
                /* L键 */
                strcat(user_info.account_number_buf, "L");
                input_changed = 1;  
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { 
                /* Z键 */
                strcat(user_info.account_number_buf, "Z");
                input_changed = 1;  
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { 
                /* X键 */
                strcat(user_info.account_number_buf, "X");
                input_changed = 1;  
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { 
                /* C键 */
                strcat(user_info.account_number_buf, "C");
                input_changed = 1;  
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { 
                /* V键 */
                strcat(user_info.account_number_buf, "V");
                input_changed = 1;  
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { 
                /* B键 */
                strcat(user_info.account_number_buf, "B");
                input_changed = 1;  
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { 
                /* N键 */
                strcat(user_info.account_number_buf, "N");
                input_changed = 1;  
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { 
                /* M键 */
                strcat(user_info.account_number_buf, "M");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { 
                /* 删除键 */
                if (strlen(user_info.account_number_buf) > 0) {
                    user_info.account_number_buf[strlen(user_info.account_number_buf) - 1] = '\0';
                }
                input_changed = 1;  
            }

            /* 标记输入有变化 */
            if (input_changed) {
                /* 更新账号输入文本框 */
                lcd_draw_filled_rectangle(
                    0, 0,                           /* 左上角坐标 (x, y) */
                    1024, 143,                      /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */ 
                );
                lcd_render_text_with_box(
                    user_info.account_number_buf,   /* 文本内容 */
                    70, 51,                         /* 起始坐标 (x, y) */
                    COLOR_BLACK,                    /* 文本颜色 */ 
                    COLOR_WHITE,                    /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                    0,                              /* 矩形样式不需要半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );        
                /* 确认按钮 */
                lcd_render_text_with_box(
                    "确认",                         /* 文本内容 */
                    800, 51,                        /* 起始坐标 (x, y) */
                    COLOR_WHITE,                    /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
                    15,                             /* 圆角矩形半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );
            }
        }
    }

    /* 关闭文件，释放资源 */
    close(input_fd);
    lcd_cleanup();
}

/* 登录界面密码输入功能实现 */
void input_password_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 加载背景图层 */
    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);  
    /* 渲染密码背景文本框 */  
    lcd_draw_filled_rectangle(
        0, 0,                           /* 左上角坐标 (x, y) */
        1024, 143,                      /* 矩形宽度和高度 */
        COLOR_WHITE                     /* 填充颜色 */ 
    );
    lcd_render_text_with_box(
        user_info.password_number_buf,  /* 文本内容 */
        70, 51,                         /* 起始坐标 (x, y) */
        COLOR_BLACK,                    /* 文本颜色 */ 
        COLOR_WHITE,                    /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
        0,                              /* 矩形样式不需要半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",                         /* 文本内容 */
        800, 51,                        /* 起始坐标 (x, y) */
        COLOR_WHITE,                    /* 文本颜色 */ 
        COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
        15,                             /* 圆角矩形半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 加载键盘 */
    keyboard();    

    /* 打开触摸屏文件 */
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }
    struct input_event input_buf;

    while (1) {
        /* 标记输入是否有变化 */ 
        int input_changed = 0;  

        /* 读取触摸屏数据 */ 
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) { 
                /* 点击到确认按钮和确认键 */

                /* 处理账号输入 */
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 账号为空时 */
                    if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                        /* 仅当密码符合条件时才显示图层 */
                        show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,               /* 左上角坐标 (x, y) */
                            386, 50,                /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                        lcd_render_text(
                            "请输入账号",            /* 文本内容 */
                            104, 260,               /* 起始坐标 (x, y) */
                            COLOR_LIGHTGRAY,        /* 文本颜色 */
                            25                      /* 字体大小 */
                        );
                    } else {
                        /* 密码不符合条件，屏幕开始提示，不显示图层 */
                        /* 渲染账号背景文本框，防止遮挡 */
                        lcd_draw_filled_rectangle(
                            0, 0,                   /* 左上角坐标 (x, y) */
                            1, 1,                   /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                    }           
                } else {
                    /* 非空账号 */
                    if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                        /* 仅当密码符合条件时才显示图层 */
                        show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,       /* 左上角坐标 (x, y) */
                            386, 50,        /* 矩形宽度和高度 */
                            COLOR_WHITE     /* 填充颜色 */
                        );
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
                    } else {
                        /* 密码不符合条件，屏幕提示，不显示图层 */
                        //show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            0, 0,                   /* 左上角坐标 (x, y) */
                            1, 1,                   /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                    }
                }

                /* 处理密码输入 */
                /* 显示或隐藏密码开关 */
                int show_hide_password_flags = 0;

                /* 空密码时 */
                if (strlen(user_info.password_number_buf) == 0) {
                    /* 加载背景图层 */
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                    /* 渲染密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,               /* 左上角坐标 (x, y) */
                        386, 50,                /* 矩形宽度和高度 */
                        COLOR_WHITE             /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入密码",            /* 文本内容 */
                        104, 323,               /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,        /* 文本颜色 */
                        25                      /* 字体大小 */
                    );
                    /* 单独账号渲染,防止界面紊乱 */
                    if (strlen(user_info.account_number_buf) == 0) {
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,                   /* 左上角坐标 (x, y) */
                            386, 50,                    /* 矩形宽度和高度 */
                            COLOR_WHITE                 /* 填充颜色 */
                        );
                        lcd_render_text(
                            "请输入要注册的账号",         /* 文本内容 */
                            104, 260,                   /* 起始坐标 (x, y) */
                            COLOR_LIGHTGRAY,            /* 文本颜色 */
                            25                          /* 字体大小 */
                        );
                    } else {
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,       /* 左上角坐标 (x, y) */
                            386, 50,        /* 矩形宽度和高度 */
                            COLOR_WHITE     /* 填充颜色 */
                        );
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

                    /* 默认隐藏密码 */
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (2) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入密码",                /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入密码",                /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
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
                            /* 空账号与密码强制不允许登录 */
                            login_allow_flags = 0;    
                            login_judgment();
                            return;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 点击注册按钮，可强行进入注册界面 */
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            register_boot();
                            return;
                        }
                    }
                    break;
                }
                /* 非空密码时 密码位数：8-12 */
                if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,                           /* 左上角坐标 (x, y) */
                        386, 50,                            /* 矩形宽度和高度 */
                        COLOR_WHITE                         /* 填充颜色 */
                    );
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf, /* 文本内容 */
                        104, 310,                           /* 起始坐标 (x, y) */
                        COLOR_BLACK,                        /* 文本颜色 */  
                        COLOR_WHITE,                        /* 文本框背景颜色 */
                        0,                                  /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                        0,                                  /* 矩形样式不需要半径 */
                        50,                                 /* 字体大小 */
                        386,                                /* 文本框宽度 */
                        50                                  /* 文本框高度 */
                    );

                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                       /* 左上角坐标 (x, y) */
                                    386, 50,                        /* 矩形宽度和高度 */
                                    COLOR_WHITE                     /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,  /* 文本内容 */
                                    104, 310,                       /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                    /* 文本颜色 */  
                                    COLOR_WHITE,                    /* 文本框背景颜色 */
                                    0,                              /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */
                                    0,                              /* 矩形样式不需要半径 */
                                    50,                             /* 字体大小 */
                                    386,                            /* 文本框宽度 */
                                    50                              /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                       /* 左上角坐标 (x, y) */
                                    386, 50,                        /* 矩形宽度和高度 */
                                    COLOR_WHITE                     /* 填充颜色 */
                                );
                                /* 渲染密码背景文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf,  /* 文本内容 */
                                    104, 310,                            /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                         /* 文本颜色 */  
                                    COLOR_WHITE,                         /* 文本框背景颜色 */
                                    0,                                   /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                 /* 矩形样式 */
                                    0,                                   /* 矩形样式不需要半径 */
                                    50,                                  /* 字体大小 */
                                    386,                                 /* 文本框宽度 */
                                    50                                   /* 文本框高度 */
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
                            /* 满足输入条件，允许登录 */
                            login_allow_flags = 1;    
                            login_judgment();
                            return;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 点击到注册按钮 */
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            register_boot();
                            return;
                        }
                    }
                } else {
                    /* 密码位数小于8或大于12 */
                    lcd_render_text_with_box(
                        "密码不符合要求，请重新输入8-12位密码！",          /* 文本内容 */
                        233, 400,                                       /* 起始坐标 (x, y) */
                        COLOR_WHITE,                                    /* 文本颜色 */
                        COLOR_LIGHTGRAY,                                /* 文本框背景颜色 */
                        10,                                             /* 文本与文本框边缘的间距 */
                        BOX_STYLE_ROUNDED,                              /* 圆角矩形样式 */
                        15,                                             /* 圆角矩形半径 */
                        30,                                             /* 字体大小 */
                        0,                                              /* 文本框宽度 */
                        0                                               /* 文本框高度 */
                    );
                    sleep(3);
                    login_allow_flags = 0;
                    memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                    memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                    memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                    input_password_box();
                    return;
                }
                break;
            }

            /* 处理键盘点击事件 */ 
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { 
                strcat(user_info.password_number_buf, " ");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { 
                strcat(user_info.password_number_buf, "Q");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { 
                strcat(user_info.password_number_buf, "W");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "E");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "R");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "T");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "Y");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "U");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "I");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "O");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "P");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "A");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "S");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "D");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "F");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "G");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "H");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "J");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "K");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "L");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "Z");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "X");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "C");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "V");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "B");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "N");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "M");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) {
                if (strlen(user_info.password_number_buf) > 0) {
                    user_info.password_number_buf[strlen(user_info.password_number_buf) - 1] = '\0';
                }
                if (strlen(user_info.hide_password_number_buf) > 0) {
                    user_info.hide_password_number_buf[strlen(user_info.hide_password_number_buf) - 1] = '\0';
                }
                input_changed = 1;  
            }

            /* 标记输入有变化 */
            if (input_changed) {
                /* 渲染密码文本框背景 */ 
                lcd_draw_filled_rectangle(
                    0, 0,                           /* 左上角坐标 (x, y) */
                    1024, 143,                      /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */ 
                );
                /* 密码输入文本框 */ 
                lcd_render_text_with_box(
                    user_info.password_number_buf,  /* 文本内容 */
                    70, 51,                         /* 起始坐标 (x, y) */
                    COLOR_BLACK,                    /* 文本颜色 */ 
                    COLOR_WHITE,                    /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                    0,                              /* 矩形样式不需要半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );        
                /* 确认按钮 */
                lcd_render_text_with_box(
                    "确认",                         /* 文本内容 */
                    800, 51,                        /* 起始坐标 (x, y) */
                    COLOR_WHITE,                    /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
                    15,                             /* 圆角矩形半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );
            }
        }
    }

    close(input_fd);
    lcd_cleanup();
}

/* 注册界面账号输入功能实现 */
void register_account_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 加载背景图层 */
    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
    /* 渲染账号背景文本框 */
    lcd_draw_filled_rectangle(
        0, 0,                           /* 左上角坐标 (x, y) */
        1024, 143,                      /* 矩形宽度和高度 */
        COLOR_WHITE                     /* 填充颜色 */ 
    );
    lcd_render_text_with_box(
        user_info.account_number_buf,   /* 文本内容 */
        70, 51,                         /* 起始坐标 (x, y) */
        COLOR_BLACK,                    /* 文本颜色 */ 
        COLOR_WHITE,                    /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
        0,                              /* 矩形样式不需要半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",                         /* 文本内容 */
        800, 51,                        /* 起始坐标 (x, y) */
        COLOR_WHITE,                    /* 文本颜色 */ 
        COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
        15,                             /* 圆角矩形半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 加载键盘 */
    keyboard();    

    /* 打开触摸屏文件 */ 
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }
    struct input_event input_buf;

    while (1) {
        /* 标记输入是否有变化 */
        int input_changed = 0;  

        /* 读取触摸屏数据 */ 
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  
                /* 点击到确认按钮和确认键 */ 
                
                /* 处理账号输入 */
                if (strlen(user_info.account_number_buf) == 0) {
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                    /* 渲染账号背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 248,               /* 左上角坐标 (x, y) */
                        386, 50,                /* 矩形宽度和高度 */
                        COLOR_WHITE             /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入要注册的账号",     /* 文本内容 */
                        104, 260,               /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,        /* 文本颜色 */
                        25                      /* 字体大小 */
                    );            
                } else if (strlen(user_info.account_number_buf) <= 12) {
                    /* 账号不超过12位 */
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    /* 加载背景图层 */
                    /* 渲染账号输入文本框 */ 
                    lcd_render_text_with_box(
                        user_info.account_number_buf,      /* 文本内容 */
                        104, 248,                          /* 起始坐标 (x, y) */
                        COLOR_BLACK,                       /* 文本颜色 */ 
                        COLOR_WHITE,                       /* 文本框背景颜色 */ 
                        0,                                 /* 文本与文本框边缘的间距 */ 
                        BOX_STYLE_RECTANGLE,               /* 矩形样式 */ 
                        0,                                 /* 矩形样式不需要半径 */
                        50,                                /* 字体大小 */
                        386,                               /* 文本框宽度 */
                        50                                 /* 文本框高度 */
                    );  
                } else {
                    /* 账号超过12位 */
                    lcd_render_text_with_box(
                        "账号超过12位，请重新注册！",     /* 文本内容 */
                        310, 400,                       /* 起始坐标 (x, y) */
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
                    register_allow_flags = 0;
                    memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                    memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                    memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                    /* 重新注册 */
                    register_account_box(); 
                    return;
                }

                /*
                * 特色:密码显示与隐藏，与注册判断在同一页，
                * 这意味着无论是否输入密码，均可以同时响应密码显示与隐藏或注册，
                * 并且可以在操作期间任意切换账号或密码输入。
                * 实现在操作期间任意切换账号或密码输入、密码显示与隐藏与注册，规避许多逻辑错误，
                * 优化整体结构。
                */

                /* 处理密码输入 */
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
                        "请输入不少于8位数的密码",                    /* 文本内容 */
                        104, 323,                                   /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                            /* 文本颜色 */
                        25                                          /* 字体大小 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入不少于8位数的密码",    /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入不少于8位数的密码",    /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            register_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            register_password_box();
                            break;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 空账号与密码强制不允许注册 */
                            register_allow_flags = 0; 
                            register_judgment();
                            return;  
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            lcd_render_text_with_box(
                                "当前在注册页面，禁止登录！",     /* 文本内容 */
                                340, 400,                       /* 起始坐标 (x, y) */
                                COLOR_WHITE,                    /* 文本颜色 */
                                COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
                                10,                             /* 文本与文本框边缘的间距 */
                                BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */
                                15,                             /* 圆角矩形半径 */
                                30,                             /* 字体大小 */
                                0,                              /* 文本框宽度 */
                                0                               /* 文本框高度 */
                            );
                            sleep(3);
                            register_allow_flags = 0;
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            login_boot();
                            return; 
                        }
                    }
                    break;
                }
                /* 非空密码时 */
                if (strlen(user_info.password_number_buf) != 0) {
                    lcd_draw_filled_rectangle(
                        104, 310,                           /* 左上角坐标 (x, y) */
                        386, 50,                            /* 矩形宽度和高度 */
                        COLOR_WHITE                         /* 填充颜色 */
                    );
                    /* 渲染密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf,     /* 文本内容 */
                        104, 310,                               /* 起始坐标 (x, y) */
                        COLOR_BLACK,                            /* 文本颜色 */  
                        COLOR_WHITE,                            /* 文本框背景颜色 */
                        0,                                      /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,                    /* 矩形样式 */
                        0,                                      /* 矩形样式不需要半径 */
                        50,                                     /* 字体大小 */
                        386,                                    /* 文本框宽度 */
                        50                                      /* 文本框高度 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (4) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                       /* 左上角坐标 (x, y) */
                                    386, 50,                        /* 矩形宽度和高度 */
                                    COLOR_WHITE                     /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,  /* 文本内容 */
                                    104, 310,                       /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                    /* 文本颜色 */  
                                    COLOR_WHITE,                    /* 文本框背景颜色 */
                                    0,                              /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */
                                    0,                              /* 矩形样式不需要半径 */
                                    50,                             /* 字体大小 */
                                    386,                            /* 文本框宽度 */
                                    50                              /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                       /* 左上角坐标 (x, y) */
                                    386, 50,                        /* 矩形宽度和高度 */
                                    COLOR_WHITE                     /* 填充颜色 */
                                );
                                /* 渲染密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf, /* 文本内容 */
                                    104, 310,                           /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                        /* 文本颜色 */  
                                    COLOR_WHITE,                        /* 文本框背景颜色 */
                                    0,                                  /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                                    0,                                  /* 矩形样式不需要半径 */
                                    50,                                 /* 字体大小 */
                                    386,                                /* 文本框宽度 */
                                    50                                  /* 文本框高度 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            register_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            register_password_box();
                            break;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 满足所有输入条件，允许注册 */
                            register_allow_flags = 1; 
                            register_judgment();
                            return;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                             lcd_render_text_with_box(
                                "当前在注册页面，禁止登录！",      /* 文本内容 */
                                340, 400,                       /* 起始坐标 (x, y) */
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
                            register_allow_flags = 0;
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            login_boot();
                            return;
                        }
                    }
                }
                break;  
            }

            /* 处理键盘点击事件 */ 
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { 
                strcat(user_info.account_number_buf, " ");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "Q");
                input_changed = 1;  
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "W");
                input_changed = 1;  
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "E");
                input_changed = 1;  
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "R");
                input_changed = 1;  
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "T");
                input_changed = 1;  
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "Y");
                input_changed = 1;  
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "U");
                input_changed = 1;  
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "I");
                input_changed = 1;  
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "O");
                input_changed = 1;  
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "P");
                input_changed = 1;  
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "A");
                input_changed = 1;  
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "S");
                input_changed = 1;  
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "D");
                input_changed = 1;  
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "F");
                input_changed = 1;  
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "G");
                input_changed = 1;  
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "H");
                input_changed = 1;  
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "J");
                input_changed = 1;  
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "K");
                input_changed = 1;  
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "L");
                input_changed = 1;  
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "Z");
                input_changed = 1;  
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "X");
                input_changed = 1;  
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "C");
                input_changed = 1;  
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "V");
                input_changed = 1;  
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "B");
                input_changed = 1;  
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "N");
                input_changed = 1;  
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "M");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) {
                if (strlen(user_info.account_number_buf) > 0) {
                    user_info.account_number_buf[strlen(user_info.account_number_buf) - 1] = '\0';
                }
                input_changed = 1;  
            }

            /* 标记输入变化 */
            if (input_changed) {
                lcd_draw_filled_rectangle(
                    0, 0,                           /* 左上角坐标 (x, y) */
                    1024, 143,                      /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */ 
                );
                lcd_render_text_with_box(
                    user_info.account_number_buf,   /* 文本内容 */
                    70, 51,                         /* 起始坐标 (x, y) */
                    COLOR_BLACK,                    /* 文本颜色 */ 
                    COLOR_WHITE,                    /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                    0,                              /* 矩形样式不需要半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );        
                /* 确认按钮 */
                lcd_render_text_with_box(
                    "确认",                          /* 文本内容 */
                    800, 51,                        /* 起始坐标 (x, y) */
                    COLOR_WHITE,                    /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
                    15,                             /* 圆角矩形半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );
            }
        }
    }

    close(input_fd);
    lcd_cleanup();
}    

/* 注册界面密码输入功能实现 */
void register_password_box() 
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 加载背景图层 */
    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
    /* 渲染密码文本框背景 */
    lcd_draw_filled_rectangle(
        0, 0,                           /* 左上角坐标 (x, y) */
        1024, 143,                      /* 矩形宽度和高度 */
        COLOR_WHITE                     /* 填充颜色 */ 
    );
    lcd_render_text_with_box(
        user_info.password_number_buf,  /* 文本内容 */
        70, 51,                         /* 起始坐标 (x, y) */
        COLOR_BLACK,                    /* 文本颜色 */ 
        COLOR_WHITE,                    /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
        0,                              /* 矩形样式不需要半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",                         /* 文本内容 */
        800, 51,                        /* 起始坐标 (x, y) */
        COLOR_WHITE,                    /* 文本颜色 */ 
        COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
        15,                             /* 圆角矩形半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 加载键盘 */
    keyboard();    

    /* 打开触摸屏文件 */ 
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }
    struct input_event input_buf;

    while (1) {
        /* 标记输入是否有变化 */ 
        int input_changed = 0;  

        /* 读取触摸屏数据 */ 
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  /* 确认按钮和确认键 */
                /* 点击到确认按钮 */ 

                /* 处理账号输入 */
                if (strlen(user_info.account_number_buf) == 0) {
                    if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                        /* 加载背景图层 */
                        show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,               /* 左上角坐标 (x, y) */
                            386, 50,                /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                        lcd_render_text(
                            "请输入要注册的账号",     /* 文本内容 */
                            104, 260,               /* 起始坐标 (x, y) */
                            COLOR_LIGHTGRAY,        /* 文本颜色 */
                            25                      /* 字体大小 */
                        );    
                    } else {
                        /* 密码不符合条件，屏幕开始提示，不显示图层 */
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            0, 0,                   /* 左上角坐标 (x, y) */
                            1, 1,                   /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                    }
                } else {
                    /* 非空账号 */
                    if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                        /* 加载背景图层 */
                        show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,                       /* 左上角坐标 (x, y) */
                            386, 50,                        /* 矩形宽度和高度 */
                            COLOR_WHITE                     /* 填充颜色 */
                        );
                        /* 账号输入文本框 */ 
                        lcd_render_text_with_box(
                            user_info.account_number_buf,   /* 文本内容 */
                            104, 248,                       /* 起始坐标 (x, y) */
                            COLOR_BLACK,                    /* 文本颜色 */ 
                            COLOR_WHITE,                    /* 文本框背景颜色 */ 
                            0,                              /* 文本与文本框边缘的间距 */ 
                            BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                            0,                              /* 矩形样式不需要半径 */
                            50,                             /* 字体大小 */
                            386,                            /* 文本框宽度 */
                            50                              /* 文本框高度 */
                        );  
                    } else {
                        /* 密码不符合条件，屏幕提示，不显示图层 */
                        //show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            0, 0,                   /* 左上角坐标 (x, y) */
                            1, 1,                   /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                    }
                }

                /* 显示或隐藏密码开关 */
                int show_hide_password_flags = 0;
                /* 空密码时 */
                if (strlen(user_info.password_number_buf) == 0) {
                    /* 加载背景图层 */
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,                   /* 左上角坐标 (x, y) */
                        386, 50,                    /* 矩形宽度和高度 */
                        COLOR_WHITE                 /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入不少于8位数的密码",    /* 文本内容 */
                        104, 323,                   /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,            /* 文本颜色 */
                        25                          /* 字体大小 */
                    );
                    /* 单独账号渲染,防止界面紊乱 */
                    if (strlen(user_info.account_number_buf) == 0) {
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,               /* 左上角坐标 (x, y) */
                            386, 50,                /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                        lcd_render_text(
                            "请输入要注册的账号",     /* 文本内容 */
                            104, 260,               /* 起始坐标 (x, y) */
                            COLOR_LIGHTGRAY,        /* 文本颜色 */
                            25                      /* 字体大小 */
                        );
                    } else {
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,       /* 左上角坐标 (x, y) */
                            386, 50,        /* 矩形宽度和高度 */
                            COLOR_WHITE     /* 填充颜色 */
                        );
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
                    
                    /* 默认隐藏密码 */
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (2) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入不少于8位数的密码",    /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入不少于8位数的密码",    /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            register_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            register_password_box();
                            break;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 空账号与密码强制不允许注册 */
                            register_allow_flags = 0; 
                            register_judgment();
                            return;  /* 强行退出函数，避免死循环 */  
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            lcd_render_text_with_box(
                                "当前在注册页面，禁止登录！",     /* 文本内容 */
                                340, 400,                       /* 起始坐标 (x, y) */
                                COLOR_WHITE,                    /* 文本颜色 */
                                COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
                                10,                             /* 文本与文本框边缘的间距 */
                                BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */
                                15,                             /* 圆角矩形半径 */
                                30,                             /* 字体大小 */
                                0,                              /* 文本框宽度 */
                                0                               /* 文本框高度 */
                            );
                            sleep(3);
                            register_allow_flags = 0;
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            login_boot();
                            return;
                        }
                    }
                    break;
                }
                /* 非空密码时 密码位数：8-12 */
                if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    lcd_draw_filled_rectangle(
                        104, 310,       /* 左上角坐标 (x, y) */
                        386, 50,        /* 矩形宽度和高度 */
                        COLOR_WHITE     /* 填充颜色 */
                    );
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf,     /* 文本内容 */
                        104, 310,                               /* 起始坐标 (x, y) */
                        COLOR_BLACK,                            /* 文本颜色 */  
                        COLOR_WHITE,                            /* 文本框背景颜色 */
                        0,                                      /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,                    /* 矩形样式 */
                        0,                                      /* 矩形样式不需要半径 */
                        50,                                     /* 字体大小 */
                        386,                                    /* 文本框宽度 */
                        50                                      /* 文本框高度 */
                    );

                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                       /* 左上角坐标 (x, y) */
                                    386, 50,                        /* 矩形宽度和高度 */
                                    COLOR_WHITE                     /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,  /* 文本内容 */
                                    104, 310,                       /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                    /* 文本颜色 */  
                                    COLOR_WHITE,                    /* 文本框背景颜色 */
                                    0,                              /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */
                                    0,                              /* 矩形样式不需要半径 */
                                    50,                             /* 字体大小 */
                                    386,                            /* 文本框宽度 */
                                    50                              /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                           /* 左上角坐标 (x, y) */
                                    386, 50,                            /* 矩形宽度和高度 */
                                    COLOR_WHITE                         /* 填充颜色 */
                                );
                                /* 密码输入文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf, /* 文本内容 */
                                    104, 310,                           /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                        /* 文本颜色 */  
                                    COLOR_WHITE,                        /* 文本框背景颜色 */
                                    0,                                  /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                                    0,                                  /* 矩形样式不需要半径 */
                                    50,                                 /* 字体大小 */
                                    386,                                /* 文本框宽度 */
                                    50                                  /* 文本框高度 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            register_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            register_password_box();
                            break;
                        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
                            /* 满足输入条件，允许注册 */
                            register_allow_flags = 1; 
                            register_judgment();
                            return;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            lcd_render_text_with_box(
                                "当前在注册页面，禁止登录！",     /* 文本内容 */
                                340, 400,                       /* 起始坐标 (x, y) */
                                COLOR_WHITE,                    /* 文本颜色 */
                                COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
                                10,                             /* 文本与文本框边缘的间距 */
                                BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */
                                15,                             /* 圆角矩形半径 */
                                30,                             /* 字体大小 */
                                0,                              /* 文本框宽度 */
                                0                               /* 文本框高度 */
                            );
                            sleep(3);
                            register_allow_flags = 0;
                            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                            login_boot();
                            return;
                        }
                    }
                } else {    
                    /* 密码位数小于8或大于12 */
                    lcd_render_text_with_box(
                        "密码不符合要求，请重新输入8-12位密码！",  /* 文本内容 */
                        233, 400,                               /* 起始坐标 (x, y) */
                        COLOR_WHITE,                            /* 文本颜色 */
                        COLOR_LIGHTGRAY,                        /* 文本框背景颜色 */
                        10,                                     /* 文本与文本框边缘的间距 */
                        BOX_STYLE_ROUNDED,                      /* 圆角矩形样式 */
                        15,                                     /* 圆角矩形半径 */
                        30,                                     /* 字体大小 */
                        0,                                      /* 文本框宽度 */
                        0                                       /* 文本框高度 */
                    );
                    sleep(3);
                    register_allow_flags = 0;
                    memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
                    memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                    memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                    register_password_box();
                    return;
                }
                break;
            }

            /* 处理键盘点击事件 */ 
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) {
                strcat(user_info.password_number_buf, " ");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "Q");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "W");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "E");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "R");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "T");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "Y");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "U");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "I");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "O");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "P");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "A");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "S");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "D");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "F");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "G");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "H");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "J");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "K");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "L");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "Z");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "X");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "C");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "V");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "B");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "N");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "M");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) {
                if (strlen(user_info.password_number_buf) > 0) {
                    user_info.password_number_buf[strlen(user_info.password_number_buf) - 1] = '\0';
                }
                if (strlen(user_info.hide_password_number_buf) > 0) {
                    user_info.hide_password_number_buf[strlen(user_info.hide_password_number_buf) - 1] = '\0';
                }
                input_changed = 1;  
            }

            /* 输入标记有变化 */
            if (input_changed) {
                lcd_draw_filled_rectangle(
                    0, 0,                           /* 左上角坐标 (x, y) */
                    1024, 143,                      /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */ 
                );
                lcd_render_text_with_box(
                    user_info.password_number_buf,  /* 文本内容 */
                    70, 51,                         /* 起始坐标 (x, y) */
                    COLOR_BLACK,                    /* 文本颜色 */ 
                    COLOR_WHITE,                    /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                    0,                              /* 矩形样式不需要半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );        
                lcd_render_text_with_box(
                    "确认",                         /* 文本内容 */
                    800, 51,                        /* 起始坐标 (x, y) */
                    COLOR_WHITE,                    /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
                    15,                             /* 圆角矩形半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );
            }
        }
    }

    close(input_fd);
    lcd_cleanup();
}

/* 找回账号界面账号输入功能实现 */
void find_account_account_box()
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 加载背景图层 */
    show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);    
    /* 渲染账号文本框背景 */
    lcd_draw_filled_rectangle(
        0, 0,                           /* 左上角坐标 (x, y) */
        1024, 143,                      /* 矩形宽度和高度 */
        COLOR_WHITE                     /* 填充颜色 */ 
    );
    /* 绘制账号输入文本框 */
    lcd_render_text_with_box(
        user_info.account_number_buf,   /* 文本内容 */
        70, 51,                         /* 起始坐标 (x, y) */
        COLOR_BLACK,                    /* 文本颜色 */ 
        COLOR_WHITE,                    /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
        0,                              /* 矩形样式不需要半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",                         /* 文本内容 */
        800, 51,                        /* 起始坐标 (x, y) */
        COLOR_WHITE,                    /* 文本颜色 */ 
        COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
        15,                             /* 圆角矩形半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 加载键盘 */
    keyboard();    

    /* 打开触摸屏文件 */ 
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;

    while (1) {
        /* 标记输入是否有变化 */ 
        int input_changed = 0;  

        /* 读取触摸屏数据 */ 
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  
                /* 确认按钮和确认键 */

                /* 处理账号输入 */
                if (strlen(user_info.account_number_buf) == 0) {
                    /* 加载背景图层 */
                    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);    
                    /* 渲染账号背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 248,               /* 左上角坐标 (x, y) */
                        386, 50,                /* 矩形宽度和高度 */
                        COLOR_WHITE             /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入要找回的账号",     /* 文本内容 */
                        104, 260,               /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,        /* 文本颜色 */
                        25                      /* 字体大小 */
                    );            
                } else if (strlen(user_info.account_number_buf) <= 12) {
                    /* 账号不超过12位 */
                    /* 加载背景图层 */
                    show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);    
                    /* 渲染账号背景文本框 */ 
                    lcd_render_text_with_box(
                        user_info.account_number_buf,       /* 文本内容 */
                        104, 248,                           /* 起始坐标 (x, y) */
                        COLOR_BLACK,                        /* 文本颜色 */ 
                        COLOR_WHITE,                        /* 文本框背景颜色 */ 
                        0,                                  /* 文本与文本框边缘的间距 */ 
                        BOX_STYLE_RECTANGLE,                /* 矩形样式 */ 
                        0,                                  /* 矩形样式不需要半径 */
                        50,                                 /* 字体大小 */
                        386,                                /* 文本框宽度 */
                        50                                  /* 文本框高度 */
                    );  
                } else {
                    lcd_render_text_with_box(
                        "账号超过12位，请重新输入！",     /* 文本内容 */
                        310, 400,                       /* 起始坐标 (x, y) */
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
                    find_account_allow_flags = 0;
                    find_account_account_box();
                    return;
                }

                /* 密码输入处理 */
                /* 显示或隐藏密码开关 */
                int show_hide_password_flags = 0;
                /* 渲染密码背景文本框 */
                lcd_draw_filled_rectangle(
                    104, 310,                       /* 左上角坐标 (x, y) */
                    386, 50,                        /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */
                );
                /* 空密码时 */
                if (strlen(user_info.password_number_buf) == 0) {
                    lcd_render_text(
                        "请输入要修改的密码(8-12位)", /* 文本内容 */
                        104, 323,                   /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,            /* 文本颜色 */
                        25                          /* 字体大小 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入要修改的密码(8-12位)", /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入要修改的密码(8-12位)", /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            find_account_account_box();
                            return;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            find_account_password_box();
                            return;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            /* 满足所有输入条件，允许找回 */
                            find_account_allow_flags = 1;    
                            find_account_judgment_1();
                            return;
                        }
                    }
                    break;
                }
                /* 非空密码时 */
                if (strlen(user_info.password_number_buf) != 0) {
                    lcd_draw_filled_rectangle(
                        104, 310,                           /* 左上角坐标 (x, y) */
                        386, 50,                            /* 矩形宽度和高度 */
                        COLOR_WHITE                         /* 填充颜色 */
                    );
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf, /* 文本内容 */
                        104, 310,                           /* 起始坐标 (x, y) */
                        COLOR_BLACK,                        /* 文本颜色 */  
                        COLOR_WHITE,                        /* 文本框背景颜色 */
                        0,                                  /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                        0,                                  /* 矩形样式不需要半径 */
                        50,                                 /* 字体大小 */
                        386,                                /* 文本框宽度 */
                        50                                  /* 文本框高度 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (4) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,               /* 左上角坐标 (x, y) */
                                    386, 50,                /* 矩形宽度和高度 */
                                    COLOR_WHITE             /* 填充颜色 */
                                );
                                /* 渲染密码背景文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,  /* 文本内容 */
                                    104, 310,                       /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                    /* 文本颜色 */  
                                    COLOR_WHITE,                    /* 文本框背景颜色 */
                                    0,                              /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */
                                    0,                              /* 矩形样式不需要半径 */
                                    50,                             /* 字体大小 */
                                    386,                            /* 文本框宽度 */
                                    50                              /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                           /* 左上角坐标 (x, y) */
                                    386, 50,                            /* 矩形宽度和高度 */
                                    COLOR_WHITE                         /* 填充颜色 */
                                );
                                /* 渲染密码背景文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf, /* 文本内容 */
                                    104, 310,                           /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                        /* 文本颜色 */  
                                    COLOR_WHITE,                        /* 文本框背景颜色 */
                                    0,                                  /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                                    0,                                  /* 矩形样式不需要半径 */
                                    50,                                 /* 字体大小 */
                                    386,                                /* 文本框宽度 */
                                    50                                  /* 文本框高度 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            find_account_account_box();
                            return;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            find_account_password_box();
                            return;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            /* 满足所有输入条件，允许找回 */
                            find_account_allow_flags = 1;    
                            find_account_judgment_1();
                            return;
                        }
                    }
                }
                break;  
            }

            /* 处理键盘点击事件 */ 
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) {
                strcat(user_info.account_number_buf, " ");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "Q");
                input_changed = 1;  
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "W");
                input_changed = 1;  
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "E");
                input_changed = 1;  
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "R");
                input_changed = 1;  
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "T");
                input_changed = 1;  
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "Y");
                input_changed = 1;  
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "U");
                input_changed = 1;  
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "I");
                input_changed = 1;  
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "O");
                input_changed = 1;  
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.account_number_buf, "P");
                input_changed = 1;  
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "A");
                input_changed = 1;  
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "S");
                input_changed = 1;  
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "D");
                input_changed = 1;  
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "F");
                input_changed = 1;  
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "G");
                input_changed = 1;  
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "H");
                input_changed = 1;  
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "J");
                input_changed = 1;  
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "K");
                input_changed = 1;  
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.account_number_buf, "L");
                input_changed = 1;  
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "Z");
                input_changed = 1;  
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "X");
                input_changed = 1;  
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "C");
                input_changed = 1;  
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "V");
                input_changed = 1;  
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "B");
                input_changed = 1;  
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "N");
                input_changed = 1;  
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.account_number_buf, "M");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) {
                if (strlen(user_info.account_number_buf) > 0) {
                    user_info.account_number_buf[strlen(user_info.account_number_buf) - 1] = '\0';
                }
                input_changed = 1;  
            }

            /* 标记输入变化 */
            if (input_changed) {
                lcd_draw_filled_rectangle(
                    0, 0,                           /* 左上角坐标 (x, y) */
                    1024, 143,                      /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */ 
                );
                lcd_render_text_with_box(
                    user_info.account_number_buf,   /* 文本内容 */
                    70, 51,                         /* 起始坐标 (x, y) */
                    COLOR_BLACK,                    /* 文本颜色 */ 
                    COLOR_WHITE,                    /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                    0,                              /* 矩形样式不需要半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );        
                lcd_render_text_with_box(
                    "确认",                         /* 文本内容 */
                    800, 51,                        /* 起始坐标 (x, y) */
                    COLOR_WHITE,                    /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
                    15,                             /* 圆角矩形半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );
            }
        }
    }

    close(input_fd);
    lcd_cleanup();
}

/* 找回账号界面密码输入功能实现 */
void find_account_password_box()
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 加载背景图层 */
    show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);    
    /* 渲染密码文本框背景 */
    lcd_draw_filled_rectangle(
        0, 0,                           /* 左上角坐标 (x, y) */
        1024, 143,                      /* 矩形宽度和高度 */
        COLOR_WHITE                     /* 填充颜色 */ 
    );
    /* 绘制密码输入文本框 */
    lcd_render_text_with_box(
        user_info.password_number_buf,  /* 文本内容 */
        70, 51,                         /* 起始坐标 (x, y) */
        COLOR_BLACK,                    /* 文本颜色 */ 
        COLOR_WHITE,                    /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
        0,                              /* 矩形样式不需要半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 绘制确认按钮 */
    lcd_render_text_with_box(
        "确认",                         /* 文本内容 */
        800, 51,                        /* 起始坐标 (x, y) */
        COLOR_WHITE,                    /* 文本颜色 */ 
        COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
        0,                              /* 文本与文本框边缘的间距 */ 
        BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
        15,                             /* 圆角矩形半径 */
        60,                             /* 字体大小 */
        0,                              /* 文本框宽度 */
        0                               /* 文本框高度 */
    );
    /* 加载键盘 */
    keyboard();    

    /* 打开触摸屏文件 */ 
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }
    struct input_event input_buf;

    while (1) {
        /* 标记输入是否有变化 */ 
        int input_changed = 0;  

        /* 读取触摸屏数据 */ 
        read(input_fd, &input_buf, sizeof(input_buf));
        if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
            input_x = input_buf.value;
        }
        if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
            input_y = input_buf.value;
        }
        if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
            if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) {  
                /* 确认按钮和确认键 */

                /* 处理账号输入 */
                if (strlen(user_info.account_number_buf) == 0) {
                    if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                        /* 加载背景图层 */
                        show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);    
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,               /* 左上角坐标 (x, y) */
                            386, 50,                /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                        lcd_render_text(
                            "请输入要找回的账号",     /* 文本内容 */
                            104, 260,               /* 起始坐标 (x, y) */
                            COLOR_LIGHTGRAY,        /* 文本颜色 */
                            25                      /* 字体大小 */
                        );
                    } else {
                        /* 密码不符合条件，屏幕开始提示，不显示图层 */
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            0, 0,                   /* 左上角坐标 (x, y) */
                            1, 1,                   /* 矩形宽度和高度 */
                            COLOR_WHITE             /* 填充颜色 */
                        );
                    }           
                } else {
                    if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                        /* 加载背景图层 */
                        show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);    
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,                       /* 左上角坐标 (x, y) */
                            386, 50,                        /* 矩形宽度和高度 */
                            COLOR_WHITE                     /* 填充颜色 */
                        );
                        /* 账号输入文本框 */ 
                        lcd_render_text_with_box(
                            user_info.account_number_buf,   /* 文本内容 */
                            104, 248,                       /* 起始坐标 (x, y) */
                            COLOR_BLACK,                    /* 文本颜色 */ 
                            COLOR_WHITE,                    /* 文本框背景颜色 */ 
                            0,                              /* 文本与文本框边缘的间距 */ 
                            BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                            0,                              /* 矩形样式不需要半径 */
                            50,                             /* 字体大小 */
                            386,                            /* 文本框宽度 */
                            50                              /* 文本框高度 */
                        );
                    } else {
                        /* 密码不符合条件，屏幕提示，不显示图层 */
                        //show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            0, 0,                           /* 左上角坐标 (x, y) */
                            1, 1,                           /* 矩形宽度和高度 */
                            COLOR_WHITE                     /* 填充颜色 */
                        );
                    }
                }

                /* 处理密码输入 */
                /* 显示或隐藏密码开关 */
                int show_hide_password_flags = 0;
                /* 空密码时 */
                if (strlen(user_info.password_number_buf) == 0) {
                    /* 加载背景图层 */
                    show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);    
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,                       /* 左上角坐标 (x, y) */
                        386, 50,                        /* 矩形宽度和高度 */
                        COLOR_WHITE                     /* 填充颜色 */
                    );
                    lcd_render_text(
                        "请输入要修改的密码(8-12位)",     /* 文本内容 */
                        104, 323,                       /* 起始坐标 (x, y) */
                        COLOR_LIGHTGRAY,                /* 文本颜色 */
                        25                              /* 字体大小 */
                    );
                    /* 单独账号渲染,防止界面紊乱 */
                    if (strlen(user_info.account_number_buf) == 0) {
                        /* 账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,                   /* 左上角坐标 (x, y) */
                            386, 50,                    /* 矩形宽度和高度 */
                            COLOR_WHITE                 /* 填充颜色 */
                        );
                        lcd_render_text(
                            "请输入要找回的账号",         /* 文本内容 */
                            104, 260,                   /* 起始坐标 (x, y) */
                            COLOR_LIGHTGRAY,            /* 文本颜色 */
                            25                          /* 字体大小 */
                        );
                    } else {
                        /* 渲染账号背景文本框 */
                        lcd_draw_filled_rectangle(
                            104, 248,       /* 左上角坐标 (x, y) */
                            386, 50,        /* 矩形宽度和高度 */
                            COLOR_WHITE     /* 填充颜色 */
                        );
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

                    /* 默认隐藏密码 */
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (2) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入要修改的密码(8-12位)", /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                lcd_draw_filled_rectangle(
                                    104, 310,                   /* 左上角坐标 (x, y) */
                                    386, 50,                    /* 矩形宽度和高度 */
                                    COLOR_WHITE                 /* 填充颜色 */
                                );
                                lcd_render_text(
                                    "请输入要修改的密码(8-12位)", /* 文本内容 */
                                    104, 323,                   /* 起始坐标 (x, y) */
                                    COLOR_LIGHTGRAY,            /* 文本颜色 */
                                    25                          /* 字体大小 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            find_account_account_box();
                            return;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            find_account_password_box();
                            return;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            /* 空账号与密码强制不允许找回 */
                            find_account_allow_flags = 0;    
                            find_account_judgment_1();
                            return;
                        }
                    }
                    break;
                }
                /* 非空密码时 密码位数：8-12 */
                if (strlen(user_info.password_number_buf) >= 8 && strlen(user_info.password_number_buf) <= 12) {
                    /* 密码背景文本框 */
                    lcd_draw_filled_rectangle(
                        104, 310,                           /* 左上角坐标 (x, y) */
                        386, 50,                            /* 矩形宽度和高度 */
                        COLOR_WHITE                         /* 填充颜色 */
                    );
                    /* 密码输入文本框 */
                    lcd_render_text_with_box(
                        user_info.hide_password_number_buf, /* 文本内容 */
                        104, 310,                           /* 起始坐标 (x, y) */
                        COLOR_BLACK,                        /* 文本颜色 */  
                        COLOR_WHITE,                        /* 文本框背景颜色 */
                        0,                                  /* 文本与文本框边缘的间距 */
                        BOX_STYLE_RECTANGLE,                /* 矩形样式 */
                        0,                                  /* 矩形样式不需要半径 */
                        50,                                 /* 字体大小 */
                        386,                                /* 文本框宽度 */
                        50                                  /* 文本框高度 */
                    );
                    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                    while (3) {
                        ts_fun();
                        if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
                            /* 切换显示隐藏密码开关 */ 
                            show_hide_password_flags = !show_hide_password_flags;
                            if (show_hide_password_flags) {
                                /* 显示密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                       /* 左上角坐标 (x, y) */
                                    386, 50,                        /* 矩形宽度和高度 */
                                    COLOR_WHITE                     /* 填充颜色 */
                                );
                                /* 渲染密码背景文本框 */
                                lcd_render_text_with_box(
                                    user_info.password_number_buf,  /* 文本内容 */
                                    104, 310,                       /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                    /* 文本颜色 */  
                                    COLOR_WHITE,                    /* 文本框背景颜色 */
                                    0,                              /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */
                                    0,                              /* 矩形样式不需要半径 */
                                    50,                             /* 字体大小 */
                                    386,                            /* 文本框宽度 */
                                    50                              /* 文本框高度 */
                                );
                                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
                            } else {
                                /* 隐藏密码 */
                                lcd_draw_filled_rectangle(
                                    104, 310,                           /* 左上角坐标 (x, y) */
                                    386, 50,                            /* 矩形宽度和高度 */
                                    COLOR_WHITE                         /* 填充颜色 */
                                );
                                /* 渲染密码背景文本框 */
                                lcd_render_text_with_box(
                                    user_info.hide_password_number_buf,  /* 文本内容 */
                                    104, 310,                            /* 起始坐标 (x, y) */
                                    COLOR_BLACK,                         /* 文本颜色 */  
                                    COLOR_WHITE,                         /* 文本框背景颜色 */
                                    0,                                   /* 文本与文本框边缘的间距 */
                                    BOX_STYLE_RECTANGLE,                 /* 矩形样式 */
                                    0,                                   /* 矩形样式不需要半径 */
                                    50,                                  /* 字体大小 */
                                    386,                                 /* 文本框宽度 */
                                    50                                   /* 文本框高度 */
                                );
                                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
                            }
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                            find_account_account_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 360) {
                            find_account_password_box();
                            break;
                        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
                            /* 满足输入条件，允许找回 */
                            find_account_allow_flags = 1;    
                            find_account_judgment_1();
                            return;
                        }
                    }
                } else {
                    /* 密码位数小于8或大于12 */
                    lcd_render_text_with_box(
                        "密码不符合要求，请重新输入8-12位密码！",   /* 文本内容 */
                        233, 400,                               /* 起始坐标 (x, y) */
                        COLOR_WHITE,                            /* 文本颜色 */
                        COLOR_LIGHTGRAY,                        /* 文本框背景颜色 */
                        10,                                     /* 文本与文本框边缘的间距 */
                        BOX_STYLE_ROUNDED,                      /* 圆角矩形样式 */
                        15,                                     /* 圆角矩形半径 */
                        30,                                     /* 字体大小 */
                        0,                                      /* 文本框宽度 */
                        0                                       /* 文本框高度 */
                    );
                    sleep(3);
                    find_account_allow_flags = 0;
                    memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
                    memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
                    find_account_password_box();
                    return;
                }
                break;
            }

            /* 处理键盘点击事件 */ 
            if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) {
                strcat(user_info.password_number_buf, " ");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "Q");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "W");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "E");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "R");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "T");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "Y");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "U");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "I");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "O");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) {
                strcat(user_info.password_number_buf, "P");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "A");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "S");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "D");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "F");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "G");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "H");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "J");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "K");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) {
                strcat(user_info.password_number_buf, "L");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "Z");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "X");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "C");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "V");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "B");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "N");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) {
                strcat(user_info.password_number_buf, "M");
                strcat(user_info.hide_password_number_buf, "*");
                input_changed = 1;  
            }
            if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) {
                if (strlen(user_info.password_number_buf) > 0) {
                    user_info.password_number_buf[strlen(user_info.password_number_buf) - 1] = '\0';
                }
                if (strlen(user_info.hide_password_number_buf) > 0) {
                    user_info.hide_password_number_buf[strlen(user_info.hide_password_number_buf) - 1] = '\0';
                }
                input_changed = 1;  
            }

            /* 标记输入变化 */
            if (input_changed) {
                lcd_draw_filled_rectangle(
                    0, 0,                           /* 左上角坐标 (x, y) */
                    1024, 143,                      /* 矩形宽度和高度 */
                    COLOR_WHITE                     /* 填充颜色 */ 
                );
                lcd_render_text_with_box(
                    user_info.password_number_buf,  /* 文本内容 */
                    70, 51,                     /* 起始坐标 (x, y) */
                    COLOR_BLACK,                    /* 文本颜色 */ 
                    COLOR_WHITE,                    /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                    0,                              /* 矩形样式不需要半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );        
                lcd_render_text_with_box(
                    "确认",                         /* 文本内容 */
                    800, 51,                        /* 起始坐标 (x, y) */
                    COLOR_WHITE,                    /* 文本颜色 */ 
                    COLOR_LIGHTGRAY,                /* 文本框背景颜色 */ 
                    0,                              /* 文本与文本框边缘的间距 */ 
                    BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */ 
                    15,                             /* 圆角矩形半径 */
                    60,                             /* 字体大小 */
                    0,                              /* 文本框宽度 */
                    0                               /* 文本框高度 */
                );
            }
        }
    }

    close(input_fd);
    lcd_cleanup();
}

/* 生成6位大写字母验证码 */ 
void generate_random_code(char *code) {
    for (int i = 0; i < 6; i++) {
        /* 'A' + rand() % 26，得到一个A到Z之间的大写字母 */
        code[i] = 'A' + rand() % 26;
    }
    /* 第7个位置添加字符串结束符 */
    code[6] = '\0';
}

/* 更新倒计时显示 */ 
void update_countdown_display() {
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }
    
    /* 检查验证码按钮可见，解决图层覆盖问题 */
    if (code_button_visible) {
        /* 按钮被点击并且处于倒计时状态 */
        if (code_button_clicked && countdown > 0) {
            /* 倒计时字符串，用于展示倒计时 */
            char countdown_str[12];
            sprintf(countdown_str, "%6ds", countdown); /* 格式化字符串，时倒计时与“获取验证码”对齐 */ 
            lcd_render_text_with_box(
                countdown_str,      /* 文本内容 */
                355, 260,           /* 起始坐标 (x, y) */
                COLOR_WHITE,        /* 文本颜色 */ 
                COLOR_LIGHTGRAY,    /* 文本框背景颜色 */ 
                10,                 /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_ROUNDED,  /* 圆角矩形样式 */ 
                13,                 /* 圆角矩形半径 */
                24,                 /* 字体大小 */
                142,                /* 文本框宽度 */
                46                  /* 文本框高度 */
            );
        } else {
            /* 按钮未被点击或不在倒计时状态 */
            lcd_render_text_with_box(
                "获取验证码",        /* 文本内容 */
                355, 260,           /* 起始坐标 (x, y) */
                COLOR_WHITE,        /* 文本颜色 */ 
                COLOR_LIGHTGRAY,    /* 文本框背景颜色 */ 
                10,                 /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_ROUNDED,  /* 圆角矩形样式 */ 
                13,                 /* 圆角矩形半径 */
                24,                 /* 字体大小 */
                142,                /* 文本框宽度 */
                46                  /* 文本框高度 */
            );
        }
    }

    /* 不要清理资源，会阻塞线程 */
    //lcd_cleanup();
}

/* 停止验证码线程 */ 
void stop_code_thread() {
    if (thread_running) {
        /* 尝试强制取消线程 */ 
        pthread_cancel(code_thread);
        /* 安全等待线程结束 */ 
        pthread_join(code_thread, NULL);
        /* 更新线程运行状态 */ 
        thread_running = 0;
        /* 重置倒计时和按钮状态 */ 
        countdown = 0;
        code_button_clicked = 0;
        code_button_visible = 1;
    }
}

/* 验证码发送线程函数 */ 
void *send_verification_code(void *arg) {
    /* 标记线程开始启动 */
    thread_running = 1;

    /* 
    * srand与rank
    * rank:生成伪随机整数 返回值:int 每次需要随机数时调用
    * srand:设置随机数种子 返回值:void 仅在程序开始时调用一次
    * 
    * srand必须在调用rand之前使用，否则将使用默认种子，
    * time(NULL)作为种子，srand()用于初始化随机数种子，配合rand()生成随机数。
    */
    srand(time(NULL));
    while (1) {
        /* 检查验证码按钮可见，但未被点击 */
        if (code_button_visible && !code_button_clicked) {
            /* 更新验证码按钮为“获取验证码” */
            update_countdown_display();
            ts_fun();
            if (input_x >= 334 && input_x <= 490 && input_y >= 250 && input_y <= 300) {
                /* 点击到了获取验证码按钮 */
                code_button_clicked = 1;    /* 标记验证码按钮已被点击 */
                start_time = time(NULL);    /* 记录开始时间，start_time用于存储时间戳 */ 
                countdown = 60;             /* 重置倒计时位60s,即从“获取验证码”变成“60”的字样 */ 

                generate_random_code(code); /* 生成随机验证码，并将结果存储在code数组中 */
                printf("收到的验证码是: %s\n", code);
                
                /* 使用系统时间计算剩余时间，避免sleep累积误差 */ 
                while (code_button_clicked) {
                    /* 按钮一直处于被点击（倒计时状态） */
                    time_t current_time = time(NULL);
                    /* 计算从按钮点击开始到当前时间经过的秒数 */
                    int elapsed_time = (int)(current_time - start_time);
                    /* 计算剩余倒计时秒数 */
                    countdown = 60 - elapsed_time;
                    /* 如果剩余倒计时秒数小于等于 0，说明倒计时结束 */
                    if (countdown <= 0) {
                        countdown = 0;              /* 将倒计时变成0 */
                        code_button_clicked = 0;    /* 重新获取验证码 */
                    }
                    /* 刷新倒计时显示 */
                    update_countdown_display();
                    /* 线程休眠1s */
                    sleep(1);
                }
            }
        }
        /* 检查是否需要退出线程，如果thread_running为0则退出循环 */
        if (!thread_running) break;
        usleep(100000); /* 每100ms检查一次 */ 
    }
    return NULL;
}

/* 找回界面验证码输入功能实现 */
void find_account_verification() {
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 强制初始化线程，避免后面在调用时引发段错误 */
    stop_code_thread();

    /* 加载图层 */
    show_bmp_to_lcd("get_verification_code.bmp", 0, 0, 1024, 600);

    /* 账号背景文本框 */
    lcd_draw_filled_rectangle(
        104, 248,               /* 左上角坐标 (x, y) */
        386, 50,                /* 矩形宽度和高度 */
        COLOR_WHITE             /* 填充颜色 */
    );
    lcd_render_text(
        "请输入六位数验证码",     /* 文本内容 */
        104, 260,               /* 起始坐标 (x, y) */
        COLOR_LIGHTGRAY,        /* 文本颜色 */
        25                      /* 字体大小 */
    );

    /* 创建新的验证码发送线程 */ 
    if (pthread_create(&code_thread, NULL, send_verification_code, NULL) != 0) {
        perror("Failed to create code thread.\n");
        return;
    }

    while (1) {
        ts_fun();
        if (input_x >= 104 && input_x <= 225 && input_y >= 437 && input_y <= 484) {
            /* 点击到修改密码按钮 */ 
            /* 强制初始化线程，避免后面在调用时引发段错误 */
            stop_code_thread();
            find_account_allow_flags = 1;
            find_account_judgment_2();
            return;
        }

        /* 点击到输入验证码文本框 */
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            /* 隐藏验证码按钮 */
            code_button_visible = 0;
            /* 加载键盘 */
            keyboard();    

            /* 打开触摸屏文件 */ 
            int input_fd = open("/dev/input/event1", O_RDWR);
            if (input_fd == -1) {
                perror("Failed to open touchscreen device.\n");
                close(input_fd);
                continue;
            }

            /* 渲染验证码文本框背景 */
            lcd_draw_filled_rectangle(
                0, 0,                               /* 左上角坐标 (x, y) */
                1024, 143,                          /* 矩形宽度和高度 */
                COLOR_WHITE                         /* 填充颜色 */ 
            );
            lcd_render_text_with_box(
                user_info.verification_code_buf,    /* 文本内容 */
                70, 51,                             /* 起始坐标 (x, y) */
                COLOR_BLACK,                        /* 文本颜色 */ 
                COLOR_WHITE,                        /* 文本框背景颜色 */ 
                0,                                  /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_RECTANGLE,                /* 矩形样式 */ 
                0,                                  /* 矩形样式不需要半径 */
                60,                                 /* 字体大小 */
                0,                                  /* 文本框宽度 */
                0                                   /* 文本框高度 */
            );
            /* 绘制确认按钮 */
            lcd_render_text_with_box(
                "确认",                             /* 文本内容 */
                800, 51,                            /* 起始坐标 (x, y) */
                COLOR_WHITE,                        /* 文本颜色 */ 
                COLOR_LIGHTGRAY,                    /* 文本框背景颜色 */ 
                0,                                  /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_ROUNDED,                  /* 圆角矩形样式 */ 
                15,                                 /* 圆角矩形半径 */
                60,                                 /* 字体大小 */
                0,                                  /* 文本框宽度 */
                0                                   /* 文本框高度 */
            );

            /* 打开触摸屏文件 */
            struct input_event input_buf;
            while (1) {
                /* 标记输入是否有变化 */ 
                int input_changed = 0; 

                /* 读取触摸屏数据 */ 
                read(input_fd, &input_buf, sizeof(input_buf));
                if (input_buf.type == EV_ABS && input_buf.code == ABS_X) {
                    input_x = input_buf.value;
                }
                if (input_buf.type == EV_ABS && input_buf.code == ABS_Y) {
                    input_y = input_buf.value;
                }
                if (input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0) {
                    if ((input_x >= 800 && input_x <= 900 && input_y >= 51 && input_y <= 111) || 
                        (input_x >= 876 && input_x <= 1024 && input_y >= 400 && input_y <= 600)) { 
                        /* 处理确认按钮点击事件 */ 

                        /* 处理验证码输入 */
                        /* 若验证码为空 */
                        if (strlen(user_info.verification_code_buf) == 0) {
                            /* 验证码长度不为6 */
                            lcd_render_text_with_box(
                                "验证码为空，请重新获取验证码！",  /* 文本内容 */
                                290, 400,                       /* 起始坐标 (x, y) */
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
                            find_account_allow_flags = 0;
                            memset(user_info.verification_code_buf, 0, sizeof(user_info.verification_code_buf));
                            memset(code, 0, sizeof(code));
                            show_bmp_to_lcd("get_verification_code.bmp", 0, 0, 1024, 600);
                            /* 验证码背景文本框 */
                            lcd_draw_filled_rectangle(
                                104, 248, 
                                386, 50, 
                                COLOR_WHITE
                            );
                            lcd_render_text(
                                "请输入六位数验证码", 
                                104, 260, 
                                COLOR_LIGHTGRAY, 
                                25
                            );
                            stop_code_thread();
                            find_account_verification();
                            return;       
                        } else if (strlen(user_info.verification_code_buf) == 6) {
                            /* 验证码的长度是6 */
                            show_bmp_to_lcd("get_verification_code.bmp", 0, 0, 1024, 600);    
                            /* 渲染验证码背景文本框 */ 
                            lcd_render_text_with_box(
                                user_info.verification_code_buf,     /* 文本内容 */
                                104, 248,                            /* 起始坐标 (x, y) */
                                COLOR_BLACK,                         /* 文本颜色 */ 
                                COLOR_WHITE,                         /* 文本框背景颜色 */ 
                                0,                                   /* 文本与文本框边缘的间距 */ 
                                BOX_STYLE_RECTANGLE,                 /* 矩形样式 */ 
                                0,                                   /* 矩形样式不需要半径 */
                                50,                                  /* 字体大小 */
                                386,                                 /* 文本框宽度 */
                                50                                   /* 文本框高度 */
                            );
                            /* 点击到修改密码 */
                            if (input_x >= 104 && input_x <= 225 && input_y >= 437 && input_y <= 484) {
                                find_account_allow_flags = 1;
                                /* 强制初始化线程，避免后面在调用时引发段错误 */
                                stop_code_thread();
                                find_account_judgment_2();
                                return;
                            }
                        } else {
                            /* 验证码长度不为6 */
                            lcd_render_text_with_box(
                                "验证码长度不为6，请重新输入",     /* 文本内容 */
                                310, 400,                       /* 起始坐标 (x, y) */
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
                            find_account_allow_flags = 0;
                            memset(user_info.verification_code_buf, 0, sizeof(user_info.verification_code_buf));
                            memset(code, 0, sizeof(code));
                            show_bmp_to_lcd("get_verification_code.bmp", 0, 0, 1024, 600);
                            /* 渲染验证码背景文本框 */
                            lcd_draw_filled_rectangle(
                                104, 248, 
                                386, 50, 
                                COLOR_WHITE
                            );
                            lcd_render_text(
                                "请输入六位数验证码", 
                                104, 260, 
                                COLOR_LIGHTGRAY, 
                                25
                            );
                            stop_code_thread();
                            find_account_verification();
                            return;
                        }
                        /* 防止输入到正确六位数密码后不显示验证码按钮 */
                        code_button_visible = 1;        /* 验证码按钮可见标记 */
                        update_countdown_display();     /* 更新验证码按钮为可见状态 */
                        break;
                    }

                    /* 处理键盘点击事件 */ 
                    if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) {
                        strcat(user_info.verification_code_buf, " ");
                        input_changed = 1;  
                    }
                    if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "Q");
                        input_changed = 1;  
                    }
                    if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "W");
                        input_changed = 1;  
                    }
                    if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "E");
                        input_changed = 1;  
                    }
                    if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "R");
                        input_changed = 1;  
                    }
                    if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "T");
                        input_changed = 1;  
                    }
                    if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "Y");
                        input_changed = 1;  
                    }
                    if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "U");
                        input_changed = 1;  
                    }
                    if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "I");
                        input_changed = 1;  
                    }
                    if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "O");
                        input_changed = 1;  
                    }
                    if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) {
                        strcat(user_info.verification_code_buf, "P");
                        input_changed = 1;  
                    }
                    if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "A");
                        input_changed = 1;  
                    }
                    if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "S");
                        input_changed = 1;  
                    }
                    if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "D");
                        input_changed = 1;  
                    }
                    if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "F");
                        input_changed = 1;  
                    }
                    if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "G");
                        input_changed = 1;  
                    }
                    if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "H");
                        input_changed = 1;  
                    }
                    if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "J");
                        input_changed = 1;  
                    }
                    if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "K");
                        input_changed = 1;  
                    }
                    if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) {
                        strcat(user_info.verification_code_buf, "L");
                        input_changed = 1;  
                    }
                    if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "Z");
                        input_changed = 1;  
                    }
                    if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "X");
                        input_changed = 1;  
                    }
                    if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "C");
                        input_changed = 1;  
                    }
                    if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "V");
                        input_changed = 1;  
                    }
                    if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "B");
                        input_changed = 1;  
                    }
                    if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "N");
                        input_changed = 1;  
                    }
                    if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) {
                        strcat(user_info.verification_code_buf, "M");
                        input_changed = 1;  
                    }
                    if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) {
                        if (strlen(user_info.verification_code_buf) > 0) {
                            user_info.verification_code_buf[strlen(user_info.verification_code_buf) - 1] = '\0';
                        }
                        input_changed = 1;  
                    }

                    /* 标记输入有变化 */
                    if (input_changed) {
                        lcd_draw_filled_rectangle(
                            0, 0,                               /* 左上角坐标 (x, y) */
                            1024, 143,                          /* 矩形宽度和高度 */
                            COLOR_WHITE                         /* 填充颜色 */ 
                        );
                        lcd_render_text_with_box(
                            user_info.verification_code_buf,    /* 文本内容 */
                            70, 51,                             /* 起始坐标 (x, y) */
                            COLOR_BLACK,                        /* 文本颜色 */ 
                            COLOR_WHITE,                        /* 文本框背景颜色 */ 
                            0,                                  /* 文本与文本框边缘的间距 */ 
                            BOX_STYLE_RECTANGLE,                /* 矩形样式 */ 
                            0,                                  /* 矩形样式不需要半径 */
                            60,                                 /* 字体大小 */
                            0,                                  /* 文本框宽度 */
                            0                                   /* 文本框高度 */
                        );        
                        lcd_render_text_with_box(
                            "确认",                             /* 文本内容 */
                            800, 51,                            /* 起始坐标 (x, y) */
                            COLOR_WHITE,                        /* 文本颜色 */ 
                            COLOR_LIGHTGRAY,                    /* 文本框背景颜色 */ 
                            0,                                  /* 文本与文本框边缘的间距 */ 
                            BOX_STYLE_ROUNDED,                  /* 圆角矩形样式 */ 
                            15,                                 /* 圆角矩形半径 */
                            60,                                 /* 字体大小 */
                            0,                                  /* 文本框宽度 */
                            0                                   /* 文本框高度 */
                        );
                    }
                }
            }
            close(input_fd);
        }
    }

    /* 清理lcd资源 */
    lcd_cleanup();
}

/* 开机引导界面 */
void boot()
{
    //show_gif_to_lcd("boot.gif", 0, 0, 1024, 600, 3);
    show_bmp_to_lcd("boot_logo.bmp", 0, 0, 1024, 600);
    sleep(3);
    //show_gif_to_lcd("boot_am.gif", 0, 0, 1024, 600, 15);
    return;

}

/* 桌面功能实现 */
void home_fun() {
    /* 重置标志位 */ 
    login_allow_flags = 0;  
    skip_login_boot_flags = 0;
    register_allow_flags = 0;
    skip_register_boot_flags = 0;
    find_account_allow_flags = 0;
    skip_find_account_boot_flags = 0;
    scratched_count = 0;
    threshold_reached = 0;
    stop_touch = 1;

    /* 显示桌面 */  
    show_bmp_to_lcd("home.bmp", 0, 0, 1024, 600);
    while(1) {
        ts_fun();
        if (input_x >= 418 && input_x <= 494 && input_y >= 259 && input_y <= 331) {
            show_bmp_to_lcd("logo.bmp", 0, 0, 1024, 600);
            sleep(1);
            //show_gif_to_lcd("game_logo.gif", 0, 0, 1024, 600, 25);
            sleep(1);
            show_bmp_to_lcd("anti_addiction.bmp", 0, 0, 1024, 600);
            usleep(1000000);
            show_bmp_to_lcd("login_entry.bmp", 0, 0, 1024, 600);
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

/* 登录初始化界面功能实现 */
void login_fun() {
    /* 初始选择登录界面 */ 
    while(1) {
        ts_fun();
        if (input_x >= 399 && input_x <= 621 && input_y >= 385 && input_y <= 490) {
            /* 进入登录引导界面 */
            login_boot();
            return; /* 强制退出函数，防死循环 */
        } else if (input_x >= 942 && input_x <= 1024 && input_y >= 524 && input_y <= 600) {
            /* 点击到关闭按钮 */
            show_bmp_to_lcd("login_exit.bmp", 0, 0, 1024, 600);
            while(2) {
                ts_fun();
                if (input_x >= 313 && input_x <= 497 && input_y >= 363 && input_y <= 447) {
                    home_fun();
                    break;
                } else if (input_x >= 534 && input_x <= 714 && input_y >= 363 && input_y <= 447) {
                    /* 刷新屏幕 */  
                    show_bmp_to_lcd("login_entry.bmp", 0, 0, 1024, 600); 
                    login_fun();
                    return; /* 强制退出函数，防死循环 */
                }
            }
        }
    }
}

/* 登录界面引导 */
void login_boot() 
{
    /* 进入到登录界面 */
    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);
    account_password_background_box();

    /* 标志变量，0 表示隐藏密码，1 表示显示密码，默认隐藏 */ 
    int show_hide_password_flags = 0; 
    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);

    while (1) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            /* 点击到输入账号文本框 */ 
            input_account_box();    
            return;
        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 400) {
            /* 点击到输入密码文本框 */
            input_password_box();   
            return;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
            /* 首次空账号密码登录，跳转登陆判断 */
            login_judgment();       
            return;
        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
            /* 点击到注册账号按钮 */
            register_background_box();  
            register_boot();
            return;
        } else if (input_x >= 370 && input_x <= 445 && input_y >= 375 && input_y <= 404) {
            /* 点击忘记密码 */
            find_account_background_box();  
            find_account_boot();
            return;
        } else if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
            show_hide_password_flags = !show_hide_password_flags; 
            if (show_hide_password_flags) {
                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
            } else {
                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
            }
        }

        /* 强制结束登陆界面引导 */
        if (skip_login_boot_flags == 1) {
            return;
        }
    }
}

/* 注册界面引导 */
void register_boot() 
{   
    /* 进入到注册界面 */
    show_bmp_to_lcd("login_initialization.bmp", 0, 0, 1024, 600);
    register_background_box();

    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 标志变量，0 表示隐藏密码，1 表示显示密码，默认隐藏 */ 
    int show_hide_password_flags = 0; 
    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);

    while (1) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            /* 点击到输入账号文本框 */ 
            register_account_box();    
            return;
        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 400) {
            /* 点击到输入密码文本框 */
            register_password_box();   
            return;
        } else if (input_x >= 104 && input_x <= 248 && input_y >= 400 && input_y <= 500) {
            lcd_render_text_with_box(
                "当前在注册页面，禁止登录！",      /* 文本内容 */
                340, 400,                       /* 起始坐标 (x, y) */
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
            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
            login_boot();
            return;
        } else if (input_x >= 250 && input_x <= 361 && input_y >= 424 && input_y <= 484) {
            register_judgment();
            return;
        } else if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
            show_hide_password_flags = !show_hide_password_flags; 
            if (show_hide_password_flags) {
                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
            } else {
                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
            }
        } 

        /* 强制结束注册界面引导 */
        if (skip_register_boot_flags == 1) {
            return;
        }
    }

    /* 清理资源 */ 
    lcd_cleanup();
}

/* 账号找回引导 */
void find_account_boot()
{
    /* 界面渲染 */
    show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);
    find_account_background_box();

    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 标志变量，0 表示隐藏密码，1 表示显示密码，默认隐藏 */ 
    int show_hide_password_flags = 0; 
    show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);

    while (1) {
        ts_fun();
        if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
            /* 点击到输入账号文本框 */
            find_account_account_box();     
            return;
        } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 380) {
            /* 点击到输入密码文本框 */
            find_account_password_box();   
            return;
        } else if (input_x >= 100 && input_x <= 218 && input_y >= 424 && input_y <= 500) {
            /* 点击到下一步 */
            find_account_judgment_1();     
            return;
        } else if (input_x >= 450 && input_x <= 495 && input_y >= 322 && input_y <= 360) {
            show_hide_password_flags = !show_hide_password_flags; 
            if (show_hide_password_flags) {
                show_bmp_to_lcd("show_password.bmp", 448, 315, 40, 40);
            } else {
                show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
            }
        } 

        /* 强制结束账号找回引导 */
        if (skip_find_account_boot_flags == 1) {
            return;
        }
    }
    
    /* 清理资源 */ 
    lcd_cleanup();
}

/* 登录判断 */ 
void login_judgment() {
    /* 初始化字库 */ 
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 临时存储登录信息 */ 
    struct {
        char account_number_buf[128];      /* 存储的账号 */
        char password_number_buf[128];     /* 存储的密码 */
        char account_status[20];           /* 账户状态：lock/unlock */
        int error_count;                   /* 密码错误次数 */
    } login_user_info = {{0}, {0}, {0}, 0};
    
    /* 存放用户输入的账号（用于判断是否存在该用户） */
    char temp_account_number_buf[128] = {0};
    /* 存放拼接后的账号 */
    sprintf(temp_account_number_buf, "%s.txt", user_info.account_number_buf);
    /* 尝试打开文档（账号），若账号不存在，返回值-1 */
    int user_data_fd = open(temp_account_number_buf, O_RDONLY);
    if (user_data_fd == -1) {
        /* 账号不存在处理 */
        lcd_render_text_with_box(
            "该账号不存在，请先注册!", 
            340, 400, 
            COLOR_WHITE, 
            COLOR_LIGHTGRAY, 
            10, 
            BOX_STYLE_ROUNDED, 
            15, 
            30, 
            0, 
            0
        );
        sleep(3);
        memset(&user_info, 0, sizeof(user_info));
        memset(temp_account_number_buf, 0, sizeof(temp_account_number_buf));
        login_boot();
        return;
    }
    
    /* 读取账户信息 */
    char user_data_buf[1024] = {0};
    /*  
    * read(int fd, void *buf, size_t count);
    * conut指定最多读取的字节数，最多读取128-1，预留一个字节给字符串结束符'\0',
    * 确保后续能把user_data_buf当作字符串处理。
    * 
    * ssize_t:read函数返回一个ssize_t类型的值bytes_read，用于表示实际读取的字节数。
    */
    ssize_t bytes_read = read(user_data_fd, user_data_buf, sizeof(user_data_buf) - 1);
    close(user_data_fd);
    
    /* 解析user_data_buf的数据 */
    if (bytes_read > 0) {
        /* 解析第一行：account:A,password:AAAAAAAA 以换行符进行分割 */ 
        char *line1 = strtok(user_data_buf, "\n");  
        if (line1) {
            /* 
            * 查找账号和密码 
            * char *strstr(const char *haystack, const char *needle);
            * haystack：要搜索的字符串。needle：要查找的子字符串。
            */ 
            char *account_ptr = strstr(line1, "account:");
            char *password_ptr = strstr(line1, "password:");
            
            if (account_ptr && password_ptr) {
                /* account_ptr与password_ptr皆不为NULL，即找到时 */
                /* 偏移到实际数据位置account:^XXXXX,password:^XXXXX ^:偏移到这个位置 */ 
                account_ptr += strlen("account:");
                password_ptr += strlen("password:");
                
                /* 提取账号 */ 
                char *comma = strchr(account_ptr, ',');
                if (comma) {
                    /* 
                    * 例如comma找到的位置是account:XXXXX^,
                    * 例如account找到的位置是account:^XXXXX,
                    * 那么账号就是comma-account_ptr,即XXXXX=len。
                    */
                    size_t len = comma - account_ptr;
                    /* 
                    * strncpy将account_ptr开始的len个字符，
                    * 复制到login_user_info.account_number_buf中。
                    * 
                    * 由于strncpy不会自动添加字符串结束符'\0'，
                    * login_user_info.account_number_buf[len]位置添加'\0'。
                    */
                    strncpy(login_user_info.account_number_buf, account_ptr, len);
                    login_user_info.account_number_buf[len] = '\0'; /* 手动添加字符串结束符 */ 
                } else {
                    /* 
                    * 处理没有逗号的情况
                    * strncpy将account_ptr开始的最多sizeof(login_user_info.account_number_buf)-1个字符，
                    * 复制到login_user_info.account_number_buf中。
                    */ 
                    strncpy(login_user_info.account_number_buf, account_ptr, sizeof(login_user_info.account_number_buf) - 1);
                    login_user_info.account_number_buf[sizeof(login_user_info.account_number_buf) - 1] = '\0';
                }
                
                /* 提取密码（直到行尾） */ 
                strncpy(login_user_info.password_number_buf, password_ptr, sizeof(login_user_info.password_number_buf) - 1);
                login_user_info.password_number_buf[sizeof(login_user_info.password_number_buf) - 1] = '\0';
                
                /* 解析第二行 用户状态以及密码输入错误次数 */ 
                char *line2 = strtok(NULL, "\n");
                if (line2) {
                    /* 解析状态和错误次数 例：account_state:lock,error_count:3 */ 
                    char state_prefix[20] = {0};    /* 存储账户状态的前缀 */
                    char error_prefix[20] = {0};    /* 存储错误次数的前缀 */
                    char state_value[10] = {0};     /* 存储账户状态的值 */
                    
                    if (sscanf(line2, "%19[^:]:%9[^,],%19[^:]:%d", 
                        state_prefix, state_value, error_prefix, &login_user_info.error_count) >= 3) {
                        /* 
                        * int sscanf(const char *str, const char *format, ...);
                        * str：要读取数据的源字符串。 format：格式控制字符串，用于指定读取数据的格式。
                        *
                        * sscanf：从字符串line2中按照指定格式读取数据。
                        * %19[^:]:  读取最多19个非:字符到state_prefix中，遇到:停止。 验证字段是否相等
                        * [^:]表示读取除:之外的任意字符，直到遇到:字符或者达到宽度限制才停止读取。
                        * [^:]:最外面的:表示在读取完[^:]部分后，会尝试匹配输入字符串中的:字符，
                        *      如果匹配成功则继续处理后续格式说明符；若匹配失败，sscanf会停止读取。
                        */
                        
                        /* 验证前缀是否匹配预期 */ 
                        if (strcmp(state_prefix, "account_state") == 0 && 
                            strcmp(error_prefix, "error_count") == 0) {
                            /* 复制状态值（lock/unlock） */ 
                            strncpy(login_user_info.account_status, state_value, sizeof(login_user_info.account_status) - 1);
                        } else {
                            /* 前缀不匹配，使用默认值 */ 
                            strcpy(login_user_info.account_status, "unlock");
                            login_user_info.error_count = 0;
                        }
                    } else {
                        /* 解析失败，使用默认值 */ 
                        strcpy(login_user_info.account_status, "unlock");
                        login_user_info.error_count = 0;
                    }
                } else {
                    /* 如果没有第二行，初始化为未锁定状态（注册时已初始化过） */ 
                    strcpy(login_user_info.account_status, "unlock");
                    login_user_info.error_count = 0;
                }
            }
        }
    }
    
    /* 检查账户是否被锁定 */ 
    if (strcmp(login_user_info.account_status, "lock") == 0) {
        /* 更新账户信息文件（确保锁定状态被保存） */ 
        user_data_fd = open(temp_account_number_buf, O_RDWR | O_CREAT | O_TRUNC);
        if (user_data_fd != -1) {
            char user_data_buf[1024] = {0};
            sprintf(user_data_buf, "account:%s,password:%s\n", 
                    login_user_info.account_number_buf, login_user_info.password_number_buf);
            sprintf(user_data_buf + strlen(user_data_buf), "account_state:%s,error_count:%d\n", 
                    login_user_info.account_status, login_user_info.error_count);
            write(user_data_fd, user_data_buf, strlen(user_data_buf));
            close(user_data_fd);
        }

        /* 屏幕提示 */
        lcd_render_text_with_box(
            "账户已锁定，请找回密码！",     
            340, 400,                       
            COLOR_WHITE,                    
            COLOR_LIGHTGRAY,                
            10,                             
            BOX_STYLE_ROUNDED,              
            15,                             
            30,                             
            0,                              
            0                               
        );
        sleep(3);
        printf("account_state:%s,error_count:%d\n",login_user_info.account_status,login_user_info.error_count); /* debug */

        /* 清除用户输入数据 */ 
        memset(&user_info, 0, sizeof(user_info));
        memset(&login_user_info, 0, sizeof(login_user_info));
        
        /* 返回登录界面 */
        login_boot();
        return;
    }
    
    /* 验证密码 */ 
    if (strcmp(login_user_info.password_number_buf, user_info.password_number_buf) == 0) {
        /* 密码正确，重置错误次数 */ 
        login_user_info.error_count = 0;
        strcpy(login_user_info.account_status, "unlock");
        
        /* 更新账户信息文件（保存重置后的状态 */ 
        user_data_fd = open(temp_account_number_buf, O_RDWR | O_CREAT | O_TRUNC);
        if (user_data_fd != -1) {
            char user_data_buf[1024] = {0};
            sprintf(user_data_buf, "account:%s,password:%s\n", 
                    login_user_info.account_number_buf, login_user_info.password_number_buf);
            sprintf(user_data_buf + strlen(user_data_buf), "account_state:%s,error_count:%d\n", 
                    login_user_info.account_status, login_user_info.error_count);
            write(user_data_fd, user_data_buf, strlen(user_data_buf));
            close(user_data_fd);
        }

        lcd_render_text_with_box(
            "登陆成功，请尽兴游玩~",     
            340, 400,                       
            COLOR_WHITE,                    
            COLOR_LIGHTGRAY,                
            10,                             
            BOX_STYLE_ROUNDED,              
            15,                             
            30,                             
            0,                              
            0                               
        );
        sleep(3);
        printf("account_state:%s,error_count:%d\n",login_user_info.account_status,login_user_info.error_count); /* debug */
        
        memset(&user_info, 0, sizeof(user_info));
        memset(&login_user_info, 0, sizeof(login_user_info));
        login_allow_flags = 1;
        skip_login_boot_flags = 1;
    } else {
        /* 密码错误，增加错误次数 */ 
        login_user_info.error_count++;
        
        /* 更新状态 */ 
        if (login_user_info.error_count >= 3) {
            strcpy(login_user_info.account_status, "lock");
            lcd_render_text_with_box(
                "密码错误3次，账户已锁定，请找回密码！",     
                310, 400,                       
                COLOR_WHITE,                    
                COLOR_LIGHTGRAY,                
                10,                             
                BOX_STYLE_ROUNDED,              
                15,                             
                30,                             
                0,                              
                0                               
            );
        } else {
            strcpy(login_user_info.account_status, "unlock");
            lcd_render_text_with_box(
                "密码错误，请重新输入！",     
                340, 400,                       
                COLOR_WHITE,                    
                COLOR_LIGHTGRAY,                
                10,                             
                BOX_STYLE_ROUNDED,              
                15,                             
                30,                             
                0,                              
                0                               
            );
        }
        sleep(3);
        printf("account_state:%s,error_count:%d\n",login_user_info.account_status,login_user_info.error_count);

        /* 更新账户信息文件（保存错误计数和状态） */ 
        user_data_fd = open(temp_account_number_buf, O_RDWR | O_CREAT | O_TRUNC);
        if (user_data_fd != -1) {
            char user_data_buf[1024] = {0}; 
            sprintf(user_data_buf, "account:%s,password:%s\n", 
                    login_user_info.account_number_buf, login_user_info.password_number_buf);
            sprintf(user_data_buf + strlen(user_data_buf), "account_state:%s,error_count:%d\n", 
                    login_user_info.account_status, login_user_info.error_count);
            write(user_data_fd, user_data_buf, strlen(user_data_buf));
            close(user_data_fd);
        }
        
        memset(&user_info, 0, sizeof(user_info));
        memset(&login_user_info, 0, sizeof(login_user_info));
        login_allow_flags = 0;
        login_boot(); // 返回登录界面
    }
}

/* 注册判断 */
void register_judgment() {
    /* 初始化字库 */ 
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }
    /* 满足所有注册条件时 */
    if (register_allow_flags == 1) {
        /*
        * 已经提前校验输入的合法性，只剩下一个问题，是否有重复的账号，对吧，
        * 因此此处再进行校验是否存在账号，若存在，不允许注册。
        * 不再注册界面判断就是因为简化判断流程，减少不必要的内嵌。
        */

        /* 1.先创建一个临时的账号数组，并用该数组来起名XXX.txt。 XXX为账号 */
        /* 存放用户输入的账号（用于判断是否存在该用户） */
        char temp_account_number_buf[128] = {0};
        /* 存放拼接后的账号,此时账号文档已创建 */
        sprintf(temp_account_number_buf, "%s.txt", user_info.account_number_buf);

        /* 2.创建个人文档，此时先判定是否存在，存在返回值-1；不存在就把数据写进去 */
        /* 尝试打开文档（账号），若账号存在，返回值-1 */
        int user_data_fd = open(temp_account_number_buf, O_RDWR | O_CREAT | O_EXCL);
        if (user_data_fd == -1) {
            /* 因为存在该账号，所以返回值-1 */
            printf("该账号已存在，请重新注册\n");
            lcd_render_text_with_box(
                "该账号已存在，请重新注册",         /* 文本内容 */
                340, 400,                       /* 起始坐标 (x, y) */
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
            memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
            memset(temp_account_number_buf, 0, sizeof(temp_account_number_buf));
            memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
            memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
            register_allow_flags = 0;
            register_boot();
            return;
        }

        /* 3.检查无误，可存放账号与密码于对应的文档内 */
        /* 再创建一个数组，用于将账号与密码统一一起，存入账号文档 */
        char user_data_buf[128] = {0};
        /* 将输入的账号与密码统一一起 */
        sprintf(user_data_buf, "account:%s,password:%s\n", 
                user_info.account_number_buf, 
                user_info.password_number_buf);
        /* 将统一后的数据，写入到个人文档中 */
        write(user_data_fd, user_data_buf, strlen(user_data_buf));
        
        /* 追加默认状态信息：account_state:unlock,error_count:0 */
        memset(user_data_buf, 0, sizeof(user_data_buf));
        sprintf(user_data_buf, "account_state:unlock,error_count:0\n");
        write(user_data_fd, user_data_buf, strlen(user_data_buf));

        lcd_render_text_with_box(
            "注册成功，返回登录界面",         /* 文本内容 */
            340, 400,                       /* 起始坐标 (x, y) */
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
        memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
        memset(temp_account_number_buf, 0, sizeof(temp_account_number_buf));
        memset(user_data_buf, 0, sizeof(user_data_buf));
        memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
        memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
        register_allow_flags = 0; /* 重置注册成功标志位 */
        skip_register_boot_flags = 1;
        close(user_data_fd);
        login_boot();
        return;
    } else if (register_allow_flags == 0 || 
               strlen(user_info.account_number_buf) == 0 ||
               strlen(user_info.password_number_buf) == 0) {
        /* 账号或密码为空时 */
        lcd_render_text_with_box(
            "账号或密码为空，请重新注册",     /* 文本内容 */
            340, 400,                       /* 起始坐标 (x, y) */
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
        memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
        register_background_box();  /* 点击到注册账号按钮 */
        register_boot();
        return;
    } else {
        /* 返回上一层 */ 
        memset(user_info.account_number_buf, 0, sizeof(user_info.account_number_buf));
        memset(user_info.password_number_buf, 0, sizeof(user_info.password_number_buf));
        memset(user_info.hide_password_number_buf, 0, sizeof(user_info.hide_password_number_buf));
        register_background_box();  /* 点击到注册账号按钮 */
        register_boot();
        return;
    }

    /* 清理资源 */ 
    lcd_cleanup();
}

/* 找回判断1 */
void find_account_judgment_1()
{
    /* 界面渲染 */
    show_bmp_to_lcd("find_account.bmp", 0, 0, 1024, 600);
    
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    if (find_account_allow_flags == 1) {
        /* 存放用户输入的账号（用于判断是否存在该用户） */
        char temp_account_number_buf[128] = {0};
        /* 存放拼接后的账号 */
        sprintf(temp_account_number_buf, "%s.txt", user_info.account_number_buf);
        /* 尝试打开文档（账号），若账号不存在，返回值-1 */
        int user_data_fd = open(temp_account_number_buf, O_RDONLY);
        if (user_data_fd == -1) {
            /* 渲染账号与密码背景文本框 */
            lcd_render_text_with_box(
                user_info.account_number_buf,   /* 文本内容 */
                104, 248,                       /* 起始坐标 (x, y) */
                COLOR_BLACK,                    /* 文本颜色 */ 
                COLOR_WHITE,                    /* 文本框背景颜色 */ 
                0,                              /* 文本与文本框边缘的间距 */ 
                BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
                0,                              /* 矩形样式不需要半径 */
                50,                             /* 字体大小 */
                386,                            /* 文本框宽度 */
                50                              /* 文本框高度 */
            );
            lcd_render_text_with_box(
                user_info.hide_password_number_buf,     /* 文本内容 */
                104, 310,                               /* 起始坐标 (x, y) */
                COLOR_BLACK,                            /* 文本颜色 */  
                COLOR_WHITE,                            /* 文本框背景颜色 */
                0,                                      /* 文本与文本框边缘的间距 */
                BOX_STYLE_RECTANGLE,                    /* 矩形样式 */
                0,                                      /* 矩形样式不需要半径 */
                50,                                     /* 字体大小 */
                386,                                    /* 文本框宽度 */
                50                                      /* 文本框高度 */
            );
            show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);

            /* 账号不存在处理 */
            lcd_render_text_with_box(
                "该账号不存在，请先进行注册！", 
                340, 400, 
                COLOR_WHITE, 
                COLOR_LIGHTGRAY, 
                10,
                BOX_STYLE_ROUNDED, 
                15, 
                30, 
                0, 
                0
            );
            sleep(3);
            memset(&user_info, 0, sizeof(user_info));
            memset(temp_account_number_buf, 0, sizeof(temp_account_number_buf));
            find_account_allow_flags = 0;
            login_boot();
            return;
        } else {
            find_account_verification();
            return;
        }
    } else {
        /* 渲染账号与密码文本框 */
        /* 账号输入文本框 */ 
        lcd_render_text_with_box(
            user_info.account_number_buf,   /* 文本内容 */
            104, 248,                       /* 起始坐标 (x, y) */
            COLOR_BLACK,                    /* 文本颜色 */ 
            COLOR_WHITE,                    /* 文本框背景颜色 */ 
            0,                              /* 文本与文本框边缘的间距 */ 
            BOX_STYLE_RECTANGLE,            /* 矩形样式 */ 
            0,                              /* 矩形样式不需要半径 */
            50,                             /* 字体大小 */
            386,                            /* 文本框宽度 */
            50                              /* 文本框高度 */
        );
        /* 密码输入文本框 */
        lcd_render_text_with_box(
            user_info.hide_password_number_buf, /* 文本内容 */
            104, 310,                           /* 起始坐标 (x, y) */
            COLOR_BLACK,                        /* 文本颜色 */  
            COLOR_WHITE,                        /* 文本框背景颜色 */
            0,                                  /* 文本与文本框边缘的间距 */
            BOX_STYLE_RECTANGLE,                /* 矩形样式 */
            0,                                  /* 矩形样式不需要半径 */
            50,                                 /* 字体大小 */
            386,                                /* 文本框宽度 */
            50                                  /* 文本框高度 */
        );
        show_bmp_to_lcd("hide_password.bmp", 448, 315, 40, 40);
        lcd_render_text_with_box(
            "账号或密码未输入，禁止账号找回！", 
            310, 400, 
            COLOR_WHITE, 
            COLOR_LIGHTGRAY, 
            10,
            BOX_STYLE_ROUNDED, 
            15, 
            30, 
            0, 
            0
        );
        sleep(3);
        memset(&user_info, 0, sizeof(user_info));
        find_account_allow_flags = 0;
        find_account_boot();
        return;
    }
}

/* 找回判断2 */
void find_account_judgment_2()
{
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("Font library initialization failed.\n");
        return;
    }

    /* 防止线程残留 */
    stop_code_thread();

    /* 先处理不允许找回的逻辑 */
    if (find_account_allow_flags == 0) {
        /* 不允许找回 */
        lcd_render_text_with_box(
            "当前禁止用户找回，请重试！",      /* 文本内容 */
            340, 400,                       /* 起始坐标 (x, y) */
            COLOR_WHITE,                    /* 文本颜色 */
            COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
            10,                             /* 文本与文本框边缘的间距 */
            BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */
            15,                             /* 圆角矩形半径 */
            30,                             /* 字体大小 */
            0,                              /* 文本框宽度 */
            0                               /* 文本框高度 */
        );
        sleep(3);
        memset(&user_info, 0, sizeof(user_info));
        memset(code, 0, sizeof(code));
        find_account_allow_flags = 0;
        stop_code_thread();
        login_boot();
        return;
    } else if (find_account_allow_flags == 1 && strlen(user_info.verification_code_buf) !=0) {
        /* 处理验证码的正确性 */
        if(strcmp(code, user_info.verification_code_buf) == 0)
        {
            /* 验证码正确 */

            /* 
            * 一定要先创建一个临时账户，否则你改都改不了。
            * 若变成int user_data_fd = open(user_info.account_number_buf, O_RDWR); 
            * 则下一步打开文件就是以账号名为文件，所以是修改不了的。
            */
            char temp_account_number_buf[128] = {0};
            sprintf(temp_account_number_buf, "%s.txt", user_info.account_number_buf);
            int user_data_fd = open(temp_account_number_buf, O_RDWR);
            char user_data_buf[256] = {0};
            sprintf(user_data_buf, "account:%s,password:%s\n"
                                   "account_state:unlock,error_count:0\n", 
                    user_info.account_number_buf, 
                    user_info.password_number_buf);
            write(user_data_fd, user_data_buf, strlen(user_data_buf));

            lcd_render_text_with_box(
                "修改密码成功，请重新登录！",      /* 文本内容 */
                340, 400,                       /* 起始坐标 (x, y) */
                COLOR_WHITE,                    /* 文本颜色 */
                COLOR_LIGHTGRAY,                /* 文本框背景颜色 */
                10,                             /* 文本与文本框边缘的间距 */
                BOX_STYLE_ROUNDED,              /* 圆角矩形样式 */
                15,                             /* 圆角矩形半径 */
                30,                             /* 字体大小 */
                0,                              /* 文本框宽度 */
                0                               /* 文本框高度 */
            );
            //show_bmp_to_lcd("find_account_success.bmp", 0, 0, 1024, 600);
            sleep(3);
            memset(&user_info, 0, sizeof(user_info));
            memset(user_data_buf, 0, sizeof(user_data_buf));
            memset(code, 0, sizeof(code));
            close(user_data_fd);
            find_account_allow_flags = 0;
            skip_find_account_boot_flags = 1;
            stop_code_thread();
            login_boot();
            return;
        } else {
            /* 验证码为空时 */
            lcd_render_text_with_box(
                "验证码错误，请重新获取验证码！",              /* 文本内容 */
                340, 400,                                   /* 起始坐标 (x, y) */
                COLOR_WHITE,                                /* 文本颜色 */
                COLOR_LIGHTGRAY,                            /* 文本框背景颜色 */
                10,                                         /* 文本与文本框边缘的间距 */
                BOX_STYLE_ROUNDED,                          /* 圆角矩形样式 */
                15,                                         /* 圆角矩形半径 */
                30,                                         /* 字体大小 */
                0,                                          /* 文本框宽度 */
                0                                           /* 文本框高度 */
            );
            sleep(3);
            memset(user_info.verification_code_buf, 0, sizeof(user_info.verification_code_buf));
            memset(code, 0, sizeof(code));
            find_account_allow_flags = 0;
            find_account_verification();
            stop_code_thread();
            return;
        }
    } else {
        /* 验证码为空时 */
        lcd_render_text_with_box(
            "验证码为空或验证码错误，请重新获取验证码！",   /* 文本内容 */
            220, 400,                                   /* 起始坐标 (x, y) */
            COLOR_WHITE,                                /* 文本颜色 */
            COLOR_LIGHTGRAY,                            /* 文本框背景颜色 */
            10,                                         /* 文本与文本框边缘的间距 */
            BOX_STYLE_ROUNDED,                          /* 圆角矩形样式 */
            15,                                         /* 圆角矩形半径 */
            30,                                         /* 字体大小 */
            0,                                          /* 文本框宽度 */
            0                                           /* 文本框高度 */
        );
        sleep(3);
        memset(user_info.verification_code_buf, 0, sizeof(user_info.verification_code_buf));
        memset(code, 0, sizeof(code));
        find_account_allow_flags = 0;
        find_account_verification();
        stop_code_thread();
        return;
    }
    /* 清理lcd资源 */
    lcd_cleanup();
}

/* 游戏引导页面 */
void game_start_home() {
    login_allow_flags = 0;
    //show_gif_to_lcd("loading.gif", 0, 0, 1024, 600, 10);
    show_bmp_to_lcd("game_start.bmp", 0, 0, 1024, 600);
    return;
}

// 加载奖项图片到缓存
int load_prize_image(const char *bmp_path) {
    int bmp_fd = open(bmp_path, O_RDONLY);
    if (bmp_fd == -1) {
        printf("打开奖项图片 %s 失败\n", bmp_path);
        return -1;
    }

    unsigned char *bmp_data = malloc(1024 * 600 * 3);
    lseek(bmp_fd, 54, SEEK_SET);
    read(bmp_fd, bmp_data, 1024 * 600 * 3);
    close(bmp_fd);

    prize_buf = malloc(1024 * 600 * 2);
    int x, y;
    for (y = 0; y < 600; y++) {
        for (x = 0; x < 1024; x++) {
            int data_idx = (599 - y) * 1024 * 3 + x * 3;
            unsigned char b = bmp_data[data_idx];
            unsigned char g = bmp_data[data_idx + 1];
            unsigned char r = bmp_data[data_idx + 2];
            // 直接进行转换逻辑
            unsigned short red = (r >> 3) << 11;
            unsigned short green = (g >> 2) << 5;
            unsigned short blue = b >> 3;
            prize_buf[y * 1024 + x] = red | green | blue;
        }
    }

    free(bmp_data);
    printf("奖项图片已加载并缩放至全屏\n");
    return 0;
}

// 显示完整中奖图片（达到阈值时调用）
void show_full_prize() {
    memcpy(lcd_buf, prize_buf, 1024 * 600 * 2);
    threshold_reached = 1;
    stop_touch = 1; // 设置停止触摸事件循环的标志
    printf("已刮开%.1f%%区域，展示完整中奖图片\n", (float)scratched_count / (1024 * 600) * 100);
}

// 刮奖逻辑（统计刮开面积）
void scratch_area(int x, int y, int radius) {
    if (threshold_reached) return;

    int dy, dx;
    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy > radius*radius) continue;

            int real_x = x + dx;
            int real_y = y + dy;

            if (real_x < 0 || real_x >= 1024) continue;
            if (real_y < 0 || real_y >= 600) continue;

            if (!scratched[real_x + real_y * 1024]) {
                scratched[real_x + real_y * 1024] = 1;
                scratched_count++;

                lcd_buf[real_y * 1024 + real_x] = prize_buf[real_y * 1024 + real_x];

                if (scratched_count % 1000 == 0) {
                    float progress = (float)scratched_count / (1024 * 600) * 100;
                    printf("当前刮开进度: %.1f%%\n", progress);
                }

                if (scratched_count >= (int)(1024 * 600 * threshold_ratio)) {
                    show_full_prize();
                    return;
                }
            }
        }
    }
}

// 连续触摸函数
void touch_fun() {
    int touch_fd = open("/dev/input/event1", O_RDONLY);
    if (touch_fd == -1) {
        perror("open error.\n");
        return;
    }

    struct input_event input_buf;
    while (1) { // 当 stop_touch 为 1 时退出循环
        if (stop_touch == 0) {
            read(touch_fd, &input_buf, sizeof(input_buf));
            if (input_buf.type == 3 && input_buf.code == 0) {
                input_x = input_buf.value;
            }
            if (input_buf.type == 3 && input_buf.code == 1) {
                input_y = input_buf.value;
            }
            scratch_area(input_x, input_y, 30);
            printf("input_x = %d, input_y = %d\n", input_x, input_y);
        } else {
            return;
        }
    }

    close(touch_fd);
}

// 游戏开始函数
void game_start() {

    /* 初始化游戏参数 */
    scratched_count = 0;
    threshold_reached = 0;
    stop_touch = 0;

    // 打开屏幕
    int lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1) {
        perror("打开屏幕失败");
        return;
    }

    // 内存映射屏幕
    lcd_buf = mmap(NULL, 1024 * 600 * 2, PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
    if (lcd_buf == MAP_FAILED) {
        perror("屏幕映射失败");
        close(lcd_fd);
        return;
    }

    // 初始化刮开标记数组
    scratched = calloc(1024 * 600, sizeof(unsigned char));

    show_bmp_to_lcd("game_start.bmp", 0, 0, 1024, 600);

    // 随机选择奖项（一等奖10%，二等奖20%，三等奖30%，不中奖40%）
    srand(time(NULL));
    int prize = rand() % 100;
    if (prize < 10) {
        printf("恭喜获得一等奖！\n");
        load_prize_image("first_prize.bmp");
    } else if (prize < 30) {
        printf("恭喜获得二等奖！\n");
        load_prize_image("second_prize.bmp");
    } else if (prize < 60) {
        printf("恭喜获得三等奖！\n");
        load_prize_image("third_prize.bmp");
    } else {
        printf("未中奖，再接再厉！\n");
        load_prize_image("no_prize.bmp");
    }

    // 显示提示信息
    printf("刮开%.1f%%区域即可显示完整结果\n", threshold_ratio * 100);

    touch_fun();

    // 清理资源
    munmap(lcd_buf, 1024 * 600 * 2);
    free(prize_buf);
    free(scratched);
    close(lcd_fd);
    game_exit();
}

void game_exit() {
    while (1) {
        ts_fun();
        if (stop_touch == 1) {
            if (input_x >= 0 && input_x <= 211 && input_y >= 540 && input_y <= 600) {
                game_start();
                return;
            } else if (input_x >= 0 && input_x <= 211 && input_y >= 477 && input_y <= 530) {
                show_bmp_to_lcd("game_exit.bmp", 0, 0, 1024, 600);
                while (2) {
                    ts_fun();
                    if (input_x >= 313 && input_x <= 497 && input_y >= 363 && input_y <= 447) {
                        main();
                        return;
                    } else if (input_x >= 534 && input_x <= 714 && input_y >= 363 && input_y <= 447) {
                        game_start();
                        return;
                    }
                }
            }
        }
    }
}

/* 程序主函数 */
int main() 
{
    boot();
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
    game_start();
    return 0;
}