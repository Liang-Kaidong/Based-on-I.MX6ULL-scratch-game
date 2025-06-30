/***********************************************************************
File name: show_bmp_to_lcd.c
Author: KD
Version: V_1.0
Build date: 2024-06-30
Description: This C file enables displaying BMP images on an LCD screen.
             It supports BMPs of any size and allows specifying display
             position and target size. Through operations like reading
             file headers, converting color formats, and memory mapping,
             it writes image pixel data to LCD memory.
Others: Usage requires preservation of original author attribution.
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

/*
* @struct BMPFileHeader
* @brief BMP文件头结构（14字节）
* 
* 用于存储BMP文件的基本信息，位于文件起始位置
* 包含文件标识、文件大小、数据偏移量等信息
*/
#pragma pack(1)  /* 强制按1字节对齐，确保结构大小与文件格式一致 */
typedef struct {
    unsigned char signature[2];      /* 文件标识，必须为"BM"(0x42, 0x4D) */
    unsigned int file_size;          /* 文件总大小(字节) */
    unsigned short reserved1;        /* 保留字段，通常为0 */
    unsigned short reserved2;        /* 保留字段，通常为0 */
    unsigned int data_offset;        /* 像素数据相对于文件头的偏移量 */
} BMPFileHeader;
#pragma pack()

/*
* @struct BMPInfoHeader
* @brief BMP信息头结构（40字节）
* 
* 存储图像的详细信息，紧跟在文件头之后
* 包含图像尺寸、位深度、分辨率等信息
*/
#pragma pack(1)
typedef struct {
    unsigned int header_size;        /* 信息头大小，通常为40字节 */
    int width;                       /* 图像宽度(像素)，可为负值表示从下到上存储 */
    int height;                      /* 图像高度(像素)，可为负值表示从上到下存储 */
    unsigned short planes;           /* 平面数，必须为1 */
    unsigned short bit_count;        /* 位深度，常见值为24(真彩色) */
    unsigned int compression;        /* 压缩方式，0表示无压缩 */
    unsigned int image_size;         /* 图像数据大小，0表示未压缩时需计算 */
    int x_pixels_per_meter;          /* 水平分辨率(像素/米) */
    int y_pixels_per_meter;          /* 垂直分辨率(像素/米) */
    unsigned int colors_used;        /* 使用的颜色数，0表示使用所有可能颜色 */
    unsigned int colors_important;   /* 重要的颜色数，0表示所有颜色都重要 */
} BMPInfoHeader;
#pragma pack()

