// Standard
#include <string.h>
#include <unistd.h>
#include <signal.h>
// OpenGL (GLX)
#include <GL/glx.h>
#include <GL/glxext.h>
// Pure X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
// Extensions
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/shape.h>
// Internal
#include "main.h"
// Logging
#define LOG_LEVEL LOG
#include "logger.h"
#define MAX_CLIENTS 7

static int stop = 0;
static int screen = 0;
static Client* workspace = NULL;
static Display* dpy = NULL;
static Window root = 0;
static Window overlay = 0;
static GLXWindow canvas = 0;
static GLXContext context = 0;
void (*glXQueryDrawableProc)(Display*, GLXDrawable, int, const int*);
void (*glXBindTexImageProc)(Display*, GLXDrawable, int, const int*);
void (*glXReleaseTexImageProc)(Display*, GLXDrawable, int);

// Options
Bool direct = True;
GLfloat left = 0.0f;
GLfloat right = 1.0f;
GLfloat top = 0.0f;
GLfloat bottom = 1.0f;
GLfloat ratio = 1.0f;
int width = 0;
int height = 0;

void del(Window w) {
	int i;
	if (w == root || w == overlay || w == canvas-1)
		return;
	if (workspace == NULL)
		die(1, "There is no windows.\n");
	for (i=0; i<MAX_CLIENTS; i++)
		if (workspace[i].window == w)
			break;
	if (i >= MAX_CLIENTS)
		return debug("Window not found\n");

	memset(workspace+i, 0, sizeof(Client));
	debug("[DelWindow] Deleted window 0x%x from position %d\n", w, i);
}

void add(Window w) {
    XWindowAttributes attr;
	int i;
	if (w == root || w == overlay || w == canvas-1)
		return;

	debug("[AddWindow] Adding window 0x%x\n", w);

	if (workspace == NULL) {
		workspace = malloc(sizeof(Client) * MAX_CLIENTS);
		memset(workspace, 0, sizeof(Client) * MAX_CLIENTS);
	}

	for (i=0; i<MAX_CLIENTS && workspace[i].window != 0; i++)
		if (workspace[i].window == 0)
			break;
	
	unsigned int v;
    int target;
	GLuint texture;
	GLXFBConfig fbc = choose_fbconfig();
	Pixmap pixmap = XCompositeNameWindowPixmap(dpy, w);
    debug("Got pixmap 0x%x for 0x%x\n", pixmap, w);
    XGetWindowAttributes(dpy, w, &attr);
    if (attr.depth == 32)
        pixmap_attr[1] = GLX_TEXTURE_FORMAT_RGBA_EXT;
    else
        pixmap_attr[1] = GLX_TEXTURE_FORMAT_RGB_EXT;
	GLXPixmap glxpixmap = glXCreatePixmap(dpy, fbc, pixmap, pixmap_attr);
    debug("Got glxpixmap 0x%x for 0x%x\n", glxpixmap, pixmap);
	check_gl(__LINE__);
	glXQueryDrawable(dpy, glxpixmap, GLX_WIDTH, &v);
	check_gl(__LINE__); debug("GLX is %d width\n", v);
	glXQueryDrawable(dpy, glxpixmap, GLX_HEIGHT, &v);
	check_gl(__LINE__); debug("GLX is %d height\n", v);
    glXQueryDrawableProc(dpy, glxpixmap, GLX_TEXTURE_TARGET_EXT, &target);
	check_gl(__LINE__); debug("GLX is 0x%x\n", target);
	glGenTextures(1, &texture);
	check_gl(__LINE__);
	
	switch (target) {
		case GLX_TEXTURE_2D_EXT:
			warn("GLX_TEXTURE_2D_EXT requested but we don't support that yet\n");
			target = GL_TEXTURE_2D;
			break;
		case GLX_TEXTURE_RECTANGLE_EXT:
			target = GL_TEXTURE_RECTANGLE_ARB;
			break;
		default:
			die(ERR_INVALID_TEXTURE, "Invalid target: 0x%x\n", target);
	}

	info("Window 0x%x has glx = %x, target 0x%x and texture %d\n",
		w, glxpixmap, target, texture);
	workspace[i].window = w;
	workspace[i].glxpixmap = glxpixmap;
	workspace[i].target = target;
	workspace[i].texture = texture;
    XGetWindowAttributes(dpy, w, &(workspace[i].geom));
}


int on_xerror(Display* dpy, XErrorEvent* e) {
	char emsg[80];
	XGetErrorText(dpy, e->error_code, emsg, 80);
	error("[XError] %s\n", emsg);
	return 1;
}

void on_keypress(XEvent* e) { }

void on_maprequest(XEvent* e) {
	Window w = e->xmap.window;
	if (w == root)
		return;
	info("[MapRequest] Mapping 0x%x\n", w);
}

void on_clientmessage(XEvent* e) {
	//info("Got client message\n");
	draw();
}

void fbconfig_attrs(GLXFBConfig fbc, int* width, int* height,
	GLfloat* top, GLfloat* bottom) {
	int v;
	XWindowAttributes xattr;
	XGetWindowAttributes(dpy, overlay, &xattr);
	glXGetFBConfigAttrib(dpy, fbc, GLX_Y_INVERTED_EXT, &v);
	*top    = (!v)? 0.0f : 1.0f;
	*bottom = (!v)? 1.0f : 0.0f;
	*width = xattr.width;
	*height = xattr.height;
}

GLXFBConfig choose_fbconfig() {
	int i, nfbc;
    GLXFBConfig* fbc = glXChooseFBConfig(dpy, screen, fbconfig_attr, &nfbc);
	check_gl(__LINE__);
    if (fbc == NULL)
        die(1, "No valid FBConfigs.\n");
	for (i=0; i<nfbc; i++) {
		int v;
        continue;
		//if (!vi || vi->visualid != vid) continue;
        debug("FBConfig #%d:\n", i);
        glXGetFBConfigAttrib(dpy, fbc[i], GLX_DRAWABLE_TYPE, &v);
        debug("GLX_DRAWABLE_TYPE: ");
        if (v & GLX_WINDOW_BIT)  log("WINDOW ");
        if (v & GLX_PIXMAP_BIT)  log("PIXMAP ");
        if (v & GLX_PBUFFER_BIT) log("PBUFFER ");
        log("\n");
        glXGetFBConfigAttrib(dpy, fbc[i], GLX_BIND_TO_TEXTURE_TARGETS_EXT, &v);
        debug("GLX_BIND_TO_TEXTURE_TARGETS_EXT: ");
        if (v & GLX_TEXTURE_1D_BIT_EXT) log("1D ");
        if (v & GLX_TEXTURE_2D_BIT_EXT) log("2D ");
        if (v & GLX_TEXTURE_RECTANGLE_BIT_EXT) log("RECTANGLE ");
        log("\n");
        glXGetFBConfigAttrib(dpy, fbc[i], GLX_BIND_TO_TEXTURE_RGBA_EXT, &v);
        debug("GLX_BIND_TO_TEXTURE_RGBA_EXT: ");
        if (v) log("yes ");
        else   log("no ");
        log("\n");
        glXGetFBConfigAttrib(dpy, fbc[i], GLX_BIND_TO_TEXTURE_RGB_EXT, &v);
        debug("GLX_BIND_TO_TEXTURE_RGB_EXT: ");
        if (v) log("yes ");
        else   log("no ");
        log("\n");
	}

    i = 0;
	if (i == nfbc)
		die(2, "No FBCofig found!\n"); 

	return fbc[i];
}

void check_gl(int line) {
	GLenum err = glGetError();
	if (err != GL_NO_ERROR)
		error("[Draw] Error 0x%x while drawing in line %d\n", (int)err, line);
}

void draw_background() {
	glBegin(GL_QUADS);
		glColor3f(.7, 0., .9);
		glVertex3f( 5.00f, 5.000f, 1.5f);
		glVertex3f( width-5.0f, 5.000f, 1.5f);
		glColor3f(.3, 0., .4);
		glVertex3f( width-5.0f, height-5.0f, 1.5f);
		glVertex3f( 5.00f, height-5.0f, 1.5f);
	glEnd();
	check_gl(__LINE__);
}

