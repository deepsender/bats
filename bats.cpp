//cs371
//
//program: bats.cpp
//author:  Gordon Griesel
//date:    2014
//
//This program was developed from a 371 project idea that didn't get finished.
//It's the opening scene of a batman trailer.
//Started with the rain forets program, and made the raindrops bats.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <X11/Xlib.h>
//#include <X11/Xutil.h>
//#include <GL/gl.h>
//#include <GL/glu.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "log.h"
#include "ppm.h"
#include "fonts.h"

//defined types
typedef float Flt;
typedef Flt Vec[3];
typedef Flt	Matrix[4][4];

//macros
#define rnd() (((Flt)rand())/(Flt)RAND_MAX)
#define random(a) (rand()%a)
#define MakeVector(x, y, z, v) (v)[0]=(x),(v)[1]=(y),(v)[2]=(z)
#define VecZero(a) (a)[0]=(a)[1]=(a)[2]=0.0f
#define VecCopy(a,b) (b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2]
#define VecDot(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VecSub(a,b,c) (c)[0]=(a)[0]-(b)[0]; \
                      (c)[1]=(a)[1]-(b)[1]; \
                      (c)[2]=(a)[2]-(b)[2]
//constants
const float timeslice = 1.0f;
const float gravity = -0.2f;
#define ALPHA 1

//X Windows variables
Display *dpy;
Window win;
GLXContext glc;

//function prototypes
void initXWindows(void);
void init_opengl(void);
void cleanupXWindows(void);
void check_resize(XEvent *e);
void check_mouse(XEvent *e);
void check_keys(XEvent *e);
void init();
void physics(void);
void render(void);

//-----------------------------------------------------------------------------
//Setup timers
const double physicsRate = 1.0 / 30.0;
const double oobillion = 1.0 / 1e9;
struct timespec timeStart, timeCurrent;
struct timespec timePause;
double physicsCountdown=0.0;
double timeSpan=0.0;
unsigned int upause=0;
double timeDiff(struct timespec *start, struct timespec *end) {
	return (double)(end->tv_sec - start->tv_sec ) +
			(double)(end->tv_nsec - start->tv_nsec) * oobillion;
}
void timeCopy(struct timespec *dest, struct timespec *source) {
	memcpy(dest, source, sizeof(struct timespec));
}
//-----------------------------------------------------------------------------


int done=0;
int xres=900, yres=480;

Ppmimage *batbackgroundImage=NULL;
Ppmimage *batsImage=NULL;
Ppmimage *batlogoImage=NULL;

GLuint batbackgroundTexture;
GLuint batsTexture;
//GLuint batlogoTexture; //not used as a texture map
int batman = 0;
struct timespec batAuto;
struct timespec batTime;
//
typedef struct t_bat {
	//these are bats
	int type;
	int linewidth;
	int sound;
	int slow;
	Vec pos;
	Vec lastpos;
	Vec vel;
	Vec savevel;
	Vec maxvel;
	Vec force;
	Vec rotate;
	int image;
	float rot;
	float scale;
	float color[4];
	struct t_bat *prev;
	struct t_bat *next;
} Bat;
Bat *ihead=NULL;
int ndrops=13;
int totbat=0;
int maxbat=0;
void delete_bat(Bat *node);
void cleanup_bats(void);

int main(void)
{
	//logOpen();
	initXWindows();
	init_opengl();
	init();
	clock_gettime(CLOCK_REALTIME, &batAuto);
	clock_gettime(CLOCK_REALTIME, &timePause);
	clock_gettime(CLOCK_REALTIME, &timeStart);
	while(!done) {
		while(XPending(dpy)) {
			XEvent e;
			XNextEvent(dpy, &e);
			check_resize(&e);
			check_mouse(&e);
			check_keys(&e);
		}
		//
		//Below is a process to apply physics at a consistent rate.
		//1. Get the time right now.
		clock_gettime(CLOCK_REALTIME, &timeCurrent);
		//2. How long since we were here last?
		timeSpan = timeDiff(&timeStart, &timeCurrent);
		//3. Save the current time as our new starting time.
		timeCopy(&timeStart, &timeCurrent);
		//4. Add time-span to our countdown amount.
		physicsCountdown += timeSpan;
		//5. Has countdown gone beyond our physics rate? 
		//       if yes,
		//           In a loop...
		//              Apply physics
		//              Reducing countdown by physics-rate.
		//              Break when countdown < physics-rate.
		//       if no,
		//           Apply no physics this frame.
		while(physicsCountdown >= physicsRate) {
			//6. Apply physics
			physics();
			//7. Reduce the countdown by our physics-rate
			physicsCountdown -= physicsRate;
		}
		//Always render every frame.
		render();
		glXSwapBuffers(dpy, win);
	}
	cleanupXWindows();
	cleanup_fonts();
	//logClose();
	return 0;
}

