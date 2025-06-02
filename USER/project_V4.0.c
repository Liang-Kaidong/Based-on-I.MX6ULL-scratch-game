#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "font.h"
#include <stdlib.h>
#include <stdbool.h>
#include <linux/input.h>
#include <time.h>
#define LCD_WIDTH 800
#define LCD_HEIGHT 480

/*
重要提醒：屎山代码,维护不易,介意勿喷！！！
重要提醒：屎山代码,维护不易,介意勿喷！！！
重要提醒：屎山代码,维护不易,介意勿喷！！！
重要的事情说三遍！！！

构建版本：V4.0
构建日期：2024-12-26 11:30:07 UTC+8
维护者：电科1223梁垲东 202111911423

更新日志：
1.新增刮刮乐小游戏功能实现
2.新增游戏加载界面
3.修复了一些已知问题
4.优化了代码结构，提高了代码可读性

温馨提示：为实现良好的游戏体验，本人在主界面、游戏界面等
         加入了许多嵌套循环代码，在未熟知代码的前提下，
         贸然修改循环结构将导致程序的异常崩溃，
         因此，在修改代码结构时，应当充分考虑代码的逻辑，
         确保修改后的代码能够正常运行。
*/

//全局变量定义
int app_click = 0;      //判断是否正确触碰到游戏 0：点击到了游戏 1：未点击游戏+
int x,y;         //触摸屏的坐标
int input_x, input_y;   //点击屏幕的坐标
int error_count = 0;    //错误的次数，请不要把他放到登陆账号功能实现中，会变得很不幸
int remove_state;   //删除账号状态 0：未删除 1：删除成功
int captcha_get_state;  //获取验证码状态 0：未获取 1：获取成功
int captcha_value;   //验证码值
char account_buf[128] = "";     //用于存储账号
char password_buf[128] = "";    //用于存储密码
char hide_password_buf[128] = "";   //用于存储隐藏密码
char user_information_temp_buf[128] = "";   //用于存储临时用户信息 创建账户时使用 
char user_information_temp_buf2[128] = "";  //用于存储临时用户信息 登录时使用
char user_information_temp_buf3[128] = "";  //用于存储临时用户信息 找回时使用
char user_information_buf[128] = "";    //用于存储用户信息 创建账户时使用
char login_account_buf[128]="";     //校验账户
char login_password_buf[128]="";    //校验账户密码
char captcha_buf[10];   //验证码
int touch_fd;//用于表示触摸屏文件描述符
struct input_event ts;//用于存储触摸屏事件数据
int *p;//用于内存映射
int bmp2[800*480];//用于存储奖项图片的像素点

//图片显示功能实现
int show_bmp_to_lcd(char *picname)
{
	//打开lcd、bmp文件
	int lcd_fd = open("/dev/fb0", O_RDWR);
	int bmp_fd = open(picname, O_RDWR);
	if (lcd_fd == -1)
	{
		printf("无法打开屏幕.\n");
		return -1;
	}
	if (bmp_fd == -1)
	{
		printf("无法打开照片.\n");
		return -2;
	}

    //获取bmp图片的颜色数据   bmpbuf:存放bmp图片的颜色数据
	char bmp_buf[800*480*3];
	lseek(bmp_fd, 54, SEEK_SET);	// 偏移文件的光标
	read(bmp_fd, bmp_buf, 800*480*3);

	//写入数据
	//申请映射空间
	int *mmap_start = mmap(NULL, 800*480*4, PROT_READ|PROT_WRITE, MAP_SHARED, lcd_fd, 0);
	if (mmap_start == (void *)-1)
	{
		printf("申请映射空间失败.\n");
		return -3;
	}
	//使用映射空间
	int n=0;
	for(int y=0; y<480; y++)
	{
		for(int x=0; x<800; x++, n+=3)
		{
			*(mmap_start+800*(479-y)+x) =   bmp_buf[n+0]<<0|
									        bmp_buf[n+1]<<8|
									        bmp_buf[n+2]<<16|
									        0<<24;
		}
	}
	//撤销映射空间 --》 内存泄露
	munmap(mmap_start, 800*480*4);

	//关闭文件
	close(lcd_fd);
	close(bmp_fd);
	return 0;
}


//触摸功能实现
int input_fun()
{
    int input_fd = open("/dev/input/event0", O_RDWR);       //打开触摸屏文件
    struct input_event input_buf;
    while(1)
    {
        read(input_fd, &input_buf, sizeof(input_buf));   //读取触摸屏数据:input_buf
        if(input_buf.type == EV_ABS && input_buf.code == ABS_X)   //判断是否是触摸屏事件
        {
            x = input_buf.value;   //获取触摸屏的x坐标
        }
        if(input_buf.type == EV_ABS && input_buf.code == ABS_Y)   //判断是否是触摸屏事件
        {
            y = input_buf.value;   //获取触摸屏的y坐标
        }
        if(input_buf.type == EV_KEY && input_buf.code == BTN_TOUCH && input_buf.value == 0)   //判断是否是触摸屏按下事件
        {
            x = x*800/1024;   //将触摸屏的坐标转换为LCD的坐标
            y = y*480/600;
            input_x = x;
            input_y = y;
            printf("x=%d,y=%d\n",input_x,input_y);   //打印坐标值 debug
            break;
         }
    }
    close(input_fd);
    return 0;
}

/*
//账号和密码文本框功能实现(使用外设实现)  当前已弃用
int account_number_buf_password_number_buf_textbox_fun()
{
    //申请内存映射
    int lcd_fd = open("/dev/fb0", O_RDWR);
    int *p = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);
    //初始化字库 f:字库句柄 "simkai.ttf":字库名字
    font *f = fontLoad("simkai.ttf");
    //设置字体大小 30:字体大小
    fontSetSize(f,30);
    //设置文本框大小 300*40：表示文本框大小 4：表示色素    A R G B
    //账号密码文本框 account_number_buf_textbox：表示账号文本框 password_number_buf_textbox：表示密码文本框
    //account_number_buf_buf：表示账号文本框显示的字符串
    //password_number_buf_buf：表示密码文本框显示的字符串
    //hide_password_number_buf_buf：表示密码文本框显示的字符串，隐藏密码
    //判断变量：m
    char account_number_buf_buf[128]={0};
    char password_number_buf_buf[128]={0};
    char hidepassword_number_buf_buf[128]={0}; //隐藏密码
    bitmap *account_number_buf_textbox = createBitmapWithInit(300, 40, 4,getColor(0,255,255,255)); //白色
    bitmap *password_number_buf_textbox = createBitmapWithInit(300, 40, 4,getColor(0,255,255,255)); //白色
    int m=0;    //判断变量m
    int flag=0; //flag=0表示账号输入，flag=1表示密码输入   
    //将字体输入到文本框内 account_number_buf_textbox:表示文本框 password_number_buf_textbox:表示密码文本框 0,0：表示字体显示的位置
    //fontPrint(f,account_number_buf_textbox,0,0,account_number_buf_buf,getColor(0,0,0,0),0); 
    //fontPrint(f,password_number_buf_textbox,0,0,password_number_buf_buf,getColor(0,0,0,0),0); 
    //将文本框显示到LCD上 81,200：文本框在LCD上显示的起始位置
    show_font_to_lcd(p,81,200,account_number_buf_textbox);
    show_font_to_lcd(p,81,245,password_number_buf_textbox);


    //方法一：通过回车输入账号密码
    while(1)
    {
        //AO表示中间值，用来判断输入的内容
        char AO=getchar();
        if(AO =='\n')   //跳过回车
        {
            AO=' ';
            continue;
        }
        else if(AO == '`')  //切换密码
        {
            flag=1;
            m=0;//清空数组下标，不影响账号密码输入
        }

        //判断账号密码长度是否超过界限
        if(m<10)    
        {
            if(flag==0)
            {
                account_number_buf_buf[m]=AO;
                m++;
            }
            else
            {
                password_number_buf_buf[m]=AO;
                m++;
            }
        }

        //显示修改
        if(flag==0)
        {
            fontPrint(f,account_number_buf_textbox,5,4,account_number_buf_buf,getColor(0,0,0,0),0); 
            show_font_to_lcd(p,150,150,account_number_buf_textbox);
        }
        else
        {
            fontPrint(f,password_number_buf_textbox,5,4,password_number_buf_buf,getColor(0,0,0,0),0); 
            show_font_to_lcd(p,150,250,password_number_buf_textbox);
        }
    }

    //方法二：通过关闭缓冲区实现输入账号密码
    while(1)
    {
        //关闭缓冲区(实现不敲回车输入账号密码)
        system("stty -icanon");
        //AO表示中间值，用来判断输入的内容
        char AO=getchar();
        if(AO == '`')  //切换密码
        {
            AO=' ';
            flag=1;
            m=0;//清空数组下标，不影响账号密码输入
        }
        else if(AO == '-')  
        {
            AO=' ';
            if(m > 0)//判断是否有内容
                m--;
                if(flag == 0)
                {
                    account_number_buf_buf[m]=AO;  
                    account_number_buf_textbox = createBitmapWithInit(300, 40, 4,getColor(0,255,255,255)); //再次调用，实现清除数据功能，刷新文本框
                }
                else
                {
                    password_number_buf_buf[m]=AO;
                    hidepassword_number_buf_buf[m]=AO; 
                    password_number_buf_textbox = createBitmapWithInit(300, 40, 4,getColor(0,255,255,255)); //再次调用，实现清除数据功能，刷新文本框
                }
        }
        else if(AO == '=' && flag == 1)//确认密码已输入,登入
        {
            printf("账号:%s\n密码:%s\n",account_number_buf_buf,password_number_buf_buf);
            break;
        }

        //判断账号密码长度是否超过界限
        else if(m < 128 && AO != '=')    
        {
            if(flag == 0)
            {
                account_number_buf_buf[m]=AO;
                m++;
            }
            else
            {
                password_number_buf_buf[m]=AO;
                hidepassword_number_buf_buf[m]='*'; //隐藏密码
                m++;
            }
        }

        //显示修改
        if(flag == 0)
        {
            fontPrint(f,account_number_buf_textbox,5,4,account_number_buf_buf,getColor(0,0,0,0),0); 
            show_font_to_lcd(p,81,200,account_number_buf_textbox);
        }
        else
        {
            fontPrint(f,password_number_buf_textbox,5,4,hidepassword_number_buf_buf,getColor(0,0,0,0),0); 
            show_font_to_lcd(p,81,245,password_number_buf_textbox);
        }
    }
    destroyBitmap(account_number_buf_textbox);
    destroyBitmap(password_number_buf_textbox);
    close(lcd_fd);
    fontUnload(f);
    munmap(p,800*480*4);
    return 0;
}
*/

