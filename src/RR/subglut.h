#ifndef _SUBGLUT_H
#define _SUBGLUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Mouse buttons. */
#define GLUT_LEFT_BUTTON		0
#define GLUT_MIDDLE_BUTTON		1
#define GLUT_RIGHT_BUTTON		2

/* Mouse button  state. */
#define GLUT_DOWN			0
#define GLUT_UP				1

#define GLUT_KEY_F1			59
#define GLUT_KEY_F2			60
#define GLUT_KEY_F3			61
#define GLUT_KEY_F4			62
#define GLUT_KEY_F5			63
#define GLUT_KEY_F6			64
#define GLUT_KEY_F7			65
#define GLUT_KEY_F8			66
#define GLUT_KEY_F9			67
#define GLUT_KEY_F10			68
#define GLUT_KEY_F11			133
#define GLUT_KEY_F12			134
#define GLUT_KEY_LEFT			75
#define GLUT_KEY_UP			72
#define GLUT_KEY_RIGHT			77
#define GLUT_KEY_DOWN			80
#define GLUT_KEY_PAGE_UP		73
#define GLUT_KEY_PAGE_DOWN		81
#define GLUT_KEY_HOME			71
#define GLUT_KEY_END			79
#define GLUT_KEY_INSERT			82

void glutInit(int *argcp, char **argv);
void glutMainLoop(void);
void glutDisplayFunc(void (*func)(void));
void glutReshapeFunc(void (*func)(int width, int height));
void glutKeyboardFunc(void (*func)(unsigned char key, int x, int y));
void glutMouseFunc(void (*func)(int button, int state, int x, int y));
void glutMotionFunc(void (*func)(int x, int y));
void glutPassiveMotionFunc(void (*func)(int x, int y));
void glutEntryFunc(void (*func)(int state));
void glutVisibilityFunc(void (*func)(int state));
void glutIdleFunc(void (*func)(void));
void glutSpecialFunc(void (*func)(int key, int x, int y));


#ifdef __cplusplus
}
#endif

#endif
