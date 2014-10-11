#include "mr_xwin.h"
#ifdef _MR_CPPLIB_XWIN_H_INCLUDE
#include "mr_strutils.h"
#include "mr_x.h"

#define SAVE_PATH "/Users/mikerobe/Downloads/img."
#define MASK ButtonPressMask | KeyPressMask | ResizeRedirectMask | ExposureMask | StructureNotifyMask

namespace {
    static bool calledXlibThreads = false;
    static mrutils::mutex_t xlibThreadMutex = mrutils::mutexCreate();
}

mrutils::XWin::XWin()
: init_(false), hidden(false), createdWin(false)
 ,startThread(true), startedThread(false)
 ,persist(false), redrawNextExpose(false)
 ,y(0), x(0), width(0), height(0)
 ,saveImg(NULL)
 ,ximg(NULL), ximgData(NULL)
 ,winMutex(mrutils::mutexCreate())
 ,imgResizes(0)
{
    if (!calledXlibThreads) {
        mrutils::mutexAcquire(xlibThreadMutex);
            if (!calledXlibThreads) {
                if (0 == XInitThreads()) {
                    fputs("Unable to init multithreading for xlib...\n",stderr);
                    fflush(stderr);
                } else calledXlibThreads = true;
            } 
        mrutils::mutexRelease(xlibThreadMutex);
    }
}

mrutils::XWin::~XWin() {
    if (createdWin) XDestroyWindow(dis,win);
    if (ximg != NULL) delete ximg;
    if (ximgData != NULL) delete ximgData;
    mrutils::mutexDelete(winMutex);
}


void mrutils::XWin::init(unsigned width, unsigned height, int y, int x) {
    if (!calledXlibThreads) return;
    if (width == 0 || height == 0) return;

    this->y = y; this->x = x;
    this->width = width; this->height = height;

    dis = XOpenDisplay(NULL);
    screen = DefaultScreen(dis);

    XSetWindowAttributes winAttrib;    
            
    unsigned windowMask = CWBackPixel | CWBorderPixel;    
    winAttrib.border_pixel = BlackPixel (dis, screen);
    winAttrib.background_pixel = BlackPixel (dis, screen);
    winAttrib.override_redirect = 0;

	win = XCreateWindow (dis, DefaultRootWindow (dis), 
		0, 0,
		width, height,
		0, DefaultDepth (dis, screen),
		InputOutput, CopyFromParent,
		windowMask, &winAttrib
	);

    /*
    win = XCreateSimpleWindow(dis
       ,DefaultRootWindow(dis)
       ,0, 0 // x & y are ignored...
       ,width, height
       ,0 // border width
       ,0 // border
       ,0 // background
       );
    */

    graphicsContext = XCreateGC (dis, win, 0, NULL);
    createdWin = init_ = true;
}

void mrutils::XWin::setName(const char * name) {
    mrutils::mutexAcquire(winMutex);
        if (init_) XStoreName(dis,win,name);
    mrutils::mutexRelease(winMutex);
}

void mrutils::XWin::threadedGUI(void*) {
    waitQuit();

    // close the window
    mrutils::mutexAcquire(winMutex);
        if (persist) hidden = true;
        else init_ = false;

        startedThread = false;
        XUnmapWindow(dis,win);
        XFlush(dis); 
    mrutils::mutexRelease(winMutex);
}

