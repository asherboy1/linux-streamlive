/**
 * 这部分主要实现接收线程
 * 使用UDP网络
 * 
 * 
 */

#include"./include.h"


extern struct Env env;

int recv_count = 0;


//需要测试UDP网络接收情况，将下面的（//）删除，会将接收的数据保存在（./video/recv.h264）
//#define UDP_RECV_TEST 

void * recv_thread(void *arg){
    struct status_flag *flag = &(env.flag);
    printf("recv_thread start\n");
    
    int client_id,ret;
    struct sockaddr_in serveraddr;

    socklen_t len;

#ifdef UDP_RECV_TEST

    FILE *fp = fopen("./video/recv.h264","wb+");
    if(fp == NULL){
        printf("create file error\n");
        flag->recv_init_ok = 1;
        return ;
    }
#endif
    client_id = socket(AF_INET,SOCK_DGRAM,0); //获得套接字
    if(client_id < 0){
        printf("socket error/n");
        flag->recv_init_ok = 1;
        return ;
    }

    //下面两步是设置client_id不阻塞,防止其等待数据
    int fcntl_flag = fcntl(client_id,F_GETFL,0);
    if(fcntl_flag < 0){
        printf("fcntl first error\n");
    }
    if(fcntl(client_id,F_SETFL,fcntl_flag | O_NONBLOCK)){
        printf("fcntl sencond error\n");
    }


    memset(&serveraddr,0,sizeof(struct sockaddr_in));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serveraddr.sin_port = htons(SERVER_PORT);
    
    struct sockaddr_in client;
    int count = 0;
    char buf[RECV_BUFFER_SIZE] = "ok";
    len = sizeof(serveraddr);
    printf("client:%s\n",buf);  
    count = sendto(client_id, buf, RECV_BUFFER_SIZE, 0,(struct sockaddr *)&serveraddr, len); //发送OK给服务端，请求数据发送
    if(count < 0){
        printf("sendto error\n");
        flag->recv_init_ok = 1;
    }
    while(1){
        if(env.flag.if_stop == 1){
            break;
        }
        memset(buf, 0, RECV_BUFFER_SIZE);
        count = recvfrom(client_id, buf, RECV_BUFFER_SIZE, 0, (struct sockaddr*)&client, &len); //接收数据函数
        if(count > 0){
            //recv_count ++;
            //printf("recv  = %d\n",recv_count); 
            if(inc_buffer(&env,buf,2,count) < 0){   //将接收的数据添加到接收链表
                printf("recv to list error\n");
                break;
            }
        }

#ifdef UDP_RECV_TEST
        fwrite(buf,count,1,fp);
#endif

    }
    close(client_id);

#ifdef UDP_RECV_TEST
        fclose(fp);
#endif

    printf("UDP recv exit\n");
    return;
}