void cleanupXWindows(void)
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

void set_title(void)
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "371 - project demo  Press B for Batman");
}

void setup_screen_res(const int w, const int h)
{
	xres = w;
	yres = h;
}

void initXWindows(void)
{
	Window root;
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	//GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, None };
	XVisualInfo *vi;
	Colormap cmap;
	XSetWindowAttributes swa;

	setup_screen_res(xres, yres);
	dpy = XOpenDisplay(NULL);
	if(dpy == NULL) {
		printf("\n\tcannot connect to X server\n\n");
		exit(EXIT_FAILURE);
	}
	root = DefaultRootWindow(dpy);
	vi = glXChooseVisual(dpy, 0, att);
	if(vi == NULL) {
		printf("\n\tno appropriate visual found\n\n");
		exit(EXIT_FAILURE);
	} 
	//else {
	//	// %p creates hexadecimal output like in glxinfo
	//	printf("\n\tvisual %p selected\n", (void *)vi->visualid);
	//}
	cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	swa.colormap = cmap;
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
						StructureNotifyMask | SubstructureNotifyMask;
	win = XCreateWindow(dpy, root, 0, 0, xres, yres, 0,
							vi->depth, InputOutput, vi->visual,
							CWColormap | CWEventMask, &swa);
	set_title();
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
}

void reshape_window(int width, int height)
{
	//window has been resized.
	setup_screen_res(width, height);
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, xres, 0, yres, -1, 1);
	set_title();
}

unsigned char *buildAlphaData(Ppmimage *img)
{
	//add 4th component to RGB stream...
	int i;
	int a,b,c;
	unsigned char *newdata, *ptr;
	unsigned char *data = (unsigned char *)img->data;
	newdata = (unsigned char *)malloc(img->width * img->height * 4);
	ptr = newdata;
	for (i=0; i<img->width * img->height * 3; i+=3) {
		a = *(data+0);
		b = *(data+1);
		c = *(data+2);
		*(ptr+0) = a;
		*(ptr+1) = b;
		*(ptr+2) = c;
		//get largest color component...
		//code suggested by Chris Smith
		*(ptr+3) = (a|b|c);
		ptr += 4;
		data += 3;
	}
	return newdata;
}

void init_opengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, xres, yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//This sets 2D mode (no perspective)
	glOrtho(0, xres, 0, yres, -1, 1);
	//
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);
	//
	//Clear the screen
	glClearColor(1.0, 1.0, 1.0, 1.0);
	//Do this to allow fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();
	//
	//load the images file into a ppm structure.
	//
	batbackgroundImage = ppm6GetImage("./images/batbackground.ppm");
	batlogoImage       = ppm6GetImage("./images/bat.ppm");
	batsImage          = ppm6GetImage("./images/bats.ppm");
	//
	glGenTextures(1, &batbackgroundTexture);
	glBindTexture(GL_TEXTURE_2D, batbackgroundTexture);
	//smooth the texdture map image
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 3,
							batbackgroundImage->width, batbackgroundImage->height,
							0, GL_RGB, GL_UNSIGNED_BYTE, batbackgroundImage->data);
	//-------------------------------------------------------------------------
	//
	//silhouette: this is similar to a sprite graphic.
	//
	glGenTextures(1, &batsTexture);
	glBindTexture(GL_TEXTURE_2D, batsTexture);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	//must build a new set of data...
	unsigned char *silhouetteData = buildAlphaData(batsImage);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
							batsImage->width, batsImage->height,
							0, GL_RGBA, GL_UNSIGNED_BYTE, silhouetteData);
	free(silhouetteData);
	//-------------------------------------------------------------------------
}

void check_resize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != xres || xce.height != yres) {
		//Window size did change.
		reshape_window(xce.width, xce.height);
	}
}

void init() {
}

void check_mouse(XEvent *e)
{
	//Did the mouse move?
	//Was a mouse button clicked?
	static int savex = 0;
	static int savey = 0;
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button is down
		}
		if (e->xbutton.button==3) {
			//Right button is down
		}
	}
	if (savex != e->xbutton.x || savey != e->xbutton.y) {
		//Mouse moved
		savex = e->xbutton.x;
		savey = e->xbutton.y;
	}
}

void disburseBatman() {
	Bat *node = ihead;
	while(node) {
		if (node->slow) {
			node->vel[0] = node->scale * 4.0f + (node->scale * 1.5f);
			Flt s = rnd() * node->scale * 4.0f;
			node->vel[1] = s - (2.0f);
			node->slow=0;
		}
		node = node->next;
	}
}

