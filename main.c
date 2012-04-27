// Standard
#include <string.h>
// OpenGL (GLX)
#include <GL/glx.h>
#include <GL/glxext.h>
// Pure X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
// Extensions
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
// Internal
#include "main.h"
// Logging
#define LOG_LEVEL DEBUG
#include "logger.h"
#define MAX_WINDOWS 7

static int stop = 0;
static int screen = 0;
static Window* workspace = NULL;
static Display* dpy = NULL;
static Window root = 0;
static Window backplane = 0;
static Window frontplane = 0;
static Window overlay = 0;
void (*glXQueryDrawableProc)(Display*, GLXDrawable, int, const int*);
void (*glXBindTexImageProc)(Display*, GLXDrawable, int, const int*);
void (*glXReleaseTexImageProc)(Display*, GLXDrawable, int);

void del(Window w) {
	int i;
	if (w == root ||  w == frontplane || w == backplane)
		return;
	if (workspace == NULL)
		die(1, "There is no windows.\n");
	for (i=0; i<MAX_WINDOWS; i++)
		if (workspace[i] == w)
			break;
	if (i >= MAX_WINDOWS)
		die(2, "Window not found\n");

	workspace[i] = 0;
	debug("[DelWindow] Deleted window 0x%x from position %d\n", w, i);
}

void add(Window w) {
	int i;
	if (w == root ||  w == frontplane || w == backplane)
		return;
	if (workspace == NULL) {
		workspace = malloc(sizeof(Window) * MAX_WINDOWS);
		memset(workspace, 0, sizeof(Window) * MAX_WINDOWS);
	}

	for (i=0; i<MAX_WINDOWS && workspace[i] != 0; i++)
		if (workspace[i] == 0)
			break;

	workspace[i] = w;
	debug("[AddWindow] Added window 0x%x at position 0x%x\n", w, i);
}


int on_xerror(Display* dpy, XErrorEvent* e) {
	char emsg[80];
	XGetErrorText(dpy, e->error_code, emsg, 80);
	error("[XError] %s\n", emsg);
	return 1;
}

void on_maprequest(XEvent* e) {
	Window w = e->xmap.window;
	if (w == root ||  w == frontplane || w == backplane)
		return;
	info("[MapRequest] Mapping 0x%x\n", w);
}

GLXFBConfig fbconfig(Window w, GLfloat *top, GLfloat *bottom) {
	int i;
	int nfbc;
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, overlay, &attr);
	VisualID vid = XVisualIDFromVisual(attr.visual);
	GLXFBConfig* fbc = glXGetFBConfigs(dpy, screen, &nfbc);
	for (i=0; i<nfbc; i++) {
		int v;
		XVisualInfo *vi = glXGetVisualFromFBConfig(dpy, fbc[i]);
		if (!vi || vi->visualid != vid)
			continue;

		glXGetFBConfigAttrib(dpy, fbc[i], GLX_DRAWABLE_TYPE, &v);
		if (!(v & GLX_PIXMAP_BIT))
			continue;
		
		glXGetFBConfigAttrib(dpy, fbc[i], GLX_BIND_TO_TEXTURE_TARGETS_EXT, &v);
		if (!(v & GLX_TEXTURE_RECTANGLE_BIT_EXT))
			continue;
		
		glXGetFBConfigAttrib(dpy, fbc[i], GLX_BIND_TO_TEXTURE_RGBA_EXT, &v);
		if (!(v))
			glXGetFBConfigAttrib(dpy, fbc[i], GLX_BIND_TO_TEXTURE_RGB_EXT, &v);
		if (!(v))
			continue;
		
		glXGetFBConfigAttrib(dpy, fbc[i], GLX_Y_INVERTED_EXT, &v);
		*top    = (!v)? 0.0f : 1.0f;
		*bottom = (!v)? 1.0f : 0.0f;
		break;
	}

	if (i == nfbc)
		die(2, "No FBCofig found!"); 

	return fbc[i];
}

void check_gl(int line) {
	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
		error("[Draw] Error 0x%x while drawing in line %d\n", (int)err, line);
}

void draw() {
	int i;
	GLfloat top, bottom;
	GLuint texture;

	XGrabServer(dpy);
	XSync(dpy, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBegin(GL_QUADS);
	glColor3f(.7, 0., .9);
	glVertex3f(-.95, -.95, 0.);
	glVertex3f( .95, -.95, 0.);
	glColor3f(.3, 0., .4);
	glVertex3f( .95,  .95, 0.);
	glVertex3f(-.95,  .95, 0.);
	glEnd();
	
	for (i=0; i<MAX_WINDOWS; i++) {
		if (workspace == NULL || workspace[i] == 0)
			break;
		Window w = workspace[i];
		info("Drawing %d window => 0x%x\n", i, w);
		GLXFBConfig fbc = fbconfig(w, &top, &bottom);
		Pixmap pixmap = XCompositeNameWindowPixmap(dpy, w);
		XSync(dpy, 0);
		GLXPixmap glxpixmap = glXCreatePixmap(dpy, fbc, pixmap, pixmap_attr);
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_RECTANGLE, texture);
		glXBindTexImageProc(dpy, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBegin(GL_QUADS);
		glColor3d(0.8, 0.0, 0.0);
		glTexCoord2d(0.0f, bottom);	glVertex2d(-0.5f,-0.5f);
		glTexCoord2d(0.0f, top);	glVertex2d(-0.5f, 0.5f);
		glTexCoord2d(1.0f, top);	glVertex2d( 0.5f, 0.5f);
		glTexCoord2d(1.0f, bottom);	glVertex2d( 0.5f,-0.5f);
		glEnd();
		glXReleaseTexImageProc(dpy, glxpixmap, GLX_FRONT_LEFT_EXT);
	}
	glFlush();
	glXSwapBuffers(dpy, overlay);
	XUngrabServer(dpy);

}

void on_mapnotify(XEvent* e) {
	Window p = e->xmap.event;
	Window w = e->xmap.window;
	if (w == root)
		return;
	debug("[MapNotify] Mapped 0x%x to 0x%x\n", w, p);
	draw();
}

void on_destroynotify(XEvent* e) {
	Window w = e->xdestroywindow.window;
	if (w == root || w == overlay || w  == backplane)
		return;
	del(w);
	draw();
}
void on_createnotify(XEvent* e) {
	Window w = e->xcreatewindow.window;
	if (w == root || w == overlay || w == backplane || w == frontplane)
		return;
	add(w);
	for_windows(root, 0, &print);
}

void on_event(XEvent* e) {
	if (event_names[e->type])
		info("[Event] Got %s event\n", event_names[e->type]);
	if (events[e->type])
		events[e->type](e);
}

Window create_overlay(void) {
	Window w = XCompositeGetOverlayWindow(dpy, root);
	ignore_input(w);
	XSelectInput (dpy, w, SubstructureNotifyMask | VisibilityChangeMask | ExposureMask);
	return w;
}

Window create_backplane(void) {
	Window w;
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, root, &attr);
	w = XCreateWindow(dpy, root, 0, 0, attr.width, attr.height, 0, 0,
			InputOutput, DefaultVisual (dpy, 0), 0, NULL);
	XSelectInput (dpy, w, SubstructureNotifyMask | PropertyChangeMask);
	XMapWindow(dpy, w);
	XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
	return w;
}

Window create_frontplane(void) {
	Window w = 0;
	return w;
}

void ignore_input(Window w) {
	// Configure window not to receive mouse events
	XserverRegion region = XFixesCreateRegion(dpy, NULL, 0);
	XFixesSetWindowShapeRegion(dpy, w, ShapeBounding, 0, 0, 0);
	XFixesSetWindowShapeRegion(dpy, w, ShapeInput, 0, 0, region);
	XFixesDestroyRegion(dpy, region);
}

void setup_gl(void) {
	XWindowAttributes xattr;
	XVisualInfo* vi;
	GLXContext glc;

	XGetWindowAttributes(dpy, overlay, &xattr);

	vi = glXChooseVisual(dpy, 0, gl_attr);
	glc = glXCreateContext(dpy, vi, NULL, GL_FALSE);

	glXMakeCurrent(dpy, overlay, glc);
	glViewport(0, 0, xattr.width, xattr.height);

	*(void**) (&glXQueryDrawableProc) = glXGetProcAddress((GLubyte*)"glXQueryDrawable");
	*(void**) (&glXBindTexImageProc) = glXGetProcAddress((GLubyte*)"glXBindTexImageEXT");
	*(void**) (&glXReleaseTexImageProc) = glXGetProcAddress((GLubyte*)"glXReleaseTexImageEXT");
}

void setup_x(void) {
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	debug("root present => 0x%x\n", root);

	XSetErrorHandler(on_xerror);
	XSelectInput (dpy, root, SubstructureNotifyMask|VisibilityChangeMask|ExposureMask);
	XCompositeRedirectSubwindows(dpy, root, CompositeRedirectAutomatic);
	info("[Setup] We are going to be notified\n");

	overlay = create_overlay();
	backplane = create_backplane();
	frontplane = create_frontplane();
	debug("Overlay present => 0x%x\n", overlay);
	debug("Backplane present => 0x%x\n", backplane);
	debug("Frontplane present => 0x%x\n", frontplane);

	stop = 0;
	info("We are ready to go\n");
}

void start(void) {
	XEvent ev;
	debug("[Start] Started\n");
	XSync(dpy, 0);
	while(!stop && !XNextEvent(dpy, &ev))
		on_event(&ev);
	debug("[Start] Done\n");
}

void clean(void) {
	debug("Quit.");
	XCloseDisplay(dpy);
}

void for_windows(Window w, int lvl, void (*f)(Window w, Window p, int lvl)) {
	unsigned int i, nc;
	Window r, p, *c=NULL;
	if(XQueryTree(dpy, w, &r, &p, &c, &nc)==0)
		return;
	f(w, p, lvl);
	for (i=0; i<nc; i++)
		for_windows(c[i], lvl+1, f);
	XFree(c);
}

int main(int argc, char** argv) {
	dpy = XOpenDisplay(NULL);
	if(!dpy) die(ERR_CANNOT_OPEN_DISPLAY, "Cannot open display!\n");
	setup_x();
	setup_gl();
	start();
	clean();
	return 0;
}
