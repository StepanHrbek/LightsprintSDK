#ifndef _VIDEO_H
#define _VIDEO_H

#define byte unsigned char

extern unsigned video_XRES, video_YRES;
extern float video_XFOV, video_YFOV;
extern bool video_inited;

extern int *video_Init(int xres, int yres);
extern void video_Clear(float rgb[3]);
extern void video_WriteBuf(char *text);
extern void video_WriteScreen(char *text);
extern void video_PutPixel(unsigned x,unsigned y,unsigned color);
extern unsigned video_GetPixel(unsigned x,unsigned y);
extern void video_Blit(void);
extern void video_Grab(const char *name);
extern void video_Done(void);

#endif
