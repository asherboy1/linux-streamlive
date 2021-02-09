/**
 * 这部分主要功能是解码和显示。
 * 其中解码使用的FFmpeg,显示使用的SDL库
 *
 * 解码过程可参考FFmpeg中实例，其中增加了从内存读取数据功能。
 * 可参考源码中的（avio_reading.c）
 * 
 * SDL显示时使用了SDL自带的多线程服务，
 * 增加了事件监控功能，可以监控各种事件。需要自己添加
 * 本程序只增加了关闭，放大，空格键暂停功能。
 * 
 * 想要理解解码显示一般过程
 * 可以参考 https://blog.csdn.net/leixiaohua1020/article/details/8652605
 * 感谢雷霄骅
 */

#include"./include.h"

extern struct Env env;

FILE *ff_read_file_fp;

//下面两个用于解码和播放功能测试
//#define SDL_DISPLAY_TEST 
//#define DECO_WRITE_TEST 


//定义的刷新事件
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//定义的结束事件
#define BREAK_EVENT  (SDL_USEREVENT + 2)

//SDL控制参数
int sdl_thread_exit = 0;
int sdl_if_stop = 0;

//SDL子线程，负责刷新屏幕控制
int refresh_video(void *opaque){
    while(sdl_thread_exit == 0){
        SDL_Event event;
        event.type =REFRESH_EVENT;
        while(sdl_if_stop);
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }
    sdl_thread_exit = 0;
    SDL_Event event;
    event.type = BREAK_EVENT;
    SDL_PushEvent(&event);
    return 0;
}
//显示线程
void * sdl_thread(void *arg){   
    const int bpp = 12;
    int screen_w = env.video_w;
    int screen_h = env.video_h;
    
    const int pixel_w = env.video_w;
    const int pixel_h = env.video_h;

    struct status_flag *flag = &(env.flag);    

    printf("SDL_thread start\n");

#ifdef SDL_DISPLAY_TEST    
    FILE *fp = fopen("./video/mtest.yuv","rb");
    if(fp == NULL){
        printf("open error\n");
        return ;
    }
#endif

    while(env.deco_buffer.next == NULL);
    if(SDL_Init(SDL_INIT_VIDEO)){
        printf("can not init SDL\n");
        flag->sdl_init_ok = 1;
        return ;
    }
    unsigned char buffer[pixel_w*pixel_h*bpp/8];
    SDL_Window *screen;

    screen = SDL_CreateWindow("专用网络播放器",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,screen_w,screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!screen){
        printf("SDL:can not create window \n");
        flag->sdl_init_ok = 1;
        return ;
    }
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen,-1,0);

    Uint32 pixformat=0;
    pixformat = SDL_PIXELFORMAT_IYUV;

    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer,pixformat,SDL_TEXTUREACCESS_STREAMING,pixel_w,pixel_h);

    SDL_Rect sdlRect;
    SDL_Thread *refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
    SDL_Event event;

    while(1){
SDL_LOOP:
        SDL_WaitEvent(&event);
        if(event.type == REFRESH_EVENT){

#ifdef SDL_DISPLAY_TEST
            if(fread(buffer, 1, pixel_w*pixel_h*bpp/8, fp) != pixel_w*pixel_h*bpp/8){
                fseek(fp, 0, SEEK_SET);
                fread(buffer, 1, pixel_w*pixel_h*bpp/8, fp);
            }
#endif
#ifndef SDL_DISPLAY_TEST

            //读取失败就返回，重新等待事件。这部分不能写成循环，可能会导致程序无法关闭 
            if(dec_buffer(&env,buffer,1,NULL) != 0){ //从解码链表中读取解码后的数据
                goto SDL_LOOP;    
            }
#endif

            SDL_UpdateTexture(sdlTexture,NULL,buffer,pixel_w);

            //设置在窗口中显示的位置
            sdlRect.x = 0;
            sdlRect.y = 0;
            sdlRect.w = screen_w;
            sdlRect.h = screen_h;                           
            SDL_RenderClear(sdlRenderer);
            SDL_RenderCopy(sdlRenderer,sdlTexture,NULL,&sdlRect);
            SDL_RenderPresent(sdlRenderer);
        }else if(event.type == SDL_WINDOWEVENT){  //监控改变窗口大小的事件
            SDL_GetWindowSize(screen,&screen_w,&screen_h); //获取改变后的大小
        }else if(event.type == SDL_QUIT){  //监控退出事件
            sdl_thread_exit = 1;
            env.flag.if_stop = 1;
        }else if(event.type == BREAK_EVENT){
            break;
        }else if(event.type == SDL_KEYDOWN){ //监控键盘按下事件
            switch(event.key.keysym.sym){
                //32是SDL定义的空格键，本人没有找到它的名字
                //因为是枚举类型，所以通过将其打印出来获取的。
                //想获得其他按键也可以通过这种办法
                case 32:sdl_if_stop = ~sdl_if_stop;break;  
                case SDLK_DOWN: ;break;
            }
        }
    }
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(screen);
    SDL_Quit();
    printf("SDL exit\n");
    return ;
}