/*
* @brief 将24位RGB888颜色转换为16位RGB565格式
* 
* RGB565格式: R占5位(最高位), G占6位, B占5位(最低位)
* @param r 红色分量(0-255)
* @param g 绿色分量(0-255)
* @param b 蓝色分量(0-255)
* @return 16位RGB565颜色值
*/
unsigned short rgb888_to_rgb565(unsigned char r, unsigned char g, unsigned char b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

/*
* @brief 显示BMP图片到LCD的函数
* 
* 该函数实现了将指定BMP图片显示到LCD的指定位置，并可指定显示尺寸
* 支持任意尺寸的BMP图片，自动获取LCD屏幕尺寸，适配不同分辨率的LCD
* 
* @param picname 要显示的BMP图片的文件名
* @param start_x 图片在LCD上显示的起始x坐标(左上角)
* @param start_y 图片在LCD上显示的起始y坐标(左上角)
* @param target_width 图片显示的目标宽度，0表示使用原始宽度
* @param target_height 图片显示的目标高度，0表示使用原始高度
* 
* @return 成功返回0，失败返回-1
*/
int show_bmp_to_lcd(char *picname, int start_x, int start_y, int target_width, int target_height) {
    int lcd_fd = -1, bmp_fd = -1;       /* LCD设备和BMP文件的文件描述符 */
    unsigned short *lcd_mem = NULL;     /* LCD内存映射地址，用于直接操作LCD像素 */
    unsigned char *bmp_data = NULL;     /* 存储BMP像素数据的缓冲区 */
    BMPFileHeader file_header;          /* BMP文件头结构，存储文件基本信息 */
    BMPInfoHeader info_header;          /* BMP信息头结构，存储图像详细信息 */
    struct fb_var_screeninfo vinfo;     /* LCD可变屏幕信息结构，用于获取屏幕分辨率 */
    int screen_width, screen_height;    /* LCD屏幕宽度和高度，通过系统调用获取 */
    int bmp_width, bmp_height;          /* BMP图片的实际宽度和高度 */
    int actual_target_width, actual_target_height;  /* 实际显示的目标尺寸 */
    float scale_x, scale_y;             /* 水平和垂直缩放比例，用于图像缩放 */
    int i, j;                           /* 循环计数器，用于遍历像素 */

    /* 打开LCD设备文件，获取文件描述符 */
    lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd < 0) {
        perror("无法打开LCD设备");
        return -1;
    }

    /* 获取LCD实际尺寸信息 */
    if (ioctl(lcd_fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
        perror("获取LCD尺寸失败");
        close(lcd_fd);
        return -1;
    }
    screen_width = vinfo.xres;          /* 屏幕实际宽度(像素) */
    screen_height = vinfo.yres;         /* 屏幕实际高度(像素) */
    //printf("检测到LCD尺寸: %dx%d\n", screen_width, screen_height);

    /* 打开BMP图片文件 */
    bmp_fd = open(picname, O_RDWR);
    if (bmp_fd < 0) {
        perror("无法打开BMP文件");
        close(lcd_fd);
        return -1;
    }

    /* 读取BMP文件头，获取文件基本信息 */
    if (read(bmp_fd, &file_header, sizeof(file_header)) != sizeof(file_header)) {
        perror("读取BMP文件头失败");
        goto cleanup;
    }

    /* 验证文件是否为合法BMP格式 */
    if (file_header.signature[0] != 'B' || file_header.signature[1] != 'M') {
        fprintf(stderr, "错误：不是有效的BMP文件\n");
        goto cleanup;
    }

    /* 读取BMP信息头，获取图像详细信息 */
    if (read(bmp_fd, &info_header, sizeof(info_header)) != sizeof(info_header)) {
        perror("读取BMP信息头失败");
        goto cleanup;
    }

    /* 获取BMP图片的实际尺寸 */
    bmp_width = info_header.width;
    bmp_height = info_header.height;

    /* 处理负高度：BMP规范中，负高度表示图像是从上到下存储的 */
    if (bmp_height < 0) {
        bmp_height = -bmp_height;
    }

    /* 确定目标显示尺寸：如果参数为0，则使用原始尺寸 */
    actual_target_width = (target_width > 0) ? target_width : bmp_width;
    actual_target_height = (target_height > 0) ? target_height : bmp_height;

    /* 计算缩放比例：用于将原始图像映射到目标尺寸 */
    scale_x = (float)bmp_width / actual_target_width;
    scale_y = (float)bmp_height / actual_target_height;

    /* 将LCD设备内存映射到用户空间，以便直接操作像素 */
    lcd_mem = (unsigned short *)mmap(
        NULL,                   /* 让系统选择映射地址 */
        screen_width * screen_height * 2,  /* 映射内存大小(2字节/像素) */
        PROT_READ | PROT_WRITE, /* 允许读写操作 */
        MAP_SHARED,             /* 共享映射，更改会反映到物理设备 */
        lcd_fd,                 /* 文件描述符 */
        0                       /* 映射偏移量 */
    );
    if (lcd_mem == MAP_FAILED) {
        perror("映射LCD内存失败");
        goto cleanup;
    }

    /* 为BMP像素数据分配内存缓冲区 */
    bmp_data = (unsigned char *)malloc(bmp_width * bmp_height * 3);
    if (!bmp_data) {
        perror("分配BMP数据内存失败");
        goto cleanup;
    }

    /* 读取BMP像素数据 */
    lseek(bmp_fd, file_header.data_offset, SEEK_SET);  /* 定位到像素数据起始位置 */
    if (read(bmp_fd, bmp_data, bmp_width * bmp_height * 3) != bmp_width * bmp_height * 3) {
        perror("读取BMP像素数据失败");
        goto cleanup;
    }

    /*
    * 显示BMP图片主循环
    * 处理缩放和位置定位，将BMP像素映射到LCD指定位置
    */
    for (j = 0; j < actual_target_height; j++) {
        for (i = 0; i < actual_target_width; i++) {
            /* 计算源图像中的对应位置(考虑缩放) */
            int src_x = (int)(i * scale_x);
            int src_y = (int)(j * scale_y);

            /* 边界检查：确保源坐标在有效范围内 */
            if (src_x < 0 || src_x >= bmp_width || src_y < 0 || src_y >= bmp_height) {
                continue;
            }

            /*
            * 计算源像素在数据缓冲区中的偏移
            * BMP存储特点：从下到上、从左到右
            * 因此实际行号为 (bmp_height - 1 - src_y)
            */
            int pixel_offset = ((bmp_height - 1 - src_y) * bmp_width + src_x) * 3;

            /* 获取RGB值(BMP存储顺序是BGR) */
            unsigned char b = bmp_data[pixel_offset];
            unsigned char g = bmp_data[pixel_offset + 1];
            unsigned char r = bmp_data[pixel_offset + 2];

            /* 转换为RGB565格式(适应16位LCD) */
            unsigned short rgb565 = rgb888_to_rgb565(r, g, b);

            /* 计算目标位置在LCD内存中的偏移 */
            int dest_x = start_x + i;
            int dest_y = start_y + j;

            /* 边界检查：确保目标位置在LCD有效范围内 */
            if (dest_x < 0 || dest_x >= screen_width || dest_y < 0 || dest_y >= screen_height) {
                continue;
            }

            /* 写入像素数据到LCD内存 */
            lcd_mem[dest_y * screen_width + dest_x] = rgb565;
        }
    }

cleanup:
    /* 资源清理 */
    if (bmp_data) free(bmp_data);                /* 释放BMP数据缓冲区 */
    if (lcd_mem != MAP_FAILED) munmap(lcd_mem, screen_width * screen_height * 2);  /* 解除内存映射 */
    if (bmp_fd >= 0) close(bmp_fd);              /* 关闭BMP文件 */
    if (lcd_fd >= 0) close(lcd_fd);              /* 关闭LCD设备 */

    return 0;
}