void check_keys(XEvent *e)
{
	//keyboard input?
	static int shift=0;
	int key = XLookupKeysym(&e->xkey, 0);
	if (e->type == KeyRelease) {
		if (key == XK_Shift_L || key == XK_Shift_R)
			shift=0;
		return;
	}
	if (e->type == KeyPress) {
		if (key == XK_Shift_L || key == XK_Shift_R) {
			shift=1;
			return;
		}
	} else {
		return;
	}
	if (shift) {}
	switch(key) {
		case XK_b:
			batman ^= 1;
			if (!batman) {
				disburseBatman();
			} else {
				clock_gettime(CLOCK_REALTIME, &batTime);
			}
			break;
		case XK_Escape:
			done=1;
			break;
	}
}

Flt VecNormalize(Vec vec)
{
	Flt len, tlen;
	Flt xlen = vec[0];
	Flt ylen = vec[1];
	Flt zlen = vec[2];
	len = xlen*xlen + ylen*ylen + zlen*zlen;
	if (len == 0.0) {
		MakeVector(0.0,0.0,1.0,vec);
		return 1.0;
	}
	len = sqrt(len);
	tlen = 1.0 / len;
	vec[0] = xlen * tlen;
	vec[1] = ylen * tlen;
	vec[2] = zlen * tlen;
	return(len);
}

void cleanup_bats(void)
{
	Bat *s;
	while(ihead) {
		s = ihead->next;
		free(ihead);
		ihead = s;
	}
	ihead=NULL;
}

void delete_bat(Bat *node)
{
	//remove a node from linked list
	//Log("delete_bat()...\n");
	if (node->prev == NULL) {
		if (node->next == NULL) {
			//Log("only 1 item in list.\n");
			ihead = NULL;
		} else {
			//Log("at beginning of list.\n");
			node->next->prev = NULL;
			ihead = node->next;
		}
	} else {
		if (node->next == NULL) {
			//Log("at end of list.\n");
			node->prev->next = NULL;
		} else {
			//Log("in middle of list.\n");
			node->prev->next = node->next;
			node->next->prev = node->prev;
		}
	}
	free(node);
	node = NULL;
}

void create_bat(const int n)
{
	//create new bat...
	int i;
	for (i=0; i<n; i++) {
		Bat *node = (Bat *)malloc(sizeof(Bat));
		if (node == NULL) {
			Log("error allocating node.\n");
			exit(EXIT_FAILURE);
		}
		node->prev = NULL;
		node->next = NULL;
		node->sound=0;
		node->slow=0;
		//
		//this is a bat!
		//start off-screen so to build up some speed before entering.
		node->pos[0] = -100;
		node->pos[1] = rnd() * (float)yres;
		VecCopy(node->pos, node->lastpos);
		node->vel[0] = rnd() * 4.0f + 4.0f;
		node->vel[1] = rnd() * 6.0f - 3.0f;
		node->color[0] = rnd() * 0.2f + 0.1f;
		node->color[1] = rnd() * 0.1f + 0.1f;
		node->color[2] = rnd() * 0.1f + 0.1f;
		node->color[3] = rnd() * 0.1f + 0.3f; //alpha
		node->color[3] = rnd() * 0.1f + 0.3f; //alpha
		VecZero(node->rotate);
		node->rotate[0] = rnd() * 360.0;
		node->rot = rnd() + 4.0f;
		//larger linewidth = faster speed
		//node->maxvel[1] = (float)(node->linewidth*16);
		//node->length = node->maxvel[1] * 0.2f + rnd();
		node->scale = rnd() + 1.0f;
		if (rnd() < 0.02f) {
			//a few large bats flying fast.
			node->scale = rnd()*4.0f + 2.0f;
		}
		node->vel[0] = node->scale * 4.0f + (node->scale * 1.5f);
		Flt s = rnd() * node->scale * 4.0f;
		node->vel[1] = s - (2.0f);
		node->image = rand() % 8;
		//put bat into linked list
		node->next = ihead;
		if (ihead != NULL)
			ihead->prev = node;
		ihead = node;
		++totbat;
	}
}

