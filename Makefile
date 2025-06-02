CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -O2 -I./LIB/lcd_font -I./SYSTEM/show_bmp_to_lcd -I./HARDWARE/LCD -I. -std=gnu99  # 启用gnu99标准，添加当前目录和HARDWARE/LCD目录到头文件搜索路径
LDFLAGS = -L./LIB/lcd_font
LDLIBS = -llcd_font -lm -lpthread

EXEC = p
OBJS = USER/p.o SYSTEM/show_bmp_to_lcd/show_bmp_to_lcd.o HARDWARE/LCD/touchscreen_input.o

all: $(EXEC)
	rm -f $(OBJS)  # 编译完成后删除.o文件

$(EXEC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

USER/p.o: USER/p.c LIB/lcd_font/lcd_font.h SYSTEM/show_bmp_to_lcd/show_bmp_to_lcd.h HARDWARE/LCD/touchscreen_input.o
	$(CC) $(CFLAGS) -c $< -o $@

SYSTEM/show_bmp_to_lcd/show_bmp_to_lcd.o: SYSTEM/show_bmp_to_lcd/show_bmp_to_lcd.c SYSTEM/show_bmp_to_lcd/show_bmp_to_lcd.h
	$(CC) $(CFLAGS) -c $< -o $@

HARDWARE/LCD/touchscreen_input.o: HARDWARE/LCD/touchscreen_input.c HARDWARE/LCD/touchscreen_input.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC)