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

static int stop = 0;
static int screen = 0;
static Display* dpy = NULL;
static Window root = 0;
static Window backplane = 0;
static Window frontplane = 0;
static Window overlay = 0;

void (*BindTexImageEXT)(Display*, GLXDrawable, int, const int*);
void (*ReleaseTexImageEXT)(Display*, GLXDrawable, int);

int on_xerror(Display* dpy, XErrorEvent* e) {
	char emsg[80];
	Display* edpy = e->display;
	XID exid = e->resourceid;
	XGetErrorText(dpy, e->error_code, emsg, 80);
	error("[XError] %s\n", emsg);
	debug("[XError] for %ld on display %d\n", exid, edpy);
	return 1;
}
void on_keypress(XEvent* e) {
	notify("Pressed key %d\n", e->xkey.keycode);
}
void on_expose(XEvent* e) { }
void on_maprequest(XEvent* e) {
	Window w = e->xany.window;
	if (w == root || w == overlay ||  w == frontplane || w == backplane)
		return;
	XMapWindow(dpy, w);
	info("Mapping window %d\n", w);
}
void on_enternotify(XEvent* e) { }
void on_mapnotify(XEvent* e) {
	int nfbc;
	int i;
	int pixmapAttr[] = {
			  GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
			  GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_RECTANGLE_EXT,
			  None};
	GLfloat top, bottom;
	GLuint texture;
	GLXFBConfig* fbc;
	GLXPixmap glxpixmap;
	XWindowAttributes attr;
	Pixmap pixmap;
	Window w = e->xany.window;
	*(void**) (&BindTexImageEXT) = glXGetProcAddress("glXBindTexImageEXT");
	debug("Got glXBindTexImageEXT\n");
	*(void**) (&ReleaseTexImageEXT) = glXGetProcAddress("glXReleaseTexImageEXT");
	debug("Got glXReleaseTexImageEXT\n");
	
	notify("Window %d mapped\n", w);

	XGetWindowAttributes(dpy, w, &attr);
	VisualID vid = XVisualIDFromVisual(attr.visual);
	debug("Window %d is viewable? %d\n", w, attr.map_state == IsViewable);
	debug("Window %d is unmapped? %d\n", w, attr.map_state == IsUnmapped);
	debug("Window %d is unviewable? %d\n", w, attr.map_state == IsUnviewable);
	fbc = glXGetFBConfigs(dpy, screen, &nfbc);
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
		top    = (!v)? 0.0f : 1.0f;
		bottom = (!v)? 1.0f : 0.0f;
		break;
	}

	if (i == nfbc)
		die(2, "No FBCofig found!"); 
	debug("bottom: %f, top: %f\n", bottom, top);
		

	pixmap = XCompositeNameWindowPixmap(dpy, w);
	debug("Got pixmap for window %d -> %d\n", w, pixmap);
	XSync(dpy, 0);
	
	glxpixmap = glXCreatePixmap(dpy, fbc[i], pixmap, pixmapAttr);
	debug("Got GLXpixmap for pixmap %d -> %d\n", pixmap, glxpixmap);
	XSync(dpy, 0);
	
	glGenTextures(1, &texture);
	debug("Generated texture %d\n", texture);
	XSync(dpy, 0);
	
	glBindTexture(GL_TEXTURE_2D, texture);
	debug("Binded texture %d\n", texture);
	XSync(dpy, 0);
	

	BindTexImageEXT (dpy, glxpixmap, GLX_FRONT_LEFT_EXT, NULL);
	debug("glXBindTexImageEXT invoked\n");

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	

	debug("Start Quad\n");
	glBegin(GL_QUADS);
	glColor3f(1.0f, 0.0f, 0.0f);
	glTexCoord2d(0.0f, bottom);
	glVertex2d(0.0f, 0.0f);
	glTexCoord2d(0.0f, top);
	glVertex2d(0.0f, 1.0f);
	glTexCoord2d(1.0f, top);
	glVertex2d(1.0f, 1.0f);
	glTexCoord2d(1.0f, bottom);
	glVertex2d(1.0f, 0.0f);
	glEnd();
	debug("Done with Quad\n");
	XSync(dpy, 0);
	debug("XSync'd\n");
	glXSwapBuffers(dpy, overlay);
	debug("Swapped buffers\n");
	ReleaseTexImageEXT (dpy, glxpixmap, GLX_FRONT_LEFT_EXT);
	debug("glXReleaseTexImageEXT invoked\n");
}
void on_unmapnotify(XEvent* e) { }
void on_buttonpressed(XEvent* e) { }
void on_destroynotify(XEvent* e) { }
void on_configurenotify(XEvent* e) { }
void on_configurerequest(XEvent* e) { }

void on_event(XEvent* e) {
	if (event_names[e->type])
		info("Got %s event\n", event_names[e->type]);
	else
		warn("Unknown event %d\n", e->type);

	if (events[e->type])
		events[e->type](e);
}

Window create_overlay(void) {
	Window w = XCompositeGetOverlayWindow(dpy, root);
	ignore_input(w);
	return w;
}

Window create_backplane(void) {
	Window w;
	XWindowAttributes attr;
	XGetWindowAttributes(dpy, root, &attr);
	w = XCreateWindow(dpy, root, 0, 0, 
		attr.width, attr.height, 0, 0,
		InputOnly, DefaultVisual (dpy, 0),
		0, NULL);
	XSelectInput (dpy, w, StructureNotifyMask
				| FocusChangeMask
				| PointerMotionMask
				| KeyPressMask
				| KeyReleaseMask
				| ButtonPressMask
				| ButtonReleaseMask
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

	vi = glXChooseVisual(dpy, 0, glattr);
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, overlay, glc);
	XGetWindowAttributes(dpy, overlay, &xattr);
  	glViewport(0, 0, xattr.width, xattr.height);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH,GL_NICEST);

	glBegin(GL_QUADS);
	glColor3f(.7, 0., .9);
	glVertex3f(-.75, -.75, 0.);
	glVertex3f( .75, -.75, 0.);
	glColor3f(.3, 0., .4);
	glVertex3f( .75,  .75, 0.);
	glVertex3f(-.75,  .75, 0.);
	glEnd();

	glXSwapBuffers(dpy, overlay);
}

void setup_x(void) {
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	
	// Tell X server to redirect client output off-screen
	XCompositeRedirectSubwindows(dpy, overlay, CompositeRedirectAutomatic);
	debug("[Setup] Composition enabled for root window %d\n", root);
	// Get notified when windows are created
	XSelectInput (dpy, root, SubstructureNotifyMask|SubstructureRedirectMask);
	info("[Setup] We are going to be notified\n");

	XSetErrorHandler(on_xerror);
	overlay = create_overlay();
	debug("Overlay present => %d\n", overlay);
	backplane = create_backplane();
	debug("Backplane present => %d\n", backplane);
	frontplane = create_frontplane();
	debug("Frontplane present => %d\n", frontplane);
	info("We are ready to go\n");
	
	stop = 0;
}

void start(void) {
	XEvent ev;
	debug("[Start] Started\n");
	// Sync
	XSync(dpy, 0);
	while(!stop && !XNextEvent(dpy, &ev))
		on_event(&ev);
	debug("[Start] Done\n");
}

void clean(void) {
	debug("Quit.");
	XCloseDisplay(dpy);
}

int main(int argc, char** argv) {
	dpy = XOpenDisplay(NULL);
	if(!dpy)
		die(ERR_CANNOT_OPEN_DISPLAY, "Cannot open display!");

	setup_x();
	setup_gl();
	start();
	clean();

	return 0;
}
