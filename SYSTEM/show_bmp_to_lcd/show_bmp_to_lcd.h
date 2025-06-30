/****************************************************************************
File name: show_bmp_to_lcd.h
Author: KD
Version: V_1.0
Build date: 2024-06-30
Description: This .h file provides the declaration and usage instructions for
             the show_bmp_to_lcd function. It uses header guards to prevent
             multiple inclusions, elaborates on function parameters, offers a
             call example and precautions, guiding users to correctly display
             BMP images at a specified position and size on the LCD screen.
Others: Usage requires preservation of original author attribution.
****************************************************************************/

/*
* 头文件保护机制，防止头文件被重复包含。
* 当第一次包含该头文件时，SHOW_BMP_TO_LCD_H 未被定义，会执行后续代码块。
* 后续再次包含时，由于该宏已被定义，会跳过后续代码块，避免重复定义错误。
*/
#ifndef SHOW_BMP_TO_LCD_H
/* 定义宏 SHOW_BMP_TO_LCD_H，以标记该头文件已被包含 */ 
#define SHOW_BMP_TO_LCD_H

/*
* 显示BMP图片函数，增加显示位置和目标尺寸参数。
* 
* @param picname 要显示的BMP图片的文件名，需传入一个以 '\0' 结尾的字符串指针，指向图片文件的路径。
* @param start_x 图片在屏幕上显示的起始 x 坐标，指定图片左上角在屏幕上的水平位置。
* @param start_y 图片在屏幕上显示的起始 y 坐标，指定图片左上角在屏幕上的垂直位置。
* @param target_width 图片显示的目标宽度，即图片在屏幕上最终显示的宽度。
* @param target_height 图片显示的目标高度，即图片在屏幕上最终显示的高度。
*/
void show_bmp_to_lcd(char *picname, int start_x, int start_y, int target_width, int target_height);

/* 结束头文件保护机制的条件编译块 */ 
#endif