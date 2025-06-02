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

// 假设rgb888_to_rgb565函数定义在这里
unsigned short rgb888_to_rgb565(unsigned char r, unsigned char g, unsigned char b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

// 从p.c复制过来的显示BMP图片函数
void show_bmp_to_lcd(char *picname)
{
    /* 打开屏幕和bmp图片文件 */
    int lcd_fd = open("/dev/fb0", O_RDWR);
    int bmp_fd = open(picname, O_RDWR);
    if (lcd_fd == -1) {
        printf("无法打开屏幕.\n");
    }
    if (bmp_fd == -1) {
        printf("无法打开照片.\n");
    }

    /* 获取bmp图片的颜色数据   bmpbuf:存放bmp图片的颜色数据 */
    unsigned char bmp_buf[1024 * 600 * 3];
    lseek(bmp_fd, 54, SEEK_SET);    // 偏移文件的光标
    if (read(bmp_fd, bmp_buf, 1024 * 600 * 3) != 1024 * 600 * 3) {
        printf("读取像素数据失败.\n");
        close(lcd_fd);
        close(bmp_fd);
    }

    /* 映射屏幕内存空间 */
    unsigned short *mmap_start = (unsigned short *)mmap(NULL, 1024 * 600 * 2, PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
    if (mmap_start == MAP_FAILED) {
        printf("申请映射空间失败.\n");
        close(lcd_fd);
        close(bmp_fd);
    }

    int n = 0;
    for (int y = 0; y < 600; y++) {
        for (int x = 0; x < 1024; x++, n += 3) {
            unsigned char b = bmp_buf[n + 0]; // BGR顺序中的B
            unsigned char g = bmp_buf[n + 1]; // BGR顺序中的G
            unsigned char r = bmp_buf[n + 2]; // BGR顺序中的R
            unsigned short rgb565 = rgb888_to_rgb565(r, g, b);
            *(mmap_start + 1024 * (599 - y) + x) = rgb565;
        }
    }
    munmap(mmap_start, 1024 * 600 * 2);
    close(lcd_fd);
    close(bmp_fd);
}