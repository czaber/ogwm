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
#include "uwm.h"
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

void list_windows();
void (*BindTexImageEXT)(Display*, GLXDrawable, int, const int*);
void (*ReleaseTexImageEXT)(Display*, GLXDrawable, int);

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
		for (i=0; i<MAX_WINDOWS; i++)
			workspace[i] = 0;
	}

	for (i=0; i<MAX_WINDOWS; i++)
		if (workspace[i] == 0)
			break;

	workspace[i] = w;
	debug("[AddWindow] Added window 0x%x at position 0x%x\n", w, i);
}


int on_xerror(Display* dpy, XErrorEvent* e) {
	char emsg[80];
	Display* edpy = e->display;
	XID exid = e->resourceid;
	XGetErrorText(dpy, e->error_code, emsg, 80);
	error("[XError] %s\n", emsg);
	debug("[XError] for 0x%x on display 0x%x\n", exid, edpy);
	return 1;
}
void on_keypress(XEvent* e) {
	notify("[KeyPress] Pressed key 0x%x\n", e->xkey.keycode);
}
void on_expose(XEvent* e) { }
void on_maprequest(XEvent* e) {
	Window w = e->xmap.window;
	if (w == root ||  w == frontplane || w == backplane)
		return;
	info("[MapRequest] Mapping 0x%x\n", w);
}
void on_enternotify(XEvent* e) { }

GLXFBConfig fbconfig(Window w, GLfloat *top, GLfloat *bottom) {
	int i;
	int nfbc;
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, w, &attr);
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
		if (!(v & GLX_TEXTURE_2D_BIT_EXT))
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

void draw() {
	int i;
	int pixmapAttr[] = {GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT, 
			    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_RECTANGLE_EXT, None};
	GLfloat top, bottom;
	GLuint texture;
	
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
		info("Got pixmap\n");
		GLXPixmap glxpixmap = glXCreatePixmap(dpy, fbc, pixmap, pixmapAttr);
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		BindTexImageEXT (dpy, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBegin(GL_QUADS);
		glColor3d(0.8, 0.3, 0.0);
		glTexCoord2d(0.0f, bottom);	glVertex2d(0.0f, 0.0f);
		glTexCoord2d(0.0f, top);	glVertex2d(0.0f, 1.0f);
		glTexCoord2d(1.0f, top);	glVertex2d(1.0f, 1.0f);
		glTexCoord2d(1.0f, bottom);	glVertex2d(1.0f, 0.0f);
		glEnd();
		ReleaseTexImageEXT (dpy, glxpixmap, GLX_FRONT_LEFT_EXT);
	}
	XSync(dpy, 0);

	glXSwapBuffers(dpy, overlay);

}

void on_mapnotify(XEvent* e) {
	Window p = e->xmap.event;
	Window w = e->xmap.window;
	if (w == root)
		return;
	debug("[MapNotify] Mapped 0x%x to 0x%x\n", w, p);
	draw();
}
void on_unmapnotify(XEvent* e) { }
void on_buttonpressed(XEvent* e) { }
void on_visibilitynotify(XEvent* e) {
	Window w = e->xvisibility.window;
	debug("[VisibilityNotify] Window 0x%x is visible\n", w);
}

void on_destroynotify(XEvent* e) {
	Window w = e->xdestroywindow.window;
	if (w == root || w == overlay || w  == backplane)
		return;
	debug("[DestroyNotify] Destroying 0x%x\n", w);
	del(w);
	draw();
}
void on_createnotify(XEvent* e) {
	Window w = e->xcreatewindow.window;
	if (w == root || w == overlay || w == backplane || w == frontplane)
		return;
	//XReparentWindow(dpy, w, overlay, 0, 0);
	add(w);
	list_windows();
}
void on_configurenotify(XEvent* e) { }
void on_configurerequest(XEvent* e) { }

void on_event(XEvent* e) {
	if (event_names[e->type])
		info("[Event] Got %s event\n", event_names[e->type]);
	else// if (0)
		warn("[Event] Unknown event %d\n", e->type);

	if (events[e->type])
		events[e->type](e);
}

Window create_overlay(void) {
	Window w = XCompositeGetOverlayWindow(dpy, root);
	ignore_input(w);
	XSelectInput (dpy, w, SubstructureNotifyMask|VisibilityChangeMask|ExposureMask);
	return w;
}

Window create_backplane(void) {
	Window w;
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, root, &attr);
	w = XCreateWindow(dpy, root, 0, 0, 
			attr.width, attr.height, 0, 0,
			InputOutput, DefaultVisual (dpy, 0),
			0, NULL);
	XSelectInput (dpy, w, SubstructureNotifyMask 
			| StructureNotifyMask
			| FocusChangeMask
			| PointerMotionMask
			| KeyPressMask
			| KeyReleaseMask
			| ButtonPressMask
			| ButtonReleaseMask
			| ExposureMask 
			| VisibilityChangeMask
			| PropertyChangeMask);
	XMapWindow(dpy, w);
	XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
	return w;
}

Window create_frontplane(void) {
	Window w;
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
	GLint glattr[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
	XWindowAttributes xattr;
	XVisualInfo* vi;
	GLXContext glc;

	XGetWindowAttributes(dpy, overlay, &xattr);

	vi = glXChooseVisual(dpy, 0, glattr);
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);

	glXMakeCurrent(dpy, overlay, glc);
	glViewport(0, 0, xattr.width, xattr.height);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH,GL_NICEST);

	*(void**) (&BindTexImageEXT) = glXGetProcAddress((GLubyte*)"glXBindTexImageEXT");
	*(void**) (&ReleaseTexImageEXT) = glXGetProcAddress((GLubyte*)"glXReleaseTexImageEXT");
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
	debug("Overlay present => 0x%x\n", overlay);
	backplane = create_backplane();
	debug("Backplane present => 0x%x\n", backplane);
	frontplane = create_frontplane();
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

void _list_windows(int w, int indent) {
	Window r;
	Window p;
	Window* c=NULL;
	int i,j;
	unsigned int nc;
	if(XQueryTree(dpy, w, &r, &p, &c, &nc)==0)
		return;

	for (j=0; j<indent; j++)
		fprintf(stderr," ");
	fprintf(stderr,"0x%x (0x%x)\n", w, p);
	for (i=0; i<nc; i++)
		_list_windows(c[i], indent+2);
	XFree(c);
}

void list_windows() {
	_list_windows(root, 0);
	_list_windows(overlay, 2);
}

int main(int argc, char** argv) {
	dpy = XOpenDisplay(NULL);
	if(!dpy)
		die(ERR_CANNOT_OPEN_DISPLAY, "Cannot open display!\n");

	setup_x();
	setup_gl();
	start();
	clean();

	return 0;
}
