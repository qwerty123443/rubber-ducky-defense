#include "duckydefense.h"

int main(int argc, char *argv[])
{
    // TODO: verity UID == 0
    int inotifyFd, wd;
    char buf[BUF_LEN];
    ssize_t numRead;
    char *p;
    struct inotify_event *event;

    // register interrupt handlers
    // the program can still be killed with SIGKILL
    // for (int i = 0; i < 32; i++)
    //     signal(i, dummy_sig_handler);

    inotifyFd = inotify_init(); // Create inotify instance
    if (inotifyFd == -1)
        die("inotify_init");

    wd = inotify_add_watch(inotifyFd, "/dev/input/by-id/", IN_CREATE);

    if (wd == -1)
        die("inotify_add_watch(): ");

    while (1)
    {
        // Read events forever
        numRead = read(inotifyFd, buf, BUF_LEN);
        if (numRead == 0)
            printf("read() from inotify fd returned 0!");

        if (numRead == -1)
            die("read");

        // Process all of the events in buffer returned by read()
        for (p = buf; p < buf + numRead;)
        {
            event = (struct inotify_event *)p;

            pthread_t thread;
            if (pthread_create(&thread, NULL, keyboard_thread, (void *)event->name) != 0)
                perror("pthread_create");
            p += sizeof(struct inotify_event) + event->len;
        }
    }
    return 0;
}

int do_display_thing(int fdi, int fdo)
{
    long dt;
    long started;
    XWindow xw;
    struct nk_context *ctx;

    struct input_event ev;

    int typed_chars = 0;
    char verification_code[VERIFICATION_LENGTH + 1] = {};
    char typed_code[VERIFICATION_LENGTH + 1] = {};

    int passed = 0;

    srandom(time(NULL));

    // generate the string to type
    int modulus = pow(10, VERIFICATION_LENGTH);
    char format_string[2 + VERIFICATION_LENGTH + 1 + 1]; /* 2 bytes for "%0",
                                                            n bytes for the digit
                                                            1 byte for "i"
                                                            1 byte for null terminator */
    sprintf(format_string, "%%0%di", VERIFICATION_LENGTH);
    sprintf(verification_code, format_string, random() % modulus);
    verification_code[VERIFICATION_LENGTH] = '\0';
    // printf("type %s\n", verification_code);

    // X11
    memset(&xw, 0, sizeof xw);
    xw.dpy = XOpenDisplay(NULL);
    if (!xw.dpy)
        die("Could not open a display; perhaps $DISPLAY is not set?");
    xw.root = DefaultRootWindow(xw.dpy);
    xw.screen = XDefaultScreen(xw.dpy);
    xw.vis = XDefaultVisual(xw.dpy, xw.screen);
    xw.cmap = XCreateColormap(xw.dpy, xw.root, xw.vis, AllocNone);

    xw.swa.colormap = xw.cmap;
    xw.swa.event_mask = 0;
    // ExposureMask | KeyPressMask | KeyReleaseMask |
    // ButtonPress | ButtonReleaseMask | ButtonMotionMask |
    // Button1MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask |
    // PointerMotionMask | KeymapStateMask;

    xw.win = XCreateWindow(xw.dpy, xw.root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                           XDefaultDepth(xw.dpy, xw.screen), InputOutput,
                           xw.vis, CWEventMask | CWColormap, &xw.swa);

    XStoreName(xw.dpy, xw.win, "Type this code");
    XMapWindow(xw.dpy, xw.win);
    xw.wm_delete_window = XInternAtom(xw.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(xw.dpy, xw.win, &xw.wm_delete_window, 1);
    XGetWindowAttributes(xw.dpy, xw.win, &xw.attr);
    xw.width = (unsigned int)xw.attr.width;
    xw.height = (unsigned int)xw.attr.height;

    // GUI
    xw.font = nk_xfont_create(xw.dpy, "fixed");
    ctx = nk_xlib_init(xw.font, xw.dpy, xw.screen, xw.win, xw.width, xw.height);

    set_style(ctx, THEME_DARK);

    // Set file reads to non-blocking so that we can run the UI loop and check the keyboard for input at the same time
    int flags = fcntl(fdi, F_GETFL, 0);
    fcntl(fdi, F_SETFL, flags | O_NONBLOCK);

    while (typed_chars < 4)
    {

        // Input
        started = timestamp();

        if (read(fdi, &ev, sizeof(struct input_event)) > 0 && ev.type == EV_KEY && ev.value == 0)
        {
            if (ev.code >= KEY_1 && ev.code <= KEY_0)
            {
                typed_code[typed_chars] = (ev.code - KEY_1 + 1) % 10 + '0';
                typed_chars++;
                if (typed_chars == 4)
                {
                    typed_code[typed_chars] = '\0';
                    if (strcmp(typed_code, verification_code) == 0)
                    {
                        passed = 1;
                    }
                    break;
                }
            }
            else if (ev.code == KEY_BACKSPACE)
            {
                typed_chars--;
                typed_code[typed_chars] = '\0';
            }
        }

        if (errno && errno != EAGAIN)
            break;

        // GUI
        if (nk_begin(ctx, "Type this:", nk_rect(10, 10, 200, 200), NK_WINDOW_TITLE))
        {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, verification_code, NK_TEXT_ALIGN_LEFT);
            nk_label(ctx, typed_code, NK_TEXT_ALIGN_LEFT);
        }
        nk_end(ctx);

        // Draw
        XClearWindow(xw.dpy, xw.win);
        nk_xlib_render(xw.win, nk_rgb(30, 30, 30));
        XFlush(xw.dpy);

        // Timing
        dt = timestamp() - started;
        if (dt < DTIME)
            sleep_for(DTIME - dt);
    }

    nk_xfont_del(xw.dpy, xw.font);
    nk_xlib_shutdown();
    XUnmapWindow(xw.dpy, xw.win);
    XFreeColormap(xw.dpy, xw.cmap);
    XDestroyWindow(xw.dpy, xw.win);
    XCloseDisplay(xw.dpy);
    // Set file back to blocking mode
    fcntl(fdi, F_SETFL, flags);
    return passed;
}