int game_begin_fun()
{
    int event(int *x,int *y)
    {
        read(touch_fd,&ts,sizeof(ts));
        if(ts.type == 3 && ts.code == 0)
            *x = ts.value*800/1022;
        if(ts.type == 3 && ts.code == 1)
            *y = ts.value*480/598;
    }

    /*该函数用于显示指定的 BMP 图片。首先打开图片文件，跳过文件头的 54 个字节，读取图片的像素数据，并将其转换为 RGB 格式存储在 bmp 数组中。
    然后将图片数据上下颠倒存储在 bmp1 数组中。最后根据指定的位置和缩放倍数，将图片数据写入到内存映射区域 p 中，从而在液晶屏上显示图片*/
    int show_bmp(char *bmpname, int x0, int y0) 
    {
        int bmp_fd = open(bmpname, O_RDWR);
        if (bmp_fd == -1) {
            printf("打开图片失败\n");
            return -1;
        }
        char buf[800 * 480 * 3] = {0};
        lseek(bmp_fd, 54, SEEK_SET); // 跳过 BMP 文件头
        read(bmp_fd, buf, 800 * 480 * 3); // 读取 BMP 像素数据

        int bmp[800 * 480] = {0};
        for (int i = 0; i < 800 * 480; i++) {
            bmp[i] = buf[i * 3 + 0] << 0 | buf[i * 3 + 1] << 8 | buf[i * 3 + 2] << 16;
        }

        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 800; x++) {
                p[(y + y0) * 800 + (x + x0)] = bmp[(479 - y) * 800 + x];
            }
        }
        close(bmp_fd);
        return 0;
    }
    /*该函数与 show_bmp 函数类似，也是读取 BMP 图片的像素数据，但不进行显示，而是将处理后的像素数据存储在全局数组 bmp2 中，用于后续刮奖操作*/
    //获取图片像素点
    int bmp_rgb(char *bmpname)
    {
        //打开图片
        int bmp_fd = open(bmpname,O_RDWR);
        if(bmp_fd == -1)
        {
            printf("打开图片失败\n"); 
            return -1;
        }
        char buf[800*480*3]={0};
        //跳过表示图片信息的54个字节
        lseek(bmp_fd,54,SEEK_SET);
        //读取图片像素点
        read(bmp_fd,buf,800*480*3);
        //补充一个字节
        int bmp[800*480]={0};
        for(int i=0;i<800*480;i++)//i:表示每一个像素点
        {
            //           B             G             R
            bmp[i]=buf[i*3+0]<<0|buf[i*3+1]<<8|buf[i*3+2]<<16;
        }
        //数据上下颠倒
        for(int y=0;y<480;y++)
        {
            for(int x=0;x<800;x++)
            {
                bmp2[800*y+x]=bmp[(479-y)*800+x];
            }
        }
        close(bmp_fd);
        return 0;
    }

    /*该函数实现刮奖效果。以指定的坐标 (x0, y0) 为圆心，半径 r 为 30 的圆形区域内，将内存映射区域 p 中的像素值替换为奖项图片的像素值 bmp2，
    从而实现刮奖效果*/
    //刮奖
    int gua(int x0,int y0)
    {
        int r = 30;
        for(int y = y0-r;y<=y0+r;y++)
        {
            for(int x = x0-r;x<=x0+r;x++)
            {
                if((x-x0)*(x-x0) + (y-y0)*(y-y0) < r*r)
                {
                    p[800*y+x]=bmp2[800*y+x];
                } 
            }
        }  
    }

// 绘制文本框函数
void draw_textbox(int x, int y, const char *text)
{
    // 假设 p 是内存映射的数据区，您需要在这里绘制矩形框以及文本
    // 在 (x, y) 位置绘制一个矩形框

    // 设置文本框的边界
    for (int i = 0; i < 160; i++)
    {
        for (int j = 0; j < 48; j++) // 假设文本框高度为48
        {
            if (i < 5 || i >= 155 || j < 5 || j >= 43) // 5像素宽的边框
            {
                p[(y + j) * 800 + (x + i)] = 0xFFFFFF; // 白色边框
            }
            else
            {
                p[(y + j) * 800 + (x + i)] = 0x000000; // 黑色背景
            }
        }
    }

    // 在文本框中绘制文本
    // 这里需要一些简单的文字渲染，具体文本处理会依赖于您的实现（简化处理）
    // 假设我们用ASCII字符进行简单表示，实际应用中可以使用图形库如库来绘制字体
    // 这里只是示例并不包含具体的字体绘制逻辑
    int text_length = strlen(text);
    for (int i = 0; i < text_length; i++)
    {
        // 假设每个字符占8x16区域（实际情况看您字符实现）
        // 用一个简单的颜色来代表字符
        for (int j = 0; j < 16; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                // 在这里简单的绘制字符的像素，实际需要具体字符的点阵数据
                p[(y + 10 + j) * 800 + (x + 10 + i * 8 + k)] = 0xFFFF00; // 黄色显示字符
            }
        }
    }
}

// 游戏主函数
int game_main()
{
    srand(time(NULL));
    int n = rand() % 100; 
    int lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1)
    {
        printf("打开液晶屏失败\n"); 
        return -1;
    }
    p = mmap(NULL, 800 * 480 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, lcd_fd, 0);
    touch_fd = open("/dev/input/event0", O_RDWR);
    if (touch_fd == -1)
    {
        printf("打开触摸屏失败\n");
        return -1;
    }
    show_bmp("1.bmp", 0, 0); 
    show_bmp("load.bmp", 0, 0); 
    usleep(1000000); // 等待1秒
    show_bmp("1.bmp", 0, 0); 
    if (n <= 9) bmp_rgb("one.bmp");
    else if (n > 9 && n <= 29) bmp_rgb("two.bmp");
    else if (n > 29 && n <= 59) bmp_rgb("three.bmp");
    else bmp_rgb("xie.bmp");

    int x, y;
    int goua_done = 0;

    while (1)
    {
        event(&x, &y);

        // 检测是否点击返回按钮
        if (goua_done && x >= 0 && x < 160 && y >= 432 && y < 480)
        {
            printf("返回按钮被点击，重新游戏！\n");
            show_bmp("1.bmp", 0, 0);
            n = rand() % 100; 
            if (n <= 9) bmp_rgb("one.bmp");
            else if (n > 9 && n <= 29) bmp_rgb("two.bmp");
            else if (n > 29 && n <= 59) bmp_rgb("three.bmp");
            else bmp_rgb("xie.bmp");
            goua_done = 0; // 重置状态标志
            continue;
        }

        if (x >= 30 && x <= 770 && y >= 30 && y <= 450)
        {
            gua(x, y); 
            goua_done = 1; // 设置标志位，表示已进行刮奖操作
        }

        // 在进行刮奖后显示返回按钮
        if (goua_done)
        {
            draw_textbox(0, 432, "返回"); // 在指定位置绘制返回按钮文本框
        }
    }

    munmap(p, 800 * 480 * 4);
    close(lcd_fd);
    close(touch_fd);
    return 0;
}
game_main();
}