void check_bats()
{
	if (random(100) < 80) {
		create_bat(160);
	}
	//
	//move bats
	Bat *node = ihead;
	while(node) {
		//these are bats, so force is sideways!
		VecCopy(node->pos, node->lastpos);
		node->rotate[0] += node->rot;
		//
		//---------------------------------------------------------------------
		//---------------------------------------------------------------------
		//---------------------------------------------------------------------
		//this is the most important part of the whole program!!!!!
		//---------------------------------------------------------------------
		//---------------------------------------------------------------------
		//---------------------------------------------------------------------
		//
		if (batman && node->scale < 2.0f) {
			//a bat will slow down if in front of the batman symbol.
			//what is the relative x,y position of the bat right now?
			float xpct = node->pos[0] / (Flt)xres;
			float ypct = node->pos[1] / (Flt)yres;
			ypct = 1.0f - ypct;
			//
			int relx = (int)((Flt)batlogoImage->width * xpct);
			int rely = (int)((Flt)batlogoImage->height * ypct);
			//
			//what is the color of the bat symbol at the position?
			//
			int offset = (rely * batlogoImage->width * 3 + relx * 3);
			unsigned char *ptr = (unsigned char *)batlogoImage->data + offset;
			if (*ptr < 100) {
				//bat is in front of logo. slow it down.
				if (!node->slow) {
					node->slow=1;
					node->vel[0] = rnd() * 2.0f + 1.0f;
					node->vel[1] = rnd() * 2.0f - 1.0f;
				}
			} else {
				if (node->slow) {
					node->vel[0] = node->scale * 4.0f + (node->scale * 1.5f);
					Flt s = rnd() * node->scale * 4.0f;
					node->vel[1] = s - (2.0f);
					node->slow=0;
				}
				node->slow=0;
			}
		}
		node->pos[0] += node->vel[0] * timeslice;
		node->pos[1] += node->vel[1] * timeslice;
		node = node->next;
	}
	//---------------------------------------------------------------------
	//---------------------------------------------------------------------
	//---------------------------------------------------------------------
	//---------------------------------------------------------------------
	//
	//check bats
	int n=0;
	node = ihead;
	while(node) {
		n++;
		if (node->pos[1] >= yres || node->pos[1] < 0) {
			Bat *savenode = node->next;
			delete_bat(node);
			node = savenode;
			continue;
		}
		if (node->pos[0] > xres) {
			//bat is past right side of screen
			Bat *savenode = node->next;
			delete_bat(node);
			node = savenode;
			continue;
		}
		node = node->next;
	}
	if (batman) {
		if (timeDiff(&batTime, &timeCurrent) > 11.0) {
			disburseBatman();
			timeCopy(&batAuto, &timeCurrent);
			batman=0;
		}
	}
	if (!batman) {
		double diff = timeDiff(&batAuto, &timeCurrent);
		//printf("diff: %lf\n",diff);
		if (diff > 10.0) {
			//printf("batAuto-----\n");
			timeCopy(&batTime, &timeCurrent);
			timeCopy(&batAuto, &timeCurrent);
			batman=1;
		}
	}
}

void physics(void)
{
	check_bats();
}

void test_bats(void)
{
	float e = 1.0f / 8.0f;
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glBindTexture(GL_TEXTURE_2D, batsTexture);
		glColor4f(1.0f,1.0f,1.0f,0.8f);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f,0.0f); glVertex2f(  0.0f,   0.0f);
			glTexCoord2f(0.0f,1.0f); glVertex2f(  0.0f, 400.0f);
			glTexCoord2f(e,   1.0f); glVertex2f(400.0f, 400.0f);
			glTexCoord2f(e,   0.0f); glVertex2f(400.0f,   0.0f);
		glEnd();
	glDisable(GL_ALPHA_TEST);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_bats(void)
{
	float e0, e1, e = 1.0f / 8.0f;
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glBindTexture(GL_TEXTURE_2D, batsTexture);
	Bat *node = ihead;
	while(node) {
		e0 = (Flt)node->image * e;
		e1 = e0 + e;
		glPushMatrix();
		glTranslatef(node->pos[0],node->pos[1],node->pos[2]);
		Flt scale = node->scale;
		//if (node->slow)
		//	scale *= 1.5f;
		glScalef(scale,scale,scale);
		glColor4f(0.0f,0.0f,0.0f,1.0f);
		//if (node->scale > 3.0f) {
		//	glEnable(GL_BLEND);
		//		glColor4f(1.0f,1.0f,1.0f,1.0f);
		//}
		glRotatef(node->rotate[0], 1.0, 0.0, 0.0);
		glBegin(GL_QUADS);
			glTexCoord2f(e0,0.0f); glVertex2f( 0.0f,  0.0f);
			glTexCoord2f(e0,1.0f); glVertex2f( 0.0f, 10.0f);
			glTexCoord2f(e1,1.0f); glVertex2f(10.0f, 10.0f);
			glTexCoord2f(e1,0.0f); glVertex2f(10.0f,  0.0f);
		glEnd();
		glPopMatrix();
		//glDisable(GL_BLEND);
		node = node->next;
	}
	glDisable(GL_ALPHA_TEST);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void render(void)
{
	Rect r;
	//Clear the screen
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	//
	//draw the background quad with texture
	glColor3f(1.0, 1.0, 1.0);
	glBindTexture(GL_TEXTURE_2D, batbackgroundTexture);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(0, yres);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(xres, yres);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(xres, 0);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_BLEND);
	//test_bats();
	draw_bats();
	//glDisable(GL_BLEND);
	//
	//
	//
	r.bot = yres - 20;
	r.left = 10;
	r.center = 0;
	ggprint8b(&r, 16, 0, "B - Batman");
}

