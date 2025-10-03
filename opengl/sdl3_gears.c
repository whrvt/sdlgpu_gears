/*
 * Copyright (C) 1999-2001  Brian Paul        All Rights Reserved.
 * Copyright (C) 2025       William Horvath   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* NOTE: compile with cc -Wall sdl3_gears.c -o sdl3_gears -lSDL3 -lGL -lm */

#include <GL/gl.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif

/** Event handler results: */
enum
{
	NOP = 0,
	EXIT = 1,
	DRAW = 2
};

static GLdouble view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLuint gear1, gear2, gear3;
static GLdouble angle = 0.0;

static bool fullscreen = false; /* create a single fullscreen window */
static int samples = 0;         /* choose visual with at least N samples. */
static bool animate = true;     /* animation */

/* return current time (in seconds) */
static double current_time(void)
{
	static SDL_Time time = 0;
	if (!SDL_GetCurrentTime(&time))
		printf("SDL_GetCurrentTime error: %s\n", SDL_GetError());

	return (double)time / (double)SDL_NS_PER_SECOND;
}

/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 *
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void gear(GLdouble inner_radius, GLdouble outer_radius, GLdouble width, GLint teeth, GLdouble tooth_depth)
{
	GLdouble r0 = 0.0, r1 = 0.0, r2 = 0.0;
	GLdouble angle = 0.0, da = 0.0;
	GLdouble u = 0.0, v = 0.0;
	GLdouble len = 0.0;

	r0 = inner_radius;
	r1 = outer_radius - (tooth_depth / 2.0);
	r2 = outer_radius + (tooth_depth / 2.0);

	da = 2.0 * M_PI / teeth / 4.0;

	glShadeModel(GL_FLAT);

	glNormal3d(0.0f, 0.0f, 1.0f);

	/* draw front face */
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= teeth; i++)
	{
		angle = i * 2.0 * M_PI / teeth;
		glVertex3d(r0 * cos(angle), r0 * sin(angle), width * 0.5);
		glVertex3d(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		if (i < teeth)
		{
			glVertex3d(r0 * cos(angle), r0 * sin(angle), width * 0.5);
			glVertex3d(r1 * cos(angle + (3 * da)), r1 * sin(angle + (3 * da)), width * 0.5);
		}
	}
	glEnd();

	/* draw front sides of teeth */
	glBegin(GL_QUADS);
	da = 2.0 * M_PI / teeth / 4.0;
	for (int i = 0; i < teeth; i++)
	{
		angle = i * 2.0 * M_PI / teeth;

		glVertex3d(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		glVertex3d(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
		glVertex3d(r2 * cos(angle + (2 * da)), r2 * sin(angle + (2 * da)), width * 0.5);
		glVertex3d(r1 * cos(angle + (3 * da)), r1 * sin(angle + (3 * da)), width * 0.5);
	}
	glEnd();

	glNormal3d(0.0, 0.0, -1.0);

	/* draw back face */
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= teeth; i++)
	{
		angle = i * 2.0 * M_PI / teeth;
		glVertex3d(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
		glVertex3d(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		if (i < teeth)
		{
			glVertex3d(r1 * cos(angle + (3 * da)), r1 * sin(angle + (3 * da)), -width * 0.5);
			glVertex3d(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		}
	}
	glEnd();

	/* draw back sides of teeth */
	glBegin(GL_QUADS);
	da = 2.0 * M_PI / teeth / 4.0;
	for (int i = 0; i < teeth; i++)
	{
		angle = i * 2.0 * M_PI / teeth;

		glVertex3d(r1 * cos(angle + (3 * da)), r1 * sin(angle + (3 * da)), -width * 0.5);
		glVertex3d(r2 * cos(angle + (2 * da)), r2 * sin(angle + (2 * da)), -width * 0.5);
		glVertex3d(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
		glVertex3d(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
	}
	glEnd();

	/* draw outward faces of teeth */
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i < teeth; i++)
	{
		angle = i * 2.0 * M_PI / teeth;

		glVertex3d(r1 * cos(angle), r1 * sin(angle), width * 0.5);
		glVertex3d(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
		u = (r2 * cos(angle + da)) - (r1 * cos(angle));
		v = (r2 * sin(angle + da)) - (r1 * sin(angle));
		len = sqrt((u * u) + (v * v));
		u /= len;
		v /= len;
		glNormal3d(v, -u, 0.0);
		glVertex3d(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
		glVertex3d(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
		glNormal3d(cos(angle), sin(angle), 0.0);
		glVertex3d(r2 * cos(angle + (2 * da)), r2 * sin(angle + (2 * da)), width * 0.5);
		glVertex3d(r2 * cos(angle + (2 * da)), r2 * sin(angle + (2 * da)), -width * 0.5);
		u = (r1 * cos(angle + (3 * da))) - (r2 * cos(angle + (2 * da)));
		v = (r1 * sin(angle + (3 * da))) - (r2 * sin(angle + (2 * da)));
		glNormal3d(v, -u, 0.0);
		glVertex3d(r1 * cos(angle + (3 * da)), r1 * sin(angle + (3 * da)), width * 0.5);
		glVertex3d(r1 * cos(angle + (3 * da)), r1 * sin(angle + (3 * da)), -width * 0.5);
		glNormal3d(cos(angle), sin(angle), 0.0);
	}

	glVertex3d(r1 * cos(0), r1 * sin(0), width * 0.5);
	glVertex3d(r1 * cos(0), r1 * sin(0), -width * 0.5);

	glEnd();

	glShadeModel(GL_SMOOTH);

	/* draw inside radius cylinder */
	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= teeth; i++)
	{
		angle = i * 2.0 * M_PI / teeth;
		glNormal3d(-cos(angle), -sin(angle), 0.0);
		glVertex3d(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
		glVertex3d(r0 * cos(angle), r0 * sin(angle), width * 0.5);
	}
	glEnd();
}

static void draw_gears(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glRotated(view_rotx, 1.0, 0.0, 0.0);
	glRotated(view_roty, 0.0, 1.0, 0.0);
	glRotated(view_rotz, 0.0, 0.0, 1.0);

	glPushMatrix();
	glTranslated(-3.0, -2.0, 0.0);
	glRotated(angle, 0.0, 0.0, 1.0);
	glCallList(gear1);
	glPopMatrix();

	glPushMatrix();
	glTranslated(3.1, -2.0, 0.0);
	glRotated((-2.0 * angle) - 9.0, 0.0, 0.0, 1.0);
	glCallList(gear2);
	glPopMatrix();

	glPushMatrix();
	glTranslated(-3.1, 4.2, 0.0);
	glRotated((-2.0 * angle) - 25.0, 0.0, 0.0, 1.0);
	glCallList(gear3);
	glPopMatrix();

	glPopMatrix();
}

/** Draw single frame, do SwapBuffers, compute FPS */
static void draw_frame(SDL_Window *window)
{
	static int frames = 0;
	static double tRot0 = -1.0;
	static double tRate0 = -1.0;
	double dt = 0.0f;
	double t = current_time();

	if (tRot0 < 0.0)
	{
		tRot0 = t;
	}
	dt = t - tRot0;
	tRot0 = t;

	if (animate)
	{
		/* advance rotation for next frame */
		angle += 70.0 * dt; /* 70 degrees per second */
		if (angle > 3600.0)
		{
			angle -= 3600.0;
		}
	}

	draw_gears();
	SDL_GL_SwapWindow(window);

	frames++;

	if (tRate0 < 0.0)
	{
		tRate0 = t;
	}
	if (t - tRate0 >= 5.0)
	{
		GLdouble seconds = t - tRate0;
		GLdouble fps = frames / seconds;
		printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds, fps);
		fflush(stdout);
		tRate0 = t;
		frames = 0;
	}
}

/* new window size or exposure */
static void reshape(int width, int height)
{
	glViewport(0, 0, (GLint)width, (GLint)height);

	GLdouble h = (GLdouble)height / (GLdouble)width;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslated(0.0, 0.0, -40.0);
}

static void init(void)
{
	static GLfloat pos[4] = {5.0f, 5.0f, 10.0f, 0.0f};
	static GLfloat red[4] = {0.8f, 0.1f, 0.0f, 1.0f};
	static GLfloat grn[4] = {0.0f, 0.8f, 0.2f, 1.0f};
	static GLfloat blu[4] = {0.2f, 0.2f, 1.0f, 1.0f};

	glLightfv(GL_LIGHT0, GL_POSITION, pos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);

	/* make the gears */
	gear1 = glGenLists(1);
	glNewList(gear1, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
	gear(1.0, 4.0, 1.0, 20, 0.7);
	glEndList();

	gear2 = glGenLists(1);
	glNewList(gear2, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, grn);
	gear(0.5, 2.0, 2.0, 10, 0.7);
	glEndList();

	gear3 = glGenLists(1);
	glNewList(gear3, GL_COMPILE);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blu);
	gear(1.3, 2.0, 0.5, 10, 0.7);
	glEndList();

	glEnable(GL_NORMALIZE);
}

/**
 * Handle one SDL event.
 * \return NOP, EXIT or DRAW
 */
static int handle_event(SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_EVENT_QUIT:
		return EXIT;
	case SDL_EVENT_WINDOW_EXPOSED:
		return DRAW;
	case SDL_EVENT_WINDOW_RESIZED:
		reshape(event->window.data1, event->window.data2);
		return DRAW;
	case SDL_EVENT_KEY_DOWN:
		switch (event->key.key)
		{
		case SDLK_LEFT:
			view_roty += 5.0;
			break;
		case SDLK_RIGHT:
			view_roty -= 5.0;
			break;
		case SDLK_UP:
			view_rotx += 5.0;
			break;
		case SDLK_DOWN:
			view_rotx -= 5.0;
			break;
		case SDLK_ESCAPE:
			return EXIT;
		case SDLK_A:
			animate = !animate;
			break;
		default:
			break;
		}
		return DRAW;
	default:
		break;
	}
	return NOP;
}

static void event_loop(SDL_Window *window)
{
	SDL_Event event;

	while (1)
	{
		int op = NOP;

		while (!animate || SDL_PollEvent(&event))
		{
			if (!animate)
				SDL_WaitEvent(&event);

			op = handle_event(&event);
			if (op == EXIT)
				return;
			if (op == DRAW)
				break;
		}

		draw_frame(window);
	}
}

static void usage(void)
{
	printf("Usage:\n");
	printf("  -samples N              run in multisample mode with at least N samples\n");
	printf("  -fullscreen             run in fullscreen mode\n");
	printf("  -info                   display OpenGL renderer info\n");
	printf("  -geometry WxH+X+Y       window geometry\n");
}

int main(int argc, char *argv[])
{
	int winWidth = 300;
	int winHeight = 300;
	int x = SDL_WINDOWPOS_CENTERED;
	int y = SDL_WINDOWPOS_CENTERED;
	SDL_Window *window = NULL;
	SDL_GLContext context = NULL;
	bool printInfo = false;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-info") == 0)
		{
			printInfo = true;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-samples") == 0)
		{
			samples = (int)strtol(argv[i + 1], NULL, 0);
			++i;
		}
		else if (strcmp(argv[i], "-fullscreen") == 0)
		{
			fullscreen = true;
		}
		else if (i < argc - 1 && strcmp(argv[i], "-geometry") == 0)
		{
			/* parse geometry string WxH+X+Y */
			char *geom = argv[i + 1];
			char *ptr = strchr(geom, 'x');
			if (ptr)
			{
				*ptr = '\0';
				winWidth = (int)strtol(geom, NULL, 0);
				geom = ptr + 1;

				ptr = strchr(geom, '+');
				if (ptr)
				{
					*ptr = '\0';
					winHeight = (int)strtol(geom, NULL, 0);
					geom = ptr + 1;

					x = (int)strtol(geom, NULL, 0);
					ptr = strchr(geom, '+');
					if (ptr)
						y = (int)strtol(ptr + 1, NULL, 0);
				}
				else
				{
					winHeight = (int)strtol(geom, NULL, 0);
				}
			}
			i++;
		}
		else
		{
			usage();
			return -1;
		}
	}

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		printf("Error: couldn't initialize SDL: %s\n", SDL_GetError());
		return -1;
	}

	/* set OpenGL attributes */
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (samples > 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
	}

	SDL_PropertiesID props = SDL_CreateProperties();

	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "SDL3 Gears");

	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, winWidth);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, winHeight);

	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, fullscreen);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);

	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);

	/* create the window */
	window = SDL_CreateWindowWithProperties(props);

	SDL_DestroyProperties(props);

	if (!window)
	{
		printf("Error: couldn't create window: %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	/* make sure the window state (size/position/fullscreen) has settled before doing anything else */
	SDL_SyncWindow(window);
	SDL_SetWindowResizable(window, true);

	if (fullscreen)
	{
		const SDL_DisplayMode *mode = NULL;
		if ((mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window))))
		{
			winWidth = mode->w;
			winHeight = mode->h;
		}
	}

	context = SDL_GL_CreateContext(window);
	if (!context)
	{
		printf("Error: couldn't create OpenGL context: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	if (!SDL_GL_MakeCurrent(window, context)) {
		printf("Error: couldn't make OpenGL context current: %s\n", SDL_GetError());
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return -1;
	}

	if (printInfo)
	{
		printf("GL_RENDERER   = %s\n", (const char *)glGetString(GL_RENDERER));
		printf("GL_VERSION    = %s\n", (const char *)glGetString(GL_VERSION));
		printf("GL_VENDOR     = %s\n", (const char *)glGetString(GL_VENDOR));
		printf("GL_EXTENSIONS = %s\n", (const char *)glGetString(GL_EXTENSIONS));
	}

	/* check swap interval */
	int interval = 0;

	SDL_GL_GetSwapInterval(&interval);
	if (interval > 0)
	{
		printf("Running synchronized to the vertical refresh.  The framerate should be\n");
		if (interval == 1)
			printf("approximately the same as the monitor refresh rate.\n");
		else if (interval > 1)
			printf("approximately 1/%u the monitor refresh rate.\n", interval);
	}

	init();

	/* set initial projection/viewing transformation */
	reshape((int)winWidth, (int)winHeight);

	event_loop(window);

	glDeleteLists(gear1, 1);
	glDeleteLists(gear2, 1);
	glDeleteLists(gear3, 1);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
