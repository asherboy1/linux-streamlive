#include"./include.h"

/**
* 向链表中添加数据
* 其中的type参数是用来区分向哪个链表中加
* 1：表示解码后链表；2：接收链表
*/
int inc_buffer(struct Env *penv,uint8_t * buffer,int type,int size){ 

    if(type == 1){
        struct deco_buf * head = &(penv->deco_buffer);
        unsigned int size_v = penv->video_w*penv->video_h;
        while(head->next != NULL){
            head = head->next;
        }
        struct deco_buf *p;
        p = (struct deco_buf *)malloc(sizeof(struct deco_buf));
        if(p == NULL){
            printf("malloc error\n");
            return -1;
        }
        memcpy(p->buff,buffer,size_v*3/2);
        p->next = NULL;
        head->next = p;
        head  = p;
        p = NULL;
    }
    if(type == 2){
        struct recv_buf * head = &(penv->recv_buffer);
        while(head->next != NULL){
            head = head->next;
        }
        struct recv_buf *p;
        p = (struct recv_buf *)malloc(sizeof(struct recv_buf));
        if(p == NULL){
            printf("malloc error\n");
            return -1;
        }
        memcpy(p->buff,buffer,size);
        p->size = size;
        p->next = NULL;
        head->next = p;
        head  = p;
        p = NULL;
    }
    return 0;
}
/**
* 从链表读数据
* 其中的type参数是用来区分从哪个链表中读
* 1：表示解码后链表；2：接收链表
*/
int dec_buffer(struct Env *penv,uint8_t * buffer,int type,int *size){
    if(type == 1){
        struct deco_buf * head = &(penv->deco_buffer);
        if(head->next == NULL)
        {
            return 2; //从解码后链表读取去显示时，因为显示过程不能阻塞，所以通过返回值完成
        }
        unsigned int size = penv->video_w*penv->video_h;
        struct deco_buf *p;
        p = head->next;
        memset(buffer, 0, size*12/8);
        memcpy(buffer,p->buff,size*3/2);
        
        head->next = p->next;
        free(p);
    }
    if(type == 2){
        struct recv_buf * head = &(penv->recv_buffer);
        while(head->next == NULL);  //等待数据
        struct recv_buf *p;
        p = head->next;
        while(p->next == NULL);  //保证链表数据量充足，防止同时操作一个空间
        memset(buffer, 0, p->size);
        memcpy(buffer,p->buff,p->size);
        *size = p->size;
        head->next = p->next;
        free(p);
    }
    
    return 0;
}

//初始化状态标识
void env_init(struct Env *penv){
    penv->video_w = WIDTH_SIZE;
    penv->video_h = HIGH_SIZE;

    penv->flag.recv_init_ok = 0;
    penv->flag.deco_init_ok = 0;
    penv->flag.contr_init_ok = 0;
    penv->flag.sdl_init_ok = 0;
    
    penv->flag.if_stop = 0;
    
}