static void *keyboard_thread(void *arg)
{
    char *filename = (char *)arg;
    int fdo, fdi;
    struct uinput_user_dev uidev;
    int fname_len = strlen(filename);
    char path_to_open[fname_len + strlen("/dev/input/by-id/") + 1];
    char match_thing[] = "-kbd"; // compare the end of the path to "kbd". It's a hacky way to detect keyboards

    if (pthread_detach(pthread_self()) != 0)
        vdie("pthread_detach");

    memset(path_to_open, 0, sizeof path_to_open);
    strcat(path_to_open, "/dev/input/by-id/");
    strncat(path_to_open, filename, fname_len - strlen(".tmp-c13:85"));
    // if (strstr(path_to_open, match_thing) == 0)
    if (strcmp(path_to_open + strlen(path_to_open) - strlen(match_thing), match_thing) != 0)
        return NULL;

    // printf("path to open: %s\n", path_to_open);

    // Grab KBD output and system input
    fdi = open(path_to_open, O_RDONLY);
    fdo = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    // Check the keyboards have been opened
    if (fdi < 0)
        vdie("error: open fdi");
    if (fdo < 0)
        vdie("error: open fdo");

    // UNIX magic, apparently
    if (ioctl(fdi, EVIOCGRAB, 1) < 0)
        vdie("error: ioctl");

    if (ioctl(fdo, UI_SET_EVBIT, EV_SYN) < 0)
        vdie("error: ioctl");
    if (ioctl(fdo, UI_SET_EVBIT, EV_KEY) < 0)
        vdie("error: ioctl");
    if (ioctl(fdo, UI_SET_EVBIT, EV_MSC) < 0)
        vdie("error: ioctl");

    for (int i = 0; i < KEY_MAX; ++i)
        if (ioctl(fdo, UI_SET_KEYBIT, i) < 0)
            vdie("error: ioctl");

    // USB magic
    memset(&uidev, 0, sizeof(uidev));
    sprintf(uidev.name, "passthrough-input");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x13;
    uidev.id.product = 0x37;
    uidev.id.version = 0x1337;

    if (write(fdo, &uidev, sizeof(uidev)) < 0)
        vdie("error: write");
    if (ioctl(fdo, UI_DEV_CREATE) < 0)
        vdie("error: ioctl");

    printf("New keyboard detected\n");
    // int res = check_pin(fdi, fdo);
    // GUI stuff start
    int res = do_display_thing(fdi, fdo);
    // GUI stuff end

    // Main MITM loop.
    if (res)
    {
        printf("Unlocked\n");
        pass_input(fdi, fdo);
    }
    else
    {
        // nullify all keyboard output
        printf("Check failed.\n");
        // close output file but keep reading the input keyboard as to consume all keystrokes
        if (ioctl(fdo, UI_DEV_DESTROY) < 0)
            vdie("error: ioctl");
        close(fdo);
        struct input_event ev;
        while (1)
        {
            if (read(fdi, &ev, sizeof(struct input_event)) < 0)
                vdie("error: read");
        }
    }

    if (ioctl(fdo, UI_DEV_DESTROY) < 0)
        vdie("error: ioctl");

    close(fdi);
    close(fdo);
    return NULL;
}

static void pass_input(int fdi, int fdo)
{

    struct input_event ev;
    int res;

    while (1)
    {

        res = read(fdi, &ev, sizeof(struct input_event));
        if (res < 0)
            break;

        res = write(fdo, &ev, sizeof(struct input_event));
        if (res < 0)
            break;
    }
}

void dummy_sig_handler(int _unused)
{
    return;
}