//未点开游戏文本框提示功能实现
int no_game_textbox_fun()
{
    int lcd_fd = open("/dev/fb0", O_RDWR);
    int *p = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);
    //初始化字库 f:字库句柄 "simkai.ttf":字库名字
    font *f = fontLoad("simkai.ttf");
    //设置字体大小 30:字体大小
    fontSetSize(f,30);
    //设置文本框大小 400*40：表示文本框大小 4：表示色素    A R G B
    //未点开游戏文本框 no_game_textbox：表示未点开游戏文本框
    //no_game_buf：表示未点开游戏文本框显示的字符串
    bitmap *no_game_textbox = createBitmapWithInit(400, 40, 4,getColor(0,128,128,128)); //灰色
    char no_game_buf[128]="未点击到游戏，请再次点开。";
    fontPrint(f,no_game_textbox,5,4,no_game_buf,getColor(0,255,255,255),0); 
    show_font_to_lcd(p,200,330,no_game_textbox);
    close(lcd_fd);
    fontUnload(f);
    munmap(p,800*480*4);
    destroyBitmap(no_game_textbox);
    return 0;
}

//账号密码输入以及虚拟键盘功能实现
int input_fd;   //触摸屏文件描述符
struct input_event input_buf;   //存储触摸屏数据
int input_event(int *input_x, int *input_y)   //获取点击屏幕坐标的事件
{
    read(input_fd, &input_buf, sizeof(input_buf));
    if (input_buf.type == 3 && input_buf.code == 0)
        *input_x = input_buf.value * 800 / 1022;
    if (input_buf.type == 3 && input_buf.code == 1)
        *input_y = input_buf.value * 480 / 598;
    //printf("x=%d,y=%d\n",*input_x,*input_y);   //debug
}