//FFmpeg从内存读的回调函数。
int read_packet(void * opaque,uint8_t *buf, int buf_size){
#ifdef  DECO_WRITE_TEST	
    if(!feof(ff_read_file_fp)){
		int true_size=fread(buf,1,buf_size,ff_read_file_fp);
		return true_size;
	}else{
		return -1;
	}
#endif
#ifndef DECO_WRITE_TEST
    int true_size = 0; 
    int ret = 0;
    ret = dec_buffer(&env,buf,2,&true_size);
    if(ret == 0){
        
        return true_size;
    }
    printf("read from recv buffer error\n");
#endif
}
void * deco_thread(void *arg){
    AVCodec *codec = NULL;
    AVCodecContext *cctx= NULL;
    AVCodecParameters *origin_par = NULL;
    AVFrame *frame = NULL;
    uint8_t *byte_buffer = NULL,*avio_ctx_buffer = NULL;
    AVPacket *pkt;
    AVFormatContext *fctx = NULL;
    int ret = 0;
    int video_stream;
    int byte_buffer_size;
    int number_of_written_bytes;
    struct status_flag *flag = &(env.flag);
    int avio_ctx_buffer_size = 10000;
    AVIOContext *avio_ctx = NULL;

#ifdef  DECO_WRITE_TEST
    char yuv_name[] = "./video/mtest.yuv";
    FILE *fp_yuv = fopen(yuv_name,"wb+");
    if(fp_yuv == NULL){
        printf("create file error\n");
        return;
    }

    char fname[] = "./video/mtest2.h264";
    ff_read_file_fp = fopen(fname,"rb+");
#endif

    printf("deco_thread start\n");

    //以下的三个关键函数是从内存中读需要做的工作
    if (!(fctx = avformat_alloc_context())) {
        av_log(NULL, AV_LOG_ERROR, "Can't create avformat context\n");
        flag->deco_init_ok = 1;
        return;
    }
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't av_malloc avio buffer\n");
        flag->deco_init_ok = 1;
        return;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, NULL, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't avio alloc context\n");
        flag->deco_init_ok = 1;
    }
    fctx->pb = avio_ctx;

    if ((ret = avformat_open_input(&fctx, NULL, NULL, NULL)) != 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        flag->deco_init_ok = 1;
        return;
    }
    ret = avformat_find_stream_info(fctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        flag->deco_init_ok = 1;
        return;
    }
    video_stream = av_find_best_stream(fctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
        flag->deco_init_ok = 1;
        return ;
    }
    origin_par = fctx->streams[video_stream]->codecpar;
    codec = avcodec_find_decoder(origin_par->codec_id);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        flag->deco_init_ok = 1;
        return ;
    }
    cctx = avcodec_alloc_context3(codec);
    if(!cctx){
        av_log(NULL,AV_LOG_ERROR,"can't copy decoder context\n");
        flag->deco_init_ok = 1;
        return ;
    }
    ret = avcodec_parameters_to_context(cctx, origin_par);
    if(ret){
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        flag->deco_init_ok = 1;
        return ;
    }
    ret = avcodec_open2(cctx,codec,NULL);
    if (ret < 0) {
        av_log(cctx, AV_LOG_ERROR, "Can't open decoder\n");
        flag->deco_init_ok = 1;
        return ;
    }
    frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        flag->deco_init_ok = 1;
        return ;
    }
    byte_buffer_size = av_image_get_buffer_size(cctx->pix_fmt, cctx->width, cctx->height, 16);
    byte_buffer = av_malloc(byte_buffer_size);
    if(!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        flag->deco_init_ok = 1;
        return ;
    }
    pkt = av_packet_alloc();
    printf("kuan = %d, gao = %d\n",cctx->width,cctx->height);
    
    while (1) {
FFmpeg_loop:  //这个loop的作用是在丢帧的时候，保证解码器持续的运行。
        while(av_read_frame(fctx, pkt) < 0 && env.flag.if_stop != 1);  //数据读取失败，也要继续读，UDP丢失数据是常态

        if(env.flag.if_stop == 1){
            break;
        }
        if(pkt->stream_index == video_stream){
            if((ret = avcodec_send_packet(cctx,pkt)) != 0){
                av_log(NULL, AV_LOG_ERROR, "Error send packet\n");
                //return;
                goto FFmpeg_loop;
            }
            if ((ret = avcodec_receive_frame(cctx,frame)) != 0){
                av_log(NULL, AV_LOG_ERROR, "Receive video frame failed!\n");
                //return;
                goto FFmpeg_loop;
            }
            if(ret == 0){
                number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,(const uint8_t* const *)frame->data, (const int*) frame->linesize,cctx->pix_fmt,cctx->width,cctx->height, 1);
                if (number_of_written_bytes < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                    //return;
                    goto FFmpeg_loop;
                }

#ifdef  DECO_WRITE_TEST
                // write test 
                fwrite(frame->data[0],1,cctx->width*cctx->height,fp_yuv);
                fwrite(frame->data[1],1,cctx->width*cctx->height/4,fp_yuv);
                fwrite(frame->data[2],1,cctx->width*cctx->height/4,fp_yuv);
#endif
#ifndef DECO_WRITE_TEST
                if(inc_buffer(&env,byte_buffer,1,0) != 0){ //将解码后的数据写到解码链表中
                    printf("inc to buffer error\n");
                }

#endif
            }
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avformat_close_input(&fctx);
    avcodec_free_context(&cctx);
    avformat_free_context(fctx);
    env.flag.if_stop == 1;
    printf("deco exit\n");
#ifdef  DECO_WRITE_TEST  
    fclose(ff_read_file_fp);
    fclose(fp_yuv);

#endif  
    return;
}
