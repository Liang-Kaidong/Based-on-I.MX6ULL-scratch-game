/* 防止头文件重复包含 */
#ifndef SHOW_GIF_TO_LCD_H
#define SHOW_GIF_TO_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * 播放GIF到LCD屏幕
 * @param filename GIF文件路径
 * @param x 显示起始X坐标
 * @param y 显示起始Y坐标
 * @param width 显示宽度
 * @param height 显示高度
 * @param play_time 播放时间（0为循环播放），单位：秒
 * @return 成功播放的帧数（失败返回-1）
 */
int show_gif_to_lcd(const char *filename, int x, int y, int width, int height, int play_time);

#ifdef __cplusplus
}
#endif

#endif