void mrutils::XWin::waitQuit() {
    if (!init_) return;

	XSelectInput(dis, win, MASK);

    for (bool done = false;!done;) {
        XEvent event; XNextEvent (dis, &event);

        switch  (event.type) {
            case Expose:
                mrutils::mutexAcquire(winMutex);
                    if (event.xexpose.window == win
                        && saveImg != NULL
                        && redrawNextExpose
                        ) {
                        redrawNextExpose = false;
                        showImage_(*saveImg,false);
                    }
                mrutils::mutexRelease(winMutex);
                break;
            case ButtonPress:
                mrutils::mutexAcquire(winMutex);
                    if (event.xexpose.window == win
                        && saveImg != NULL
                        ) showImage_(*saveImg,false);
                mrutils::mutexRelease(winMutex);
                break;
            case ResizeRequest:
                mrutils::mutexAcquire(winMutex);
                    if (event.xresizerequest.window == win) {
                        if (imgResizes == 0) {
                            if (saveImg != NULL) {
                                if (event.xresizerequest.height < (int)height
                                    || event.xresizerequest.width < (int)width) {
                                    height = event.xresizerequest.height;
                                    width = event.xresizerequest.height;
                                    showImage_(*saveImg,false);
                                }
                            }
                        } else --imgResizes;
                    }
                mrutils::mutexRelease(winMutex);
                break;
            case UnmapNotify: 
                mrutils::mutexAcquire(winMutex);
                    if (event.xunmap.window == win) {
                        redrawNextExpose = true;
                        if (!init_) done = true;
                    }
                mrutils::mutexRelease(winMutex);
                break;
            case KeyPress:
                if (event.xkey.window == win) {
                    switch (int const key = XLookupKeysym (&event.xkey, 0)) {
                        case XK_semicolon:
							this->exitKey = ';';
							done = true;
							break;
                        case XK_q:
							this->exitKey = 'q';
							done = true;
							break;
						case XK_Return:
							this->exitKey = '\n';
							done = true;
							break;
                        case XK_s:
                            mrutils::mutexAcquire(winMutex);
                                if (saveImg != NULL) {
                                    mrutils::stringstream ss;
                                    ss << SAVE_PATH << saveImg->formatExtension[saveImg->format];
                                    if (saveImg->saveOriginal(ss.str().c_str())) {
                                        XStoreName(dis,win,ss.str().c_str());
                                    }
                                }
                            mrutils::mutexRelease(winMutex);
                            break;
                        default:
							break;
                    }
                }
                break;

            default: break;
        }
    }
}

int mrutils::XWin::getKey() {
	XSelectInput(dis, win, MASK);

    for (bool done = false;!done;) {
        XEvent event; XNextEvent (dis, &event);

        switch  (event.type) {
            case KeyPress:
                return XLookupKeysym (&event.xkey, 0);
            default: 
                break;
        }
    }

    return 0;
}

void mrutils::XWin::resize(int width, int height) {
    mrutils::mutexAcquire(winMutex);
        if (init_) {
            XResizeWindow(dis,win,width,height);
            XFlush(dis); 
            this->width = width; this->height = height;
        }
    mrutils::mutexRelease(winMutex);
}

void mrutils::XWin::show_(bool getLock) {
    if (getLock) mrutils::mutexAcquire(winMutex);

        if (!init_) {
            if (getLock) mrutils::mutexRelease(winMutex);
            return;
        }

        hidden = false;

        XSelectInput(dis, win, MASK);

        XMapRaised (dis,win);
        if (y != 0 || x != 0) XMoveWindow(dis,win,x,y);
        XFlush(dis); XSync(dis,false);

        // now wait for window expose event
        for (;;) {
            XEvent event; XNextEvent (dis, &event);
            if (event.type == Expose && event.xexpose.window  == win)
                break;
        }

        if (startThread && !startedThread) {
            mrutils::threadRun(fastdelegate::MakeDelegate(this
                ,&XWin::threadedGUI));
            startedThread = true;
        }

    if (getLock) mrutils::mutexRelease(winMutex);
}

void mrutils::XWin::hide() {
    mrutils::mutexAcquire(winMutex);
        if (init_) {
            XUnmapWindow(dis,win);
            XFlush(dis); 
            hidden = true;
        }
    mrutils::mutexRelease(winMutex);
}

bool mrutils::XWin::showImage_(mrutils::ImageDecode& img, bool getLock) {
    if (getLock) mrutils::mutexAcquire(winMutex);
        if (!init_ || (hidden && !getLock) || img.width == 0 || img.height == 0) {
            if (getLock) mrutils::mutexRelease(winMutex);
            return false;
        }

        if (hidden) show_(false);

        if (img.width != width || img.height != height) {
            width = img.width; height = img.height;
            XResizeWindow(dis,win,width,height);
            ++imgResizes;
        }

        // if getlock then assumed a user call
        if (getLock) {
            if (ximg != NULL) delete ximg;
            ximg = mrutils::x11::create_image_from_buffer(dis, screen, img.rawImg, img.width, img.height, ximgData);
        }

        XPutImage(dis, win, graphicsContext, ximg, 0, 0, 0, 0, img.width, img.height);
        XFlush(dis); 
    if (getLock) mrutils::mutexRelease(winMutex);

    return true;
}

#endif