void draw() {
	int i;
	GLfloat width, height;
	glXWaitX(); check_gl(__LINE__);
	//XGrabServer(dpy);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT); check_gl(__LINE__);
	
	draw_background();

	for (i=0; i<MAX_CLIENTS; i++) {
		if (workspace == NULL || workspace[i].window == 0)
			break;
		Client client = workspace[i];
		width = (GLfloat) client.geom.width;
		height = (GLfloat) client.geom.height;
		//info("Drawing %d window => 0x%x, %g x %g\n", i, client.window, width, height);
		if(client.geom.depth == 32) {
            info("Using blending\n");
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // X windows are premultiplied
		} else
            glDisable(GL_BLEND);

        	
 	   	glEnable(client.target); check_gl(__LINE__); 
		glBindTexture(client.target, client.texture); check_gl(__LINE__); 
		glXBindTexImageProc(dpy, client.glxpixmap, GLX_FRONT_LEFT_EXT, NULL); 
		check_gl(__LINE__);
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
		glTexParameterf(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
		check_gl(__LINE__);
		glBegin(GL_QUADS);
			glColor3d(1.0, 1.0, 1.0);
			glTexCoord2f(left*width,  bottom*height);   glVertex3f(0.00f, 0.000f, 1.0f);
			glTexCoord2f(left*width,     top*height);	glVertex3f(0.00f, height, 1.0f);
			glTexCoord2f(right*width,    top*height);	glVertex3f(width, height, 1.0f);
			glTexCoord2f(right*width, bottom*height);	glVertex3f(width, 0.000f, 1.0f);
		glEnd();
		check_gl(__LINE__);
		glBindTexture(client.target, 0); check_gl(__LINE__); 
		glDisable(client.target); check_gl(__LINE__); 
		glXReleaseTexImageProc(dpy, client.glxpixmap, GLX_FRONT_LEFT_EXT); 
		check_gl(__LINE__);
	}
    glXWaitGL(); check_gl(__LINE__); 
	glXSwapBuffers(dpy, canvas); check_gl(__LINE__); 
	usleep(2000);  // 50Hz
	XEvent ev;
	ev.type = ClientMessage;
	ev.xclient.display = dpy;
	ev.xclient.window = root;
	ev.xclient.format = 8;
	XSendEvent(dpy, overlay, False, SubstructureNotifyMask, &ev);
	XFlush(dpy);
	//XUngrabServer(dpy);
}

void on_mapnotify(XEvent* e) {
	Window p = e->xmap.event;
	Window w = e->xmap.window;
	if (w == root)
		return;
	debug("[MapNotify] Mapped 0x%x to 0x%x\n", w, p);
	add(w);
}

void on_destroynotify(XEvent* e) {
	Window w = e->xdestroywindow.window;
	if (w == root || w == overlay)
		return;
	del(w);
}
void on_createnotify(XEvent* e) {
	Window w = e->xcreatewindow.window;
	if (w == root || w == overlay || w == canvas-1)
		return;
	for_windows(root, &print);
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

GLXWindow create_canvas(void) {
	XSetWindowAttributes sattr;
	GLXWindow c = 0;
	Window w = 0;
	
	GLXFBConfig fbc = choose_fbconfig();
	XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbc);
	check_gl(__LINE__);
	fbconfig_attrs(fbc, &width, &height, &top, &bottom);
    sattr.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    ratio = ((GLfloat) width) / ((GLfloat) height);
	w = XCreateWindow(dpy, overlay, 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual, CWColormap, &sattr);
	c = glXCreateWindow(dpy, fbc, w, NULL);
	check_gl(__LINE__);
	XFree(vi);
	XSelectInput(dpy, w, ExposureMask);

	context = glXCreateNewContext(dpy, fbc, GLX_RGBA_TYPE, NULL, direct);
	check_gl(__LINE__);
	glXMakeCurrent(dpy, c, context); check_gl(__LINE__);
    info("Canvas is %d x %d (ratio: %g)\n", width, height, ratio);
	return c;
}

void ignore_input(Window w) {
	XserverRegion region = XFixesCreateRegion(dpy, NULL, 0);
	XFixesSetWindowShapeRegion(dpy, w, ShapeBounding, 0, 0, 0);
	XFixesSetWindowShapeRegion(dpy, w, ShapeInput, 0, 0, region);
	XFixesDestroyRegion(dpy, region);
}

void setup_gl(void) {
	if (direct && !glXIsDirect(dpy, context))
		warn("Server doesn't support direct rendering! Falling back to indirect.\n");
	direct = glXIsDirect(dpy, context); check_gl(__LINE__);
	if (direct) info("Using direct rendering\n");
	else        info("Using indirect rendering\n");

	glEnable(GL_TEXTURE_RECTANGLE_ARB); check_gl(__LINE__);
	
	//glDrawBuffer(GL_BACK); check_gl(__LINE__);

	*(void**) (&glXQueryDrawableProc) = glXGetProcAddress((GLubyte*)"glXQueryDrawable");
	*(void**) (&glXBindTexImageProc) = glXGetProcAddress((GLubyte*)"glXBindTexImageEXT");
	*(void**) (&glXReleaseTexImageProc) = glXGetProcAddress((GLubyte*)"glXReleaseTexImageEXT");
	if (glXQueryDrawableProc == NULL)
        die(1, "No glXQueryDrawableProc\n");
	if (glXBindTexImageProc == NULL)
        die(1, "No glXBindTexImageProc\n");
	if (glXQueryDrawableProc == NULL)
        die(1, "No glXReleaseTexImageProc\n");
	XMapSubwindows(dpy, overlay);

    info("Defined viewport as %d, %d (ratio = %g)\n", width, height, ratio);
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, width, height, 0, -2, 2);
    glMatrixMode(GL_MODELVIEW);
    //glOrtho(-1.0f, +1.0f, -1.0f*ratio, +1.0f*ratio, 1e-2f, 1e+4f);
    //for_windows(root, &print);
	//for_windows(root, &add);
}

void on_signal(int sig) {
    debug("Caught signal %d.\n", sig);
    clean();
}

void setup_app(void) {
    struct sigaction action;
    sigset_t empty;
    sigemptyset(&empty);
    action.sa_handler = on_signal;
    action.sa_mask = empty;
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    debug("Configured signal handler\n");
}

void check_features(void) {
    int event_base, error_base, glxver;
    int major = 0, minor = 3;
    if(!XCompositeQueryExtension(dpy, &event_base, &error_base))
        die(1, "Composite extension not available\n");
    if(!XCompositeQueryVersion(dpy, &major, &minor))
        die(1, "Composite extension version >=0.3 is required\n");
    if(!XShapeQueryExtension(dpy, &event_base, &error_base))
        die(1, "Shape extension not available\n");
    if(!XDamageQueryExtension(dpy, &event_base, &error_base))
        die(1, "Damage extension not available\n");
    if(!glXQueryExtension(dpy, &event_base, &error_base))
        die(1, "GLX extension not available\n");
    glXQueryVersion(dpy, &major, &minor);
    glxver = MAKE_GL_VERSION(major, minor, 0);
    if(glxver < 0x010300) // GLX 1.3 required
        die(1, "GLX version >=1.3 is required\n");
    info("All features supported\n");
}

void setup_x(void) {
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	XSetErrorHandler(on_xerror);
	XSelectInput (dpy, root, SubstructureNotifyMask|KeyPressMask|VisibilityChangeMask|ExposureMask);
	XCompositeRedirectSubwindows(dpy, root, CompositeRedirectAutomatic);

	overlay = create_overlay();
	canvas = create_canvas();
	debug("Root 0x%x, Overlay 0x%x, Canvas 0x%x\n", root, overlay, canvas);

	stop = 0;
}

void start(void) {
	XEvent ev;
	XSync(dpy, 0);
	debug("[Start] Started\n");
	draw();
	while(!stop && !XNextEvent(dpy, &ev))
		on_event(&ev);
}

void clean(void) {
	debug("Quit.\n");
    stop = 1;
    if (dpy)
        XCloseDisplay(dpy);
}

void for_windows(Window w, void (*f)(Window w)) {
	unsigned int i, nc;
	Window r, p, *c=NULL;
	if(XQueryTree(dpy, w, &r, &p, &c, &nc)==0)
		return;
	f(w);
	for (i=0; i<nc; i++)
		for_windows(c[i], f);
	XFree(c);
}

int main(int argc, char** argv) {
    dpy = XOpenDisplay(NULL);
	if(!dpy)
		die(ERR_CANNOT_OPEN_DISPLAY, "Cannot open display!\n");
    setup_app();
    check_features();
    setup_x();
	setup_gl();
	start();
	clean();
    return 0;
}

void print(Window w) {
	unsigned int nc;
	Window p, r, *c = NULL;
	XQueryTree(dpy, w, &r, &p, &c, &nc);
	debug("0x%x (0x%x)\n", (unsigned int) w, (unsigned int) p);
}

