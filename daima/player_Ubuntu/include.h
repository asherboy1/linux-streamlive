/**
 *这是软件的头文件 
 *定义了本程序使用的所有结构体和函数 
 * 
 * 
 */

#include"stdio.h"
#include"pthread.h"
#include"string.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


//FFmpeg 库
#include "libavutil/adler32.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"

//SDL 库
#include<SDL2/SDL.h>

#include "fcntl.h"
#include "unistd.h"

//根据获取视频大小设置，我获取的大小是1920*1080，

#define DECODE_BUFFER_SIZE  1920*1080

//根据服务器一次发送的数据大小设置，可以在流转发服务器中看到
#define RECV_BUFFER_SIZE  60000

//设置客户端连接服务器的IP和端口号
#define SERVER_IP "192.168.126.129"
#define SERVER_PORT  8888

//设置视频大小（宽高）
#define WIDTH_SIZE  1920; //宽
#define HIGH_SIZE   1080; //高

//监控程序运行状态
struct status_flag {
    //初始化各个初始化情况标识
    int recv_init_ok;
    int deco_init_ok;
    int contr_init_ok;
    int sdl_init_ok;

    int if_stop;  //控制程序是否结束
    
};

//存放接收数据的结构体
struct recv_buf{
    int size;
    unsigned char buff[RECV_BUFFER_SIZE];
    struct recv_buf *next;
};

//存放解码数据的结构体
struct deco_buf{
    unsigned char buff[DECODE_BUFFER_SIZE*3/2 + 10];
    struct deco_buf *next;
};

//将所有数据放在一个结构体，方便传递
struct Env{
    struct status_flag flag;
    struct recv_buf recv_buffer;
    struct deco_buf deco_buffer;

    //视频的大小（长，宽）
    int video_w;    //长
    int video_h;    //宽
};

//初始化状态标识
void env_init(struct Env *);

//SDL 显示线程
void * sdl_thread(void *arg);
int refresh_video(void *opaque);

//解码线程
void * deco_thread(void *arg);

//网络接收线程(UDP)
void * recv_thread(void *arg);

//向链表里添加
int inc_buffer(struct Env *penv,uint8_t * buffer,int type,int size);

//从链表里读取
int dec_buffer(struct Env *penv,uint8_t * buffer,int type,int *size);

//解码线程中的读文件回调函数
int read_packet(void * opaque,uint8_t *buf, int buf_size);
