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
int m = 0;    /* 判断变量m */ 
bool confirm_clicked = false;  /* 确认键是否被点击的标志 */

// 定义包含账号和密码数组的结构体
typedef struct {
    char account_number_buf[128];   /* 用于存储账号的数组 */
    char password_number_buf[128];  /* 用于存储密码的数组 */
} UserInfo;
UserInfo user_info = {{0}, {0}}; // 初始化用户信息结构体 

/* 向前声明 main 函数 */ 
int main();
void input_account_box();
void input_password_box();

/* 解码 UTF-8 字符，返回字符的字节长度 */ 
int decode_utf8(const char *str, int *codepoint) {   
    unsigned char c = (unsigned char)*str;  /* c：用于存储当前字符的第一个字节 */
    int len = 1;    /* len：用于存储当前字符的字节长度，初始化为 1，因为单字节 UTF-8 字符占用 1 个字节 */
    
    /* UTF-8 编码规则进行解码 */
    if (c < 0x80) {                     /* 单字节字符（ASCII 字符） */
        *codepoint = c;
    } else if ((c & 0xE0) == 0xC0) {    /* 双字节字符 */
        *codepoint = ((c & 0x1F) << 6) | (str[1] & 0x3F);
        len = 2;
    } else if ((c & 0xF0) == 0xE0) {    /* 三字节字符 */
        *codepoint = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        len = 3;
    } else if ((c & 0xF8) == 0xF0) {    /* 四字节字符 */
        *codepoint = ((c & 0x07) << 18) | ((str[1] & 0x3F) << 12) | 
                      ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        len = 4;
    } else {
        *codepoint = 0xFFFD;            /* 无效字符 */ 
    }
    
    return len;  /* 返回当前字符的字节长度 */
}

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
void not_open_game_notification() {
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

void keyboard() {
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
    }
    //lcd_clear(COLOR_BLACK);
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
void account_password_background_box() {
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
        "请输入账号（可用中文）",             /* 文本内容 */
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
    //lcd_cleanup();    /* 请不要启用，会导致输入开始时无效，并且会导致多种问题，建议后来者覆盖 */
}


/* 账号输入界面刷新线程函数 */
void* account_input_refresh(void* arg) {
    while (!confirm_clicked) {
        /* 初始化字库 */
        if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
            printf("初始化失败。\n");
            continue;
        }

        /* 置顶账号输入框，以解决闪烁问题 */
        show_bmp_to_lcd("login_2.bmp");    /* 加载背景图层 */
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
void* password_input_refresh(void* arg) {
    while (!confirm_clicked) {
        /* 初始化字库 */
        if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
            printf("初始化失败。\n");
            continue;
        }

        /* 置顶密码输入框，以解决闪烁问题 */
        show_bmp_to_lcd("login_2.bmp");    /* 加载背景图层 */
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
void input_account_box() {
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }

    /* 置顶账号输入框，以解决闪烁问题 */
    show_bmp_to_lcd("login_2.bmp");    /* 加载背景图层 */
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

    int m = strlen(user_info.account_number_buf);    /* 判断变量m，初始化为当前账号输入的长度 */ 
    system("stty -icanon");     /* 禁用标准输入的缓冲 */ 

    // 打开触摸屏文件
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;
    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(input_fd, &readfds);

        // 使用select函数同时监听标准输入和触摸屏事件
        int ret = select(input_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("select");
            break;
        }

        int input_changed = 0;  // 标记输入是否有变化

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buffer[4];  // 最大4字节的UTF-8字符
            int bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (bytes_read <= 0) continue;

            if (buffer[0] == '-' && m > 0) {   /* 处理退格键，且输入框不为空 */ 
                int i = m - 1;
                /* 从当前位置向前查找UTF-8字符的起始字节 */ 
                while (i > 0) {
                    unsigned char c = (unsigned char)user_info.account_number_buf[i];
                    /* 如果是后续字节 (0x80-0xBF)，继续向前查找 */ 
                    if ((c & 0xC0) == 0x80) {
                        i--;
                    } else {
                        /* 找到起始字节，退出循环 */ 
                        break;
                    }
                }
                /* 删除找到的UTF-8字符 */ 
                int char_len = m - i;
                if (char_len > 0) {
                    memmove(&user_info.account_number_buf[i], &user_info.account_number_buf[m], sizeof(user_info.account_number_buf) - m);
                    m = i;
                    input_changed = 1;  // 标记输入有变化
                }
            } else if (buffer[0] != '-' && m + bytes_read < sizeof(user_info.account_number_buf)) {  /* 存储输入字符，且不为退格键 */ 
                memcpy(&user_info.account_number_buf[m], buffer, bytes_read);
                m += bytes_read;
                input_changed = 1;  // 标记输入有变化
            }
        }

        if (FD_ISSET(input_fd, &readfds)) {
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
                    // 处理确认按钮点击事件
                    show_bmp_to_lcd("login_2.bmp");    /* 加载背景图层 */
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

                    ts_fun();
                    if (input_x >= 104 && input_x <= 248 && input_y >= 310 && input_y <= 400) {
                        input_password_box();
                        break;  // 退出循环
                    } else {
                        break;
                    }
                    break;
                }
                ts_fun();   //debug
                // 处理键盘点击事件
                if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { //空格键
                    strcat(user_info.account_number_buf, " ");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { //Q键
                    strcat(user_info.account_number_buf, "Q");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { //W键
                    strcat(user_info.account_number_buf, "W");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { //E键
                    strcat(user_info.account_number_buf, "E");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { //R键
                    strcat(user_info.account_number_buf, "R");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { //T键
                    strcat(user_info.account_number_buf, "T");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { //Y键
                    strcat(user_info.account_number_buf, "Y");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { //U键
                    strcat(user_info.account_number_buf, "U");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { //I键
                    strcat(user_info.account_number_buf, "I");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { //O键
                    strcat(user_info.account_number_buf, "O");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { //P键
                    strcat(user_info.account_number_buf, "P");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { //A键
                    strcat(user_info.account_number_buf, "A");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { //S键
                    strcat(user_info.account_number_buf, "S");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { //D键
                    strcat(user_info.account_number_buf, "D");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { //F键
                    strcat(user_info.account_number_buf, "F");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                    strcat(user_info.account_number_buf, "G");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { //H键
                    strcat(user_info.account_number_buf, "H");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { //J键
                    strcat(user_info.account_number_buf, "J");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { //K键
                    strcat(user_info.account_number_buf, "K");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { //L键
                    strcat(user_info.account_number_buf, "L");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { //Z键
                    strcat(user_info.account_number_buf, "Z");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { //X键
                    strcat(user_info.account_number_buf, "X");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { //C键
                    strcat(user_info.account_number_buf, "C");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { //V键
                    strcat(user_info.account_number_buf, "V");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { //B键
                    strcat(user_info.account_number_buf, "B");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { //N键
                    strcat(user_info.account_number_buf, "N");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { //M键
                    strcat(user_info.account_number_buf, "M");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { //删除键
                    user_info.account_number_buf[strlen(user_info.account_number_buf) - 1] = '\0';
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
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

    // 恢复标准输入的缓冲
    system("stty icanon");

    // 关闭触摸屏文件
    close(input_fd);

    // 清理资源
    lcd_cleanup();
}

/* 密码输入实现 */
void input_password_box() {
    /* 初始化字库 */
    if (lcd_init("/dev/fb0", "simkai.ttf") != 0) {
        printf("初始化失败。\n");
        return;
    }
    show_bmp_to_lcd("login_2.bmp");    /* 加载背景图层 */
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
    
    int m = strlen(user_info.password_number_buf);    /* 判断变量m，初始化为当前账号输入的长度 */ 
    system("stty -icanon");     /* 禁用标准输入的缓冲 */ 

    // 打开触摸屏文件
    int input_fd = open("/dev/input/event1", O_RDWR);
    if (input_fd == -1) {
        perror("Failed to open touchscreen device");
        return;
    }

    struct input_event input_buf;
    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(input_fd, &readfds);

        // 使用select函数同时监听标准输入和触摸屏事件
        int ret = select(input_fd + 1, &readfds, NULL, NULL, NULL);
        if (ret == -1) {
            perror("select");
            break;
        }

        int input_changed = 0;  // 标记输入是否有变化

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buffer[4];  // 最大4字节的UTF-8字符
            int bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (bytes_read <= 0) continue;

            if (buffer[0] == '-' && m > 0) {   /* 处理退格键，且输入框不为空 */ 
                int i = m - 1;
                /* 从当前位置向前查找UTF-8字符的起始字节 */ 
                while (i > 0) {
                    unsigned char c = (unsigned char)user_info.password_number_buf[i];
                    /* 如果是后续字节 (0x80-0xBF)，继续向前查找 */ 
                    if ((c & 0xC0) == 0x80) {
                        i--;
                    } else {
                        /* 找到起始字节，退出循环 */ 
                        break;
                    }
                }
                /* 删除找到的UTF-8字符 */ 
                int char_len = m - i;
                if (char_len > 0) {
                    memmove(&user_info.password_number_buf[i], &user_info.password_number_buf[m], sizeof(user_info.password_number_buf) - m);
                    m = i;
                    input_changed = 1;  // 标记输入有变化
                }
            } else if (buffer[0] != '-' && m + bytes_read < sizeof(user_info.password_number_buf)) {  /* 存储输入字符，且不为退格键 */ 
                memcpy(&user_info.password_number_buf[m], buffer, bytes_read);
                m += bytes_read;
                input_changed = 1;  // 标记输入有变化
            }
        }

        if (FD_ISSET(input_fd, &readfds)) {
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
                    if (strlen(user_info.account_number_buf) && strlen(user_info.password_number_buf) != 0) {
                        show_bmp_to_lcd("login_2.bmp");    //测试debug
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
                        ts_fun();
                        if (input_x >= 104 && input_x <= 204 && input_y >= 431 && input_y <= 479) {
                            show_bmp_to_lcd("1.bmp");    //测试debug
                            printf("login sucessful.\n");
                        }
                    } else if (strlen(user_info.account_number_buf) == 0) {
                        printf("no acconut.\n");
                    } else if (strlen(user_info.password_number_buf) == 0) {
                        printf("no password.\n"); 
                    } else {
                        printf("no account and password.\n");
                    }
                    break;
                }
                ts_fun();   //debug
                // 处理键盘点击事件
                if (input_x >= 224 && input_x <= 810 && input_y >= 503 && input_y <= 600) { //空格键
                    strcat(user_info.password_number_buf, " ");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 0 && input_x <= 100 && input_y >= 198 && input_y <= 254) { //Q键
                    strcat(user_info.password_number_buf, "Q");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 144 && input_x <= 210 && input_y >= 198 && input_y <= 254) { //W键
                    strcat(user_info.password_number_buf, "W");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 247 && input_x <= 301 && input_y >= 198 && input_y <= 254) { //E键
                    strcat(user_info.password_number_buf, "E");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 331 && input_x <= 400 && input_y >= 198 && input_y <= 254) { //R键
                    strcat(user_info.password_number_buf, "R");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 436 && input_x <= 495 && input_y >= 198 && input_y <= 254) { //T键
                    strcat(user_info.password_number_buf, "T");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 544 && input_x <= 600 && input_y >= 198 && input_y <= 254) { //Y键
                    strcat(user_info.password_number_buf, "Y");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 640 && input_x <= 694 && input_y >= 198 && input_y <= 254) { //U键
                    strcat(user_info.password_number_buf, "U");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 740 && input_x <= 797 && input_y >= 198 && input_y <= 254) { //I键
                    strcat(user_info.password_number_buf, "I");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 834 && input_x <= 900 && input_y >= 198 && input_y <= 254) { //O键
                    strcat(user_info.password_number_buf, "O");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 927 && input_x <= 1024 && input_y >= 198 && input_y <= 254) { //P键
                    strcat(user_info.password_number_buf, "P");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 104 && input_x <= 150 && input_y >= 297 && input_y <= 353) { //A键
                    strcat(user_info.password_number_buf, "A");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 194 && input_x <= 264 && input_y >= 297 && input_y <= 353) { //S键
                    strcat(user_info.password_number_buf, "S");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 291 && input_x <= 350 && input_y >= 297 && input_y <= 353) { //D键
                    strcat(user_info.password_number_buf, "D");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 385 && input_x <= 447 && input_y >= 297 && input_y <= 353) { //F键
                    strcat(user_info.password_number_buf, "F");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 489 && input_x <= 544 && input_y >= 297 && input_y <= 353) { //G键
                    strcat(user_info.password_number_buf, "G");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 589 && input_x <= 647 && input_y >= 297 && input_y <= 353) { //H键
                    strcat(user_info.password_number_buf, "H");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 697 && input_x <= 742 && input_y >= 297 && input_y <= 353) { //J键
                    strcat(user_info.password_number_buf, "J");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 789 && input_x <= 847 && input_y >= 297 && input_y <= 353) { //K键
                    strcat(user_info.password_number_buf, "K");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 894 && input_x <= 938 && input_y >= 297 && input_y <= 353) { //L键
                    strcat(user_info.password_number_buf, "L");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 155 && input_x <= 205 && input_y >= 400 && input_y <= 458) { //Z键
                    strcat(user_info.password_number_buf, "Z");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 249 && input_x <= 297 && input_y >= 400 && input_y <= 458) { //X键
                    strcat(user_info.password_number_buf, "X");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 342 && input_x <= 400 && input_y >= 400 && input_y <= 458) { //C键
                    strcat(user_info.password_number_buf, "C");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 445 && input_x <= 495 && input_y >= 400 && input_y <= 458) { //V键
                    strcat(user_info.password_number_buf, "V");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 541 && input_x <= 589 && input_y >= 400 && input_y <= 458) { //B键
                    strcat(user_info.password_number_buf, "B");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 641 && input_x <= 692 && input_y >= 400 && input_y <= 458) { //N键
                    strcat(user_info.password_number_buf, "N");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 739 && input_x <= 800 && input_y >= 400 && input_y <= 458) { //M键
                    strcat(user_info.password_number_buf, "M");
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
                if (input_x >= 0 && input_x <= 120 && input_y >= 393 && input_y <= 600) { //删除键
                    user_info.password_number_buf[strlen(user_info.password_number_buf) - 1] = '\0';
                    input_changed = 1;  // 标记输入有变化
                    // 重置输入状态，确保后续键盘输入正常处理
                    FD_ZERO(&readfds);
                    FD_SET(STDIN_FILENO, &readfds);
                    FD_SET(input_fd, &readfds);
                }
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

    // 恢复标准输入的缓冲
    system("stty icanon");

    // 关闭触摸屏文件
    close(input_fd);

    // 清理资源
    lcd_cleanup();
}

void home_fun() {
    /* 显示桌面 */  
    show_bmp_to_lcd("home.bmp");
    while(1) {
        ts_fun();
        if (input_x >= 418 && input_x <= 494 && input_y >= 259 && input_y <= 331) {
            show_bmp_to_lcd("logo.bmp");
            usleep(1000000);
            show_bmp_to_lcd("anti_addiction.bmp");
            usleep(1000000);
            show_bmp_to_lcd("login_1.bmp");
            break;
        } else {
            not_open_game_notification();
            sleep(2);
            main();
        }
    }
    return;
}

void login_fun() {
    /* 登录界面 */ 
    while(1) {
        ts_fun();
        if (input_x >= 399 && input_x <= 621 && input_y >= 385 && input_y <= 490) {
            show_bmp_to_lcd("login_2.bmp");
            account_password_background_box();
            while (2) {
                ts_fun();
                if (input_x >= 104 && input_x <= 290 && input_y >= 248 && input_y <= 300) {
                    input_account_box();
                } else if (input_x >= 104 && input_x <= 290 && input_y >= 310 && input_y <= 400) {
                    input_password_box();
                } else {
                    while (2);
                }
                break;
            }
            break;
        }
        if (input_x >= 942 && input_x <= 1024 && input_y >= 524 && input_y <= 600) {
            show_bmp_to_lcd("login_exit.bmp");
            while(2) {
                ts_fun();
                if (input_x >= 313 && input_x <= 497 && input_y >= 363 && input_y <= 447) {
                    home_fun();
                    login_fun();    //重调用登陆界面
                    break;
                } else if (input_x >= 534 && input_x <= 714 && input_y >= 363 && input_y <= 447) {
                    show_bmp_to_lcd("login_1.bmp"); //  刷新屏幕
                    login_fun();
                    break;
                }
            }
            break;
        }
    }
    return;
}

int main() {
    home_fun();
    login_fun();
    //notification();
    return 0;
}