//登录账号密码功能实现
int login_fun()
{
    int lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1) 
    {
        printf("打开液晶屏失败.\n");
        return -1;
    }
    input_fd = open("/dev/input/event0", O_RDWR);
    if (input_fd == -1) {
        printf("打开触摸屏失败.\n");
        return -2;
    }
    
    show_bmp_to_lcd("login_initialization.bmp");    //显示登陆账号初始界面
    while(1)
    {   
        input_fun();   //获取触摸屏坐标
        if(input_x >= 65 && input_x <= 170 && input_y >= 329 && input_y <= 376)   //点击登录按钮 登陆账户user
            {   
                int login_user_fun()
                {
                show_bmp_to_lcd("login_2.bmp");    //显示登陆账号界面
                int *p = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);     //内存映射
                font *f = fontLoad("simkai.ttf");    //初始化字库 f:字库句柄 "simkai.ttf":字库名字
                fontSetSize(f,30);  //设置字体大小 30:字体大小

                //设置文本框大小 80*50:表示文本框大小 4：表示色素     A  R  G  B  （0123456789，+，-，确认，删除，清零）
                bitmap *number_textbox_1 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字1的文本框
                bitmap *number_textbox_2 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字2的文本框
                bitmap *number_textbox_3 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字3的文本框
                bitmap *number_textbox_4 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字4的文本框
                bitmap *number_textbox_5 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字5的文本框
                bitmap *number_textbox_6 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字6的文本框
                bitmap *number_textbox_7 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字7的文本框
                bitmap *number_textbox_8 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字8的文本框
                bitmap *number_textbox_9 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字9的文本框
                bitmap *number_textbox_0 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字0的文本框
                bitmap *delete_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //删除的文本框
                bitmap *clean_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));     //清空的文本框
                bitmap *sum_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));      //加号的文本框
                bitmap *sub_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));      //减号的文本框
                bitmap *O_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));         //"O"的文本框
                bitmap *K_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));         //"K"的文本框
                bitmap *account_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));   //账号的文本框
                bitmap *password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255));     //密码的文本框
                bitmap *hide_password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255)); //隐藏密码的文本框
                bitmap *login_len_over_textbox = createBitmapWithInit(550,40,4,getColor(0,128,128,128)); //账号长度超过界限的文本框
                bitmap *login_none_exist_textbox = createBitmapWithInit(400,40,4,getColor(0,128,128,128)); //账号不存在的文本框
                bitmap *login_success_textbox = createBitmapWithInit(500,40,4,getColor(0,128,128,128)); //账号登录成功
                bitmap *login_fail_textbox = createBitmapWithInit(450,40,4,getColor(0,128,128,128));   //账号或密码错误提示的文本框
                bitmap *login_fail_over_textbox = createBitmapWithInit(450,40,4,getColor(0,128,128,128)); //密码错误超过3次的文本框
                
                //字符数组
                char number_buf_1[21]="1";
                char number_buf_2[21]="2";
                char number_buf_3[21]="3";
                char number_buf_4[21]="4";
                char number_buf_5[21]="5";
                char number_buf_6[21]="6";
                char number_buf_7[21]="7";
                char number_buf_8[21]="8";
                char number_buf_9[21]="9";
                char number_buf_0[21]="0";
                char delete_buf[21]="删除";
                char clean_buf[21]="全清";
                char sum_buf[21]="爱";
                char sub_buf[21]="你";
                char O_buf[21]="确";
                char K_buf[21]="认";
                char login_len_over_buf[128]="账号或密码长度超过限制，请重新输入！";
                char login_none_exist_buf[128]="该账号不存在，请重新输入！";
                char login_success_buf[128]="登录成功，请尽情游玩刮刮乐游戏！";
                char login_fail_buf[128]="账号或密码错误，请重新输入！";
                char login_fail_over_buf[128]="密码错误3次，账号已被锁定！";

                //将字体输入到文本框内 number_textbox:表示数字的文本框 0,0：表示字体显示的起始位置
                fontPrint(f,number_textbox_1,30,25,number_buf_1,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_2,30,25,number_buf_2,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_3,30,25,number_buf_3,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_4,30,25,number_buf_4,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_5,30,25,number_buf_5,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_6,30,25,number_buf_6,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_7,30,25,number_buf_7,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_8,30,25,number_buf_8,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_9,30,25,number_buf_9,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_0,33,25,number_buf_0,getColor(0,0,0,0),0);
                fontPrint(f,delete_textbox,0,25,delete_buf,getColor(0,0,0,0),0);
                fontPrint(f,clean_textbox,0,25,clean_buf,getColor(0,0,0,0),0);
                fontPrint(f,sum_textbox,25,25,sum_buf,getColor(0,0,0,0),0);
                fontPrint(f,sub_textbox,22,25,sub_buf,getColor(0,0,0,0),0);
                fontPrint(f,O_textbox,15,25,O_buf,getColor(0,0,0,0),0);
                fontPrint(f,K_textbox,20,25,K_buf,getColor(0,0,0,0),0);
                fontPrint(f,account_textbox,0,7,account_buf,getColor(0,0,0,0),0);
                fontPrint(f,password_text,0,7,hide_password_buf,getColor(0,0,0,0),0);
                fontPrint(f,login_len_over_textbox,5,4,login_len_over_buf,getColor(0,255,255,255),0);
                fontPrint(f,login_none_exist_textbox,5,4,login_none_exist_buf,getColor(0,255,255,255),0);
                fontPrint(f,login_success_textbox,5,4,login_success_buf,getColor(0,255,255,255),0);
                fontPrint(f,login_fail_textbox,5,4,login_fail_buf,getColor(0,255,255,255),0);
                fontPrint(f,login_fail_over_textbox,5,4,login_fail_over_buf,getColor(0,255,255,255),0);

                //将文本框显示到LCD上 400,150：文本框在LCD上显示的起始位置
                //第一行(1,2,3,删除)
                show_font_to_lcd(p,400,150,number_textbox_1);
                show_font_to_lcd(p,480,150,number_textbox_2);
                show_font_to_lcd(p,560,150,number_textbox_3);
                show_font_to_lcd(p,640,150,delete_textbox);
                //第二行(30,25,6,全清)
                show_font_to_lcd(p,400,230,number_textbox_4);
                show_font_to_lcd(p,480,230,number_textbox_5);
                show_font_to_lcd(p,560,230,number_textbox_6);
                show_font_to_lcd(p,640,230,clean_textbox);
                //第三行(7,8,9,确)
                show_font_to_lcd(p,400,310,number_textbox_7);
                show_font_to_lcd(p,480,310,number_textbox_8);
                show_font_to_lcd(p,560,310,number_textbox_9);
                show_font_to_lcd(p,640,310,O_textbox);
                //第四行(加,0,减,认)
                show_font_to_lcd(p,400,390,sum_textbox);
                show_font_to_lcd(p,480,390,number_textbox_0);
                show_font_to_lcd(p,560,390,sub_textbox);
                show_font_to_lcd(p,640,390,K_textbox);
                //账号密码的文本框显示
                show_font_to_lcd(p,81,200,account_textbox);
                show_font_to_lcd(p,81,250,password_text);

                int out_put_state = 0; // 0: 输入账号, 1: 输入密码
                int touch_state = 0; // 触摸状态 0: 未触摸, 1: 已触摸

                while(2)
                {
                    input_event(&input_x, &input_y);   // 只有在触摸按键时才进行处理
                    if (input_buf.type == 1 && input_buf.code == 330 && input_buf.value == 1)   // input_buf.value == 1 表示触摸按下
                    { 
                        // 键盘触摸判断区域
                        if (input_x >= 506 && input_x <= 558 && input_y >= 409 && input_y <= 469)    //数字0触摸区域
                        {
                            if (out_put_state == 0)     // 输入账号
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {  
                                    strcat(account_buf, "0");   //账号输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else    // 输入密码
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "0");   //密码输入0 (拼接之前的数据)
                                    strcat(hide_password_buf, "*");    //隐藏密码输入* (拼接之前的数据)
                                    touch_state = 1;
                                }
                            }
                        }
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0)    // 输入账号
                            {
                                if (touch_state == 0) 
                                {  
                                    strcat(account_buf, "1");   //账号输入1(拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "1");   //密码输入1 (拼接之前的数据)
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "2");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "2");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "3");
                                    touch_state = 1;
                                }
                            } 
                            else
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "3");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0 && strlen(account_buf) > 0) 
                            {
                                account_buf[strlen(account_buf) - 1] = '\0';  // 删除账号的最后一个字符
                                touch_state = 1;
                            } 
                            else if (out_put_state == 1 && strlen(password_buf) > 0) 
                            {
                                password_buf[strlen(password_buf) - 1] = '\0';  // 删除密码的最后一个字符
                                hide_password_buf[strlen(hide_password_buf) - 1] = '\0';     // 删除隐藏密码的最后一个字符
                                touch_state = 1;
                            }
                        } 
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 252 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "4");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "4");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "5");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "5");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "6");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "6");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号
                                touch_state = 1;
                            } 
                            else 
                            {
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码
                                touch_state = 1;
                            }
                        } 
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 315 && input_y <= 371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "7");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "7");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >=315 && input_y <=371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "8");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "8");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >=315 && input_y <=371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "9");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "9");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >=315 && input_y <=475)    //确认键
                        {
                            while (2) 
                            {
                                if (out_put_state == 0) 
                                {
                                    if (touch_state == 0) 
                                    {
                                        //strcat(account_buf, "0");     //确认键设定为无效
                                        touch_state = 1;
                                        break;    //跳出输入账号密码界面循环
                                    }
                                } 
                                else 
                                {
                                    if (touch_state == 0) 
                                    {
                                        //strcat(password_buf, "0");     //确认键设定为无效
                                        touch_state = 1;
                                        break;    //跳出输入账号密码界面循环
                                    }
                                }
                            }
                            //等待账号输入完毕再判断是否超过界限
                            //检测是否超过账号和密码长度限制
                            int account_len = strlen(account_buf);
                            int password_len = strlen(password_buf);
                            int hide_password_len = strlen(hide_password_buf);
                            if (account_len > 12 || password_len > 12 || hide_password_len > 12 ) 
                            {   
                                show_font_to_lcd(p, 130, 330, login_len_over_textbox);    //显示账号或密码长度超过限制的提示
                                usleep(2000000);    //延时2秒
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                login_fun();
                                //printf("账号或密码长度超过限制\n");   //debug
                            }
                            else 
                            {   
                                //校验是否存在账号(与创建账户功能实现比较)
                                //存放拼接后的账号 账号.txt
                                sprintf(user_information_temp_buf2, "%s.txt", account_buf); //将已输入好的账号数据写入用户信息临时文件2的buf中
                                //打开个人文档
                                int user_information_fd = open(user_information_temp_buf2,O_RDWR);   //在本地检验是否存在该文件（与创建用户成功后保留的文档比较）
                                if(user_information_fd == -1)   //不存在的情况(与创建账户功能实现比较)
                                {
                                    show_font_to_lcd(p, 200, 330, login_none_exist_textbox);    //显示账号不存在的提示
                                    usleep(2000000);    //延时2秒
                                    memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                    memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                    memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框    
                                    login_fun();                                 
                                }
                                else    //存在的情况
                                {   
                                    char user_information_temp_buf3[256];
                                    sprintf(user_information_temp_buf3, "%s.txt", account_buf); //将已输入好的账号密码数据写入用户信息临时文件3的buf中
                                    //将个人文档内的数据读取出来
                                    read(user_information_fd,user_information_temp_buf,256);    //已创建的用户数据
                                    //分割数据符号
                                    char seqs[] = ",";
                                    char *tmp = strtok(user_information_temp_buf,seqs);
                                    strcpy(login_account_buf,tmp + strlen("账号："));
                                    tmp = strtok(NULL,seqs);
                                    strcpy(login_password_buf,tmp + strlen("密码："));
                                }

                                //密码是否匹配
                                if(strcmp(password_buf,login_password_buf) == 0) //当前账户密码等于已存放的密码
                                {
                                    //printf("登录成功.\n");
                                    show_font_to_lcd(p, 130, 330, login_success_textbox);    //显示登录成功的提示
                                    usleep(2000000);    //延时2秒
                                    memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框      
                                    memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                    memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                    game_begin_fun();   //调用游戏开始功能实现
                                }
                                else
                                {   
                                    for(error_count;error_count <2;error_count++)     //不要怀疑，时间告诉我超过三次就是这样写
                                    {
                                        //printf("密码错误.\n");
                                        show_font_to_lcd(p, 180, 330, login_fail_textbox);    //显示密码错误的提示
                                        usleep(2000000);    //延时2秒
                                        show_bmp_to_lcd("login_2.bmp");    //显示登录初始化界面
                                        memset(account_buf, 0, sizeof(account_buf));  //清空账号输入框
                                        memset(password_buf, 0, sizeof(password_buf));  //清空密码输入框
                                        memset(hide_password_buf, 0, sizeof(hide_password_buf));     //清空隐藏密码输入框
                                        error_count++;
                                        printf("密码错误次数：%d\n",error_count);
                                        login_fun();   //调用账号密码输入功能实现
                                        break;
                                    }
                                    printf("密码错误次数超过3次,账号已被冻结！\n");
                                    show_font_to_lcd(p, 180, 330, login_fail_over_textbox);    //显示密码错误3次的提示
                                    memset(account_buf, 0, sizeof(account_buf));  //清空账号输入框
                                    memset(password_buf, 0, sizeof(password_buf));  //清空密码输入框
                                    memset(hide_password_buf, 0, sizeof(hide_password_buf));    //清空隐藏密码输入框
                                    usleep(1000000);    //延时1秒     
                                    login_fun();   //调用账号密码输入功能实现
                                    break;
                                }
                                
                                close(user_information_fd);
                                show_bmp_to_lcd("login_2.bmp");    //显示登录初始化界面
                            }
                        }

                        else if (input_x >= 81 && input_x <= 363 && input_y >= 200 && input_y <= 249) 
                        {
                            // 点击到输入账号的区域
                            out_put_state = 0;
                            touch_state = 1;
                        } 
                        else if (input_x >= 81 && input_x <= 363 && input_y >= 250 && input_y <= 290) 
                        {
                            // 输入密码
                            out_put_state = 1;
                            touch_state = 1;
                        }
                    }

                    // 如果触摸事件结束（input_buf.value == -1），则重置触摸状态
                    if (input_buf.type != 1 || input_buf.code != 330 || input_buf.value != 1) 
                    {
                        touch_state = 0;  // 触摸结束，允许下一次触摸识别
                        //printf("触摸坐标: (%d, %d)\n", input_x, input_y);     //debug
                    }

                    //刷新文本框，防止文字重叠
                    bitmap *account_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *hide_password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    fontPrint(f,account_textbox,0,7,account_buf,getColor(0,0,0,0),0);
                    fontPrint(f,password_text,0,7,hide_password_buf,getColor(0,0,0,0),0);
                    show_font_to_lcd(p,81,200,account_textbox);
                    show_font_to_lcd(p,81,250,password_text);
                }


                //debug账号密码输入功能测试
                //printf("账号: %s\n", account_buf);
                //printf("密码: %s\n", password_buf);
                //printf("隐藏密码: %s\n", hide_password_buf);

                //关闭文件
                close(lcd_fd);
                close(input_fd);
                fontUnload(f);
                destroyBitmap(account_textbox);
                destroyBitmap(password_text);
                destroyBitmap(login_len_over_textbox);
                destroyBitmap(login_none_exist_textbox);
                destroyBitmap(login_success_textbox);
                destroyBitmap(login_fail_textbox);
                destroyBitmap(login_fail_over_textbox);
                munmap(p, 800 * 480 * 4);
                }
                login_user_fun();
                break;    
            }

        if (input_x >= 201 && input_x <= 291 && input_y >= 349 && input_y <= 375)    //点击创建按钮 创建账户user
        {   
                int create_user_fun()
                {
                show_bmp_to_lcd("register.bmp");    //显示游戏注册界面
                int lcd_fd = open("/dev/fb0", O_RDWR);
                if (lcd_fd == -1) 
                {
                    printf("打开液晶屏失败.\n");
                    return -1;
                }
                input_fd = open("/dev/input/event0", O_RDWR);
                if (input_fd == -1) {
                    printf("打开触摸屏失败.\n");
                    return -2;
                }
                
                int *p = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);     //内存映射
                font *f = fontLoad("simkai.ttf");    //初始化字库 f:字库句柄 "simkai.ttf":字库名字
                fontSetSize(f,30);  //设置字体大小 30:字体大小

                //设置文本框大小 80*50:表示文本框大小 4：表示色素     A  R  G  B  （0123456789，+，-，确认，删除，清零）
                bitmap *number_textbox_1 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字1的文本框
                bitmap *number_textbox_2 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字2的文本框
                bitmap *number_textbox_3 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字3的文本框
                bitmap *number_textbox_4 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字4的文本框
                bitmap *number_textbox_5 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字5的文本框
                bitmap *number_textbox_6 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字6的文本框
                bitmap *number_textbox_7 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字7的文本框
                bitmap *number_textbox_8 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字8的文本框
                bitmap *number_textbox_9 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字9的文本框
                bitmap *number_textbox_0 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字0的文本框
                bitmap *delete_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //删除的文本框
                bitmap *clean_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));     //清空的文本框
                bitmap *sum_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));      //加号的文本框
                bitmap *sub_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));      //减号的文本框
                bitmap *O_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));         //"O"的文本框
                bitmap *K_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));         //"K"的文本框
                bitmap *account_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));   //账号的文本框
                bitmap *password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255));     //密码的文本框
                bitmap *hide_password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255)); //隐藏密码的文本框
                bitmap *create_len_over_textbox = createBitmapWithInit(550, 40, 4,getColor(0,128,128,128)); //长度超过限制提示文本框 灰色
                bitmap *create_exist_textbox = createBitmapWithInit(400, 40, 4,getColor(0,128,128,128)); //账号已存在提示文本框 灰色
                bitmap *create_success_textbox = createBitmapWithInit(500, 40, 4,getColor(0,128,128,128)); //注册成功提示文本框 灰色
                //字符数组
                char number_buf_1[21]="1";
                char number_buf_2[21]="2";
                char number_buf_3[21]="3";
                char number_buf_4[21]="4";
                char number_buf_5[21]="5";
                char number_buf_6[21]="6";
                char number_buf_7[21]="7";
                char number_buf_8[21]="8";
                char number_buf_9[21]="9";
                char number_buf_0[21]="0";
                char delete_buf[21]="删除";
                char clean_buf[21]="全清";
                char sum_buf[21]="爱";
                char sub_buf[21]="你";
                char O_buf[21]="确";
                char K_buf[21]="认";
                char create_len_over_buf[128]="账号或密码长度超过限制，请重新输入！";
                char create_exist_buf[128]="该账号已存在，请重新注册！";
                char create_success_buf[128]="注册成功，请牢记您的账号和密码！";

                //将字体输入到文本框内 number_textbox:表示数字的文本框 0,0：表示字体显示的起始位置
                fontPrint(f,number_textbox_1,30,25,number_buf_1,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_2,30,25,number_buf_2,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_3,30,25,number_buf_3,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_4,30,25,number_buf_4,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_5,30,25,number_buf_5,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_6,30,25,number_buf_6,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_7,30,25,number_buf_7,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_8,30,25,number_buf_8,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_9,30,25,number_buf_9,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_0,33,25,number_buf_0,getColor(0,0,0,0),0);
                fontPrint(f,delete_textbox,0,25,delete_buf,getColor(0,0,0,0),0);
                fontPrint(f,clean_textbox,0,25,clean_buf,getColor(0,0,0,0),0);
                fontPrint(f,sum_textbox,25,25,sum_buf,getColor(0,0,0,0),0);
                fontPrint(f,sub_textbox,22,25,sub_buf,getColor(0,0,0,0),0);
                fontPrint(f,O_textbox,15,25,O_buf,getColor(0,0,0,0),0);
                fontPrint(f,K_textbox,20,25,K_buf,getColor(0,0,0,0),0);
                fontPrint(f,account_textbox,0,7,account_buf,getColor(0,0,0,0),0);
                fontPrint(f,password_text,0,7,hide_password_buf,getColor(0,0,0,0),0);
                fontPrint(f,create_len_over_textbox,5,4,create_len_over_buf,getColor(0,255,255,255),0);
                fontPrint(f,create_exist_textbox,5,4,create_exist_buf,getColor(0,255,255,255),0);
                fontPrint(f,create_success_textbox,5,4,create_success_buf,getColor(0,255,255,255),0);


                //将文本框显示到LCD上 400,150：文本框在LCD上显示的起始位置
                //第一行(1,2,3,删除)
                show_font_to_lcd(p,400,150,number_textbox_1);
                show_font_to_lcd(p,480,150,number_textbox_2);
                show_font_to_lcd(p,560,150,number_textbox_3);
                show_font_to_lcd(p,640,150,delete_textbox);
                //第二行(30,25,6,全清)
                show_font_to_lcd(p,400,230,number_textbox_4);
                show_font_to_lcd(p,480,230,number_textbox_5);
                show_font_to_lcd(p,560,230,number_textbox_6);
                show_font_to_lcd(p,640,230,clean_textbox);
                //第三行(7,8,9,确)
                show_font_to_lcd(p,400,310,number_textbox_7);
                show_font_to_lcd(p,480,310,number_textbox_8);
                show_font_to_lcd(p,560,310,number_textbox_9);
                show_font_to_lcd(p,640,310,O_textbox);
                //第四行(加,0,减,认)
                show_font_to_lcd(p,400,390,sum_textbox);
                show_font_to_lcd(p,480,390,number_textbox_0);
                show_font_to_lcd(p,560,390,sub_textbox);
                show_font_to_lcd(p,640,390,K_textbox);
                //账号密码的文本框显示
                show_font_to_lcd(p,81,200,account_textbox);
                show_font_to_lcd(p,81,250,password_text);

                int out_put_state = 0; // 0: 输入账号, 1: 输入密码
                int touch_state = 0; // 触摸状态 0: 未触摸, 1: 已触摸

                while(2)
                {
                    input_event(&input_x, &input_y);   // 只有在触摸按键时才进行处理
                    if (input_buf.type == 1 && input_buf.code == 330 && input_buf.value == 1)   // input_buf.value == 1 表示触摸按下
                    { 
                        // 键盘触摸判断区域
                        if (input_x >= 506 && input_x <= 558 && input_y >= 409 && input_y <= 469)    //数字0触摸区域
                        {
                            if (out_put_state == 0)     // 输入账号
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {  
                                    strcat(account_buf, "0");   //账号输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else    // 输入密码
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "0");   //密码输入0 (拼接之前的数据)
                                    strcat(hide_password_buf, "*");    //隐藏密码输入* (拼接之前的数据)
                                    touch_state = 1;
                                }
                            }
                        }
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0)    // 输入账号
                            {
                                if (touch_state == 0) 
                                {  
                                    strcat(account_buf, "1");   //账号输入1(拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "1");   //密码输入1 (拼接之前的数据)
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "2");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "2");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "3");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "3");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0 && strlen(account_buf) > 0) 
                            {
                                account_buf[strlen(account_buf) - 1] = '\0';  // 删除账号的最后一个字符
                                touch_state = 1;
                            } 
                            else if (out_put_state == 1 && strlen(password_buf) > 0) 
                            {
                                password_buf[strlen(password_buf) - 1] = '\0';  // 删除密码的最后一个字符
                                hide_password_buf[strlen(hide_password_buf) - 1] = '\0';     // 删除隐藏密码的最后一个字符
                                touch_state = 1;
                            }
                        } 
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 252 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "4");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "4");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "5");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "5");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "6");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "6");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号
                                touch_state = 1;
                            } 
                            else 
                            {
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码
                                touch_state = 1;
                            }
                        } 
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 315 && input_y <= 371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "7");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "7");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >=315 && input_y <=371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "8");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "8");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >=315 && input_y <=371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "9");
                                    touch_state = 1;
                                }
                            } 
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "9");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >=315 && input_y <=475)    // 确认键
                        {
                            while(2)    //预先输入完账号和密码后才开始判定，以免同步buf是意外结束
                            {
                                if (out_put_state == 0) 
                                {
                                    if (touch_state == 0) 
                                    {
                                        //strcat(account_buf, "0");     //确认键设定为无效
                                        touch_state = 1;
                                        break;    
                                    }
                                } 
                                else 
                                {
                                    if (touch_state == 0) 
                                    {
                                        //strcat(password_buf, "0");     //确认键设定为无效
                                        touch_state = 1;
                                        break;    
                                    }
                                }                    
                            }

                            //用于判断账号或密码是否超过限制，若超过，则账号和密码不记录，若未超过，则创建账号和密码
                            //检测是否超过账号和密码长度限制
                            int account_len = strlen(account_buf);
                            int password_len = strlen(password_buf);
                            int hide_password_len = strlen(hide_password_buf);
                            if (account_len > 12 || password_len > 12 || hide_password_len > 12 ) 
                            {
                                show_font_to_lcd(p,130,330,create_len_over_textbox);
                                usleep(2000000);
                                show_bmp_to_lcd("register.bmp");    //显示创建账号和密码初始界面
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                login_fun();   //调用账号密码功能实现
                            }
                            else    //存放账号数据
                            {
                                //存放拼接后的账号 账号.txt
                                sprintf(user_information_temp_buf, "%s.txt", account_buf);
                                //创建个人文档
                                int user_information_fd = open(user_information_temp_buf, O_RDWR | O_CREAT | O_EXCL);
                                if(user_information_fd == -1)
                                {
                                    show_font_to_lcd(p,200,330,create_exist_textbox);
                                    usleep(2000000);
                                    memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                    memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                    memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                    login_fun();   //调用创建账号密码功能实现
                                }
                                //将个人数据存放到文档内
                                sprintf(user_information_buf,"账号：%s,密码：%s",account_buf,password_buf);
                                write(user_information_fd, user_information_buf, strlen(user_information_buf));
                                close(user_information_fd);
                                show_font_to_lcd(p,160,330,create_success_textbox);
                                usleep(2000000);
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                show_bmp_to_lcd("login_2.bmp");    //显示登录初始化界面
                                login_fun();   //调用账号密码输入功能实现
                                printf("账号密码已创建成功！\n");
                                printf("你的账号：%s\n", account_buf);
                                printf("你的密码：%s\n", password_buf);
                                break;
                            }
                        } 
                        else if (input_x >= 81 && input_x <= 363 && input_y >= 200 && input_y <= 249) 
                        {
                            // 点击到输入账号的区域
                            out_put_state = 0;
                            touch_state = 1;
                        } 
                        else if (input_x >= 81 && input_x <= 363 && input_y >= 250 && input_y <= 290) 
                        {
                            // 点击到输入密码的区域
                            out_put_state = 1;
                            touch_state = 1;
                        }

                    }

                    // 如果触摸事件结束（input_buf.value == -1），则重置触摸状态
                    if (input_buf.type != 1 || input_buf.code != 330 || input_buf.value != 1) 
                    {
                        touch_state = 0;  // 触摸结束，允许下一次触摸识别
                        //printf("触摸坐标: (%d, %d)\n", input_x, input_y);     //debug
                    }

                    //刷新文本框，防止文字重叠
                    bitmap *account_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *hide_password_text = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    fontPrint(f,account_textbox,0,7,account_buf,getColor(0,0,0,0),0);
                    fontPrint(f,password_text,0,7,hide_password_buf,getColor(0,0,0,0),0);
                    show_font_to_lcd(p,81,200,account_textbox);
                    show_font_to_lcd(p,81,250,password_text);
                }
                //debug注册账号密码输入功能测试
                //printf("注册的账号是: %s\n", account_buf);
                //printf("注册的密码是: %s\n", password_buf);
                //printf("隐藏密码: %s\n", hide_password_buf);

                //关闭文件
                close(lcd_fd);
                close(input_fd);
                fontUnload(f);
                destroyBitmap(account_textbox);
                destroyBitmap(password_text);
                destroyBitmap(create_len_over_textbox);
                destroyBitmap(create_exist_textbox);
                munmap(p, 800 * 480 * 4);               
                }
                create_user_fun();   //调用创建账号密码功能实现
                break;
        }
        if (input_x > 288 && input_x < 353 && input_y > 306 && input_y < 319)    //找回密码
        {
            int find_password_fun()  //找回密码功能实现
            {
                show_bmp_to_lcd("find_password.bmp");    //显示找回密码界面
                int lcd_fd = open("/dev/fb0", O_RDWR);
                if (lcd_fd == -1) 
                {
                    printf("打开液晶屏失败.\n");
                    return -1;
                }
                input_fd = open("/dev/input/event0", O_RDWR);
                if (input_fd == -1) {
                    printf("打开触摸屏失败.\n");
                    return -2;
                }
                
                int captcha_get_fun()
                {   

                    srand(time(NULL));
                    captcha_value = rand() % 9000 + 1000;   //生成4位随机验证码
                    sprintf(captcha_buf, "%d", captcha_value);
                    printf("验证码是: %s\n", captcha_buf);
                    captcha_get_state = 1;
                }

                int *p = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);     //内存映射
                font *f = fontLoad("simkai.ttf");    //初始化字库 f:字库句柄 "simkai.ttf":字库名字
                fontSetSize(f,30);  //设置字体大小 30:字体大小

                //设置文本框大小 80*50:表示文本框大小 4：表示色素     A  R  G  B  （0123456789，+，-，确认，删除，清零）
                bitmap *number_textbox_1 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字1的文本框
                bitmap *number_textbox_2 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字2的文本框
                bitmap *number_textbox_3 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字3的文本框
                bitmap *number_textbox_4 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字4的文本框
                bitmap *number_textbox_5 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字5的文本框
                bitmap *number_textbox_6 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字6的文本框
                bitmap *number_textbox_7 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字7的文本框
                bitmap *number_textbox_8 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字8的文本框
                bitmap *number_textbox_9 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //数字9的文本框
                bitmap *number_textbox_0 = createBitmapWithInit(80,80,4,getColor(0,255,255,255));  //数字0的文本框
                bitmap *delete_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));   //删除的文本框
                bitmap *clean_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));     //清空的文本框
                bitmap *sum_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));      //加号的文本框
                bitmap *sub_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));      //减号的文本框
                bitmap *O_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));         //"O"的文本框
                bitmap *K_textbox = createBitmapWithInit(80,80,4,getColor(0,255,255,255));         //"K"的文本框
                bitmap *account_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));   //账号的文本框
                bitmap *password_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));     //密码的文本框
                bitmap *hide_password_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255)); //隐藏密码的文本框
                bitmap *modify_len_over_textbox = createBitmapWithInit(550, 40, 4,getColor(0,128,128,128)); //超过长度限制提示文本框 灰色
                bitmap *modify_success_textbox = createBitmapWithInit(500, 40, 4,getColor(0,128,128,128)); //注册成功提示文本框 灰色
                bitmap *modify_fail_textbox = createBitmapWithInit(500, 40, 4,getColor(0,128,128,128)); //找回失败提示文本框 灰色
                bitmap *input_captcha_textbox = createBitmapWithInit(280, 40, 4,getColor(0,255,255,255)); //验证码的文本框
                bitmap *input_captcha_right_textbox = createBitmapWithInit(500, 40, 4,getColor(0,128,128,128)); //验证码正确的文本框
                bitmap *input_captcha_error_textbox = createBitmapWithInit(500, 40, 4,getColor(0,128,128,128)); //验证码错误的文本框
                bitmap *input_captcha_get_textbox = createBitmapWithInit(500, 40, 4,getColor(0,128,128,128)); //提示获取验证码的文本框

                //字符数组
                char number_buf_1[21]="1";
                char number_buf_2[21]="2";
                char number_buf_3[21]="3";
                char number_buf_4[21]="4";
                char number_buf_5[21]="5";
                char number_buf_6[21]="6";
                char number_buf_7[21]="7";
                char number_buf_8[21]="8";
                char number_buf_9[21]="9";
                char number_buf_0[21]="0";
                char delete_buf[21]="删除";
                char clean_buf[21]="全清";
                char sum_buf[21]="爱";
                char sub_buf[21]="你";
                char O_buf[21]="确";
                char K_buf[21]="认";
                char modify_len_over_buf[128]="账号或密码长度超过限制，请重新输入！";
                char modify_success_buf[128]="找回成功，请牢记您的账号和密码！";
                char modify_fail_buf[128]="未找到用户，请检查账号是否有误！";
                char input_captcha_buf[10]="";
                char input_captcha_right_buf[128]="验证码正确！";
                char input_captcha_error_buf[128]="验证码错误！";
                char input_captcha_get_buf[128]="请获取验证码！";

                //将字体输入到文本框内 number_textbox:表示数字的文本框 0,0：表示字体显示的起始位置
                fontPrint(f,number_textbox_1,30,25,number_buf_1,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_2,30,25,number_buf_2,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_3,30,25,number_buf_3,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_4,30,25,number_buf_4,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_5,30,25,number_buf_5,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_6,30,25,number_buf_6,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_7,30,25,number_buf_7,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_8,30,25,number_buf_8,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_9,30,25,number_buf_9,getColor(0,0,0,0),0);
                fontPrint(f,number_textbox_0,33,25,number_buf_0,getColor(0,0,0,0),0);
                fontPrint(f,delete_textbox,0,25,delete_buf,getColor(0,0,0,0),0);
                fontPrint(f,clean_textbox,0,25,clean_buf,getColor(0,0,0,0),0);
                fontPrint(f,sum_textbox,25,25,sum_buf,getColor(0,0,0,0),0);
                fontPrint(f,sub_textbox,22,25,sub_buf,getColor(0,0,0,0),0);
                fontPrint(f,O_textbox,15,25,O_buf,getColor(0,0,0,0),0);
                fontPrint(f,K_textbox,20,25,K_buf,getColor(0,0,0,0),0);
                fontPrint(f,account_textbox,0,7,account_buf,getColor(0,0,0,0),0);
                fontPrint(f,password_textbox,0,7,hide_password_buf,getColor(0,0,0,0),0);
                fontPrint(f,modify_len_over_textbox,5,4,modify_len_over_buf,getColor(0,255,255,255),0);
                fontPrint(f,modify_success_textbox,5,4,modify_success_buf,getColor(0,255,255,255),0);
                fontPrint(f,modify_fail_textbox,5,4,modify_fail_buf,getColor(0,255,255,255),0);
                fontPrint(f,input_captcha_textbox,0,4,input_captcha_buf,getColor(0,0,0,0),0);
                fontPrint(f,input_captcha_right_textbox,5,4,input_captcha_right_buf,getColor(0,255,255,255),0);
                fontPrint(f,input_captcha_error_textbox,5,4,input_captcha_error_buf,getColor(0,255,255,255),0);
                fontPrint(f,input_captcha_get_textbox,5,4,input_captcha_get_buf,getColor(0,255,255,255),0);


                //将文本框显示到LCD上 400,150：文本框在LCD上显示的起始位置
                //第一行(1,2,3,删除)
                show_font_to_lcd(p,400,150,number_textbox_1);
                show_font_to_lcd(p,480,150,number_textbox_2);
                show_font_to_lcd(p,560,150,number_textbox_3);
                show_font_to_lcd(p,640,150,delete_textbox);
                //第二行(30,25,6,全清)
                show_font_to_lcd(p,400,230,number_textbox_4);
                show_font_to_lcd(p,480,230,number_textbox_5);
                show_font_to_lcd(p,560,230,number_textbox_6);
                show_font_to_lcd(p,640,230,clean_textbox);
                //第三行(7,8,9,确)
                show_font_to_lcd(p,400,310,number_textbox_7);
                show_font_to_lcd(p,480,310,number_textbox_8);
                show_font_to_lcd(p,560,310,number_textbox_9);
                show_font_to_lcd(p,640,310,O_textbox);
                //第四行(加,0,减,认)
                show_font_to_lcd(p,400,390,sum_textbox);
                show_font_to_lcd(p,480,390,number_textbox_0);
                show_font_to_lcd(p,560,390,sub_textbox);
                show_font_to_lcd(p,640,390,K_textbox);
                //账号密码的文本框显示
                show_font_to_lcd(p,81,200,account_textbox);
                show_font_to_lcd(p,81,250,password_textbox);

                show_font_to_lcd(p,81,400,input_captcha_textbox);

                int out_put_state = 0; // 0: 输入账号, 1: 输入密码, 2: 输入验证码
                int touch_state = 0; // 触摸状态 0: 未触摸, 1: 已触摸

                while(2)
                {
                    input_event(&input_x, &input_y);   // 只有在触摸按键时才进行处理
                    if (input_buf.type == 1 && input_buf.code == 330 && input_buf.value == 1)   // input_buf.value == 1 表示触摸按下
                    { 
                        // 键盘触摸判断区域
                        if (input_x >= 506 && input_x <= 558 && input_y >= 409 && input_y <= 469)    //数字0触摸区域
                        {
                            if (out_put_state == 0)     // 输入账号
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {  
                                    strcat(account_buf, "0");   //账号输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "0");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else    // 输入密码
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "0");   //密码输入0 (拼接之前的数据)
                                    strcat(hide_password_buf, "*");    //隐藏密码输入* (拼接之前的数据)
                                    touch_state = 1;
                                }
                            }
                        }
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0)    // 输入账号
                            {
                                if (touch_state == 0) 
                                {  
                                    strcat(account_buf, "1");   //账号输入1(拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "1");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "1");   //密码输入1 (拼接之前的数据)
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "2");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "2");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "2");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "3");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "3");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "3");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >= 162 && input_y <= 215) 
                        {
                            if (out_put_state == 0 && strlen(account_buf) > 0) 
                            {
                                account_buf[strlen(account_buf) - 1] = '\0';  // 删除账号的最后一个字符
                                touch_state = 1;
                            } 
                            else if (out_put_state == 1 && strlen(password_buf) > 0) 
                            {
                                password_buf[strlen(password_buf) - 1] = '\0';  // 删除密码的最后一个字符
                                hide_password_buf[strlen(hide_password_buf) - 1] = '\0';     // 删除隐藏密码的最后一个字符
                                touch_state = 1;
                            }
                            else if (out_put_state == 2 && strlen(input_captcha_buf) > 0) 
                            {
                                input_captcha_buf[strlen(input_captcha_buf) - 1] = '\0';  // 删除验证码的最后一个字符
                                touch_state = 1;
                            }
                        } 
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 252 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "4");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "4");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "4");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "5");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "5");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "5");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "6");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "6");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "6");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >= 255 && input_y <= 293) 
                        {
                            if (out_put_state == 0) 
                            {
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号
                                touch_state = 1;
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    memset(input_captcha_buf, 0, sizeof(input_captcha_buf));  // 清空验证码输入框
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码
                                touch_state = 1;
                            }
                        } 
                        else if (input_x >= 411 && input_x <= 472 && input_y >= 315 && input_y <= 371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "7");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "7");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "7");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 505 && input_x <= 547 && input_y >=315 && input_y <=371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "8");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "8");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "8");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 580 && input_x <= 631 && input_y >=315 && input_y <=371) 
                        {
                            if (out_put_state == 0) 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(account_buf, "9");
                                    touch_state = 1;
                                }
                            } 
                            if (out_put_state == 2)     // 输入验证码
                            {
                                if (touch_state == 0)   //确保只处理一次触摸
                                {
                                    strcat(input_captcha_buf, "9");   //验证码输入0 (拼接之前的数据)
                                    touch_state = 1;  // 更新触摸状态
                                }
                            }
                            else 
                            {
                                if (touch_state == 0) 
                                {
                                    strcat(password_buf, "9");
                                    strcat(hide_password_buf, "*");
                                    touch_state = 1;
                                }
                            }
                        } 
                        else if (input_x >= 653 && input_x <= 715 && input_y >=315 && input_y <=475)    // 确认键
                        {
                            while(3)    //预先输入完账号和密码后才开始判定，以免同步buf是意外结束
                            {
                                if (out_put_state == 0) 
                                {
                                    if (touch_state == 0) 
                                    {
                                        //strcat(account_buf, "0");     //确认键设定为无效
                                        touch_state = 1;
                                        break;    
                                    }
                                }
                                if (out_put_state == 2)     // 输入验证码
                                {
                                    if (touch_state == 0)   //确保只处理一次触摸
                                    {
                                        //strcat(input_captcha_buf, "0");   //验证码输入0 (拼接之前的数据)
                                        touch_state = 1;  // 更新触摸状态
                                        break;
                                    }
                                }
                                else 
                                {
                                    if (touch_state == 0) 
                                    {
                                        //strcat(password_buf, "0");     //确认键设定为无效
                                        touch_state = 1;
                                        break;    
                                    }
                                }                    
                            }
                            //用于判断账号或密码是否超过限制，若超过，则账号和密码不记录，若未超过，则创建账号和密码
                            //检测是否超过账号和密码长度限制
                            int account_len = strlen(account_buf);
                            int password_len = strlen(password_buf);
                            int hide_password_len = strlen(hide_password_buf);
                            if (account_len > 12 || password_len > 12 || hide_password_len > 12 ) 
                            {
                                show_font_to_lcd(p,130,330,modify_len_over_textbox);
                                usleep(2000000);
                                show_bmp_to_lcd("register.bmp");    //显示创建账号和密码初始界面
                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                memset(input_captcha_buf, 0, sizeof(input_captcha_buf));  // 清空验证码输入框
                                login_fun();   //   调用创建账号密码功能实现
                            }
                            else    //存放账号数据
                            {   
                                if(captcha_get_state == 1)    //点击获取验证码
                                {   
                                    if(strcmp(captcha_buf, input_captcha_buf) == 0)    //验证码输入正确
                                    {   
                                        //不要随意使用memset，会导致即使验证码输入正确也会提示错误！
                                        if(account_buf == "" || password_buf == "")    //账号或密码为空
                                        {
                                            printf("账号或密码不能为空！\n");
                                        }
                                        else
                                        {   
                                            sprintf(user_information_temp_buf3, "%s.txt", account_buf);
                                            int user_information_fd = open(user_information_temp_buf3, O_RDWR);
                                            if (user_information_fd == -1)
                                            {
                                                show_font_to_lcd(p,160,330,modify_fail_textbox);
                                                usleep(2000000);
                                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                                find_password_fun();   //调用找回账号功能实现
                                            }
                                            else
                                            {   
                                                char user_file[256];
                                                sprintf(user_file, "%s.txt", account_buf);
                                                if(remove(user_file) == 0)
                                                {
                                                    printf("已找回该用户！\n",user_file);
                                                    remove_state = 1;
                                                }
                                                else
                                                {
                                                    printf("未找到该用户！\n",user_file);
                                                    remove_state = 0;
                                                }           
                                                //存放拼接后的账号 账号.txt
                                                sprintf(user_information_temp_buf, "%s.txt", account_buf);
                                                //创建个人文档
                                                int user_information_fd = open(user_information_temp_buf, O_RDWR | O_CREAT | O_EXCL);
                                                //将个人数据存放到文档内
                                                sprintf(user_information_buf,"账号：%s,密码：%s",account_buf,password_buf);
                                                write(user_information_fd, user_information_buf, strlen(user_information_buf));
                                                close(user_information_fd);
                                                show_font_to_lcd(p,160,330,modify_success_textbox);
                                                show_font_to_lcd(p,160,370,input_captcha_right_textbox);
                                                usleep(2000000);
                                                memset(account_buf, 0, sizeof(account_buf));  // 清空账号输入框
                                                memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                                memset(hide_password_buf, 0, sizeof(hide_password_buf));     // 清空隐藏密码输入框
                                                show_bmp_to_lcd("login_2.bmp");    //显示登录初始化界面
                                                error_count = 0;
                                                login_fun();   //调用账号密码输入功能实现
                                                printf("账号密码已找回成功！\n");
                                                printf("你的账号：%s\n", account_buf);
                                                printf("你的密码：%s\n", password_buf);
                                                break;           
                                            }          
                                        }
                                    }
                                    else    //验证码输入错误
                                    {   
                                        show_font_to_lcd(p,160,350,input_captcha_error_textbox);
                                        usleep(2000000);
                                        printf("验证码输入错误！\n");
                                        memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                        memset(input_captcha_buf, 0, sizeof(input_captcha_buf));  // 清空验证码输入框
                                        captcha_get_state = 0;
                                        find_password_fun();   //调用找回密码功能实现
                                        break;
                                    } 
                                }
                                else        //未点击获取验证码
                                {
                                    show_font_to_lcd(p,160,350,input_captcha_get_textbox);
                                    printf("请点击获取验证码！\n");
                                    usleep(2000000);
                                    memset(password_buf, 0, sizeof(password_buf));  // 清空密码输入框
                                    memset(input_captcha_buf, 0, sizeof(input_captcha_buf));  // 清空验证码输入框
                                    captcha_get_state = 0;
                                    find_password_fun();   //调用找回密码功能实现
                                    break;
                                }
                                break;
                            }
                         }
                        else if (input_x >= 81 && input_x <= 363 && input_y >= 200 && input_y <= 249) 
                        {
                            // 点击到输入账号的区域
                            out_put_state = 0;
                            touch_state = 1;
                        } 
                        else if (input_x >= 81 && input_x <= 363 && input_y >= 250 && input_y <= 290) 
                        {
                            // 点击到输入密码的区域
                            out_put_state = 1;
                            touch_state = 1;                  
                        }
                        else if (input_x >= 81 && input_x <= 363 && input_y >= 400 && input_y <= 480) 
                        {
                            // 点击到输入验证码的区域
                            out_put_state = 2;
                            touch_state = 1;
                        }
                        else if(input_x >= 192 && input_x <= 291 && input_y >= 320 && input_y <= 371)
                        {
                            // 点击获取验证码
                            captcha_get_fun();
                            //printf("验证码: %s\n", captcha_buf);
                            touch_state = 1;
                        }
                    }
                    // 如果触摸事件结束（input_buf.value == -1），则重置触摸状态
                    if (input_buf.type != 1 || input_buf.code != 330 || input_buf.value != 1) 
                    {
                        touch_state = 0;  // 触摸结束，允许下一次触摸识别
                        //printf("触摸坐标: (%d, %d)\n", input_x, input_y);     //debug
                    }

                    //刷新文本框，防止文字重叠
                    bitmap *account_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *password_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *hide_password_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    bitmap *input_captcha_textbox = createBitmapWithInit(280,40,4,getColor(0,255,255,255));
                    fontPrint(f,account_textbox,0,7,account_buf,getColor(0,0,0,0),0);
                    fontPrint(f,password_textbox,0,7,hide_password_buf,getColor(0,0,0,0),0);
                    fontPrint(f,input_captcha_textbox,0,7,input_captcha_buf,getColor(0,0,0,0),0);
                    show_font_to_lcd(p,81,200,account_textbox);
                    show_font_to_lcd(p,81,250,password_textbox);
                    show_font_to_lcd(p,81,400,input_captcha_textbox);
                }
                //debug注册账号密码输入功能测试
                //printf("注册的账号是: %s\n", account_buf);
                //printf("注册的密码是: %s\n", password_buf);
                //printf("隐藏密码: %s\n", hide_password_buf);

                //关闭文件
                close(lcd_fd);
                close(input_fd);
                fontUnload(f);
                destroyBitmap(account_textbox);
                destroyBitmap(password_textbox);
                destroyBitmap(modify_len_over_textbox);
                destroyBitmap(input_captcha_textbox);
                munmap(p, 800 * 480 * 4);                 
            }
            find_password_fun();
            break;
        }
        else
        {    
            show_bmp_to_lcd("login_initialization.bmp");    //显示登陆账号初始界面
            input_fun();   //获取触摸屏坐标
            login_fun();   //调用登录、注册功能实现
            break;
        }
    }
    return 0;
}

