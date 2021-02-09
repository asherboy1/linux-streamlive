/**
 * 这一个简单的专用网络实时播放器
 * 作者：王新武
 * 完成时间：2019.5.20
 * 
 * 使用本程序需要安装FFmpeg库和SDL2库
 * 本程序拥有三个线程，分别负责接收，解码，播放。
 * 这三个方面的知识会在其功能实现文件中说明.
 *
 * 本程序将接收的数据放在接收链表中，解码后的数据放在解码链表中
 * 解码时从接收链表中取数据，显示从解码链表中取数据
 *
 */ 


#include"./include.h"

struct Env env; //包含本程序所有需用的数据

int main()
{
    env_init(&env);

    pthread_t recv_thread1,deco_thread1,sdl_thread1;

    
    //接收线程启动
    if(pthread_create(&recv_thread1,NULL,recv_thread,NULL) < 0){
        printf("recv_thread error\n");
        return -1;
    }
    usleep(500);
    //显示线程启动
    if(pthread_create(&sdl_thread1,NULL,sdl_thread,NULL) < 0){
        printf("sdl_thread error\n");
        return -1;
    }

    //解码线程启动
    if(pthread_create(&deco_thread1,NULL,deco_thread,NULL) < 0){
        printf("deco_thread error\n");
        return -1;
    }
    
    
    while(env.flag.if_stop == 0);
    sleep(5);
    return 0;
}