//桌面-游戏功能实现
int home_fun()  //主界面功能实现
{
    show_bmp_to_lcd("home.bmp");    //显示主界面
    while(1)    //是否点击游戏
    {
        input_fun();   //调用触摸屏功能实现，获取坐标数据
        if (input_x >= 333 && input_x <= 390 && input_y >= 210 && input_y <= 275)  //正确触碰到游戏打开范围
        {
            int game_fun()  //游戏功能实现
            {   
                while(2)
                {
                    int game_initialization()    //游戏初始化功能实现
                    {
                        show_bmp_to_lcd("logo.bmp");   //显示游戏logo界面
                        usleep(1000000);    //延时3秒
                        show_bmp_to_lcd("anti_addiction.bmp");    //显示防沉迷提示界面
                        usleep(1000000);    //延时3秒

                        int input_account_number_buf_fun()  //账号密码输入功能实现
                        {
                            show_bmp_to_lcd("login_1.bmp");    //显示开始登陆界面
                            input_fun();   //调用触摸屏功能实现，获取坐标数据
                            while(3)    //是否点击账号密码输入界面，是则输入账号和密码；若点击退出按钮，则返回主界面，若点击除上述外的区域，保持登陆界面不变
                            {
                                if (input_x >= 318 && input_x <= 496 && input_y >= 217 && input_y <= 392)    //点击登录按钮                        
                                    {
                                        show_bmp_to_lcd("login_initialization.bmp");    //显示登录初始化界面
                                        //account_number_buf_password_number_buf_textbox_fun();   //调用使用外设填入账号密码文本框功能实现(已弃用，可自主开启)
                                        login_fun();   //调用登录、注册功能实现
                                    }                                
                                if (input_x >= 729 && input_x <= 798 && input_y >= 409 && input_y <= 476)    //点击游戏退出按钮
                                    {   
                                        show_bmp_to_lcd("login_exit.bmp");    //显示游戏退出确认界面
                                        while(3)
                                        {
                                            input_fun();   //调用触摸屏功能实现，获取坐标数据
                                            if(input_x > 243 && input_x < 390 && input_y > 284 && input_y < 360)    //确定退出
                                                {
                                                    home_fun();   //返回主界面
                                                }
                                            if(input_x > 400 && input_x < 547 && input_y > 284 && input_y < 360)    //取消退出
                                                {
                                                    input_account_number_buf_fun();    //返回登录界面
                                                }
                                            else    
                                                {
                                                    break;    //重新回到游戏登录界面
                                                }
                                        }
                                    }   
                                else    //点击其他区域，保持登陆界面不变
                                {  
                                    show_bmp_to_lcd("login_1.bmp");    //显示开始登陆界面
                                    input_fun();   //重新监听，直到点击登录按钮或退出按钮
                                }  
                            } 
                        } 
                        input_account_number_buf_fun(); 
                    }
                    game_initialization();   //调用游戏初始化功能实现
                    break;                   
                }
            }
            game_fun();   //调用游戏功能实现
            break;   //跳出主界面循环
        }
        else    //未点击游戏打开范围
        {
            show_bmp_to_lcd("home.bmp");    //显示主界面
            no_game_textbox_fun();   //调用未点开游戏文本框提示功能实现
            usleep(2000000);    //延时2秒,缓冲
            show_bmp_to_lcd("home.bmp");    //显示主界面
            input_fun();   //重新监听，直至点击游戏打开范围
        }
    }
    return 0;
}

//主函数
int main(int argc, char const *argv[])
{
    home_fun();   //主界面功能实现
    return 0;
}


