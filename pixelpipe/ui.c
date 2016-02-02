#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GL/glew.h>

#include "ui.h"

static SDL_Window* window;
static const int FPS = 30;
static int uiLoopAlive = 0;

static struct uiLayer* layers[2];

static void ui_cleanup();
static struct uiLayer * ui_layer_create(int width, int height, char alpha);
static void ui_layer_draw(struct uiLayer *layer, int width, int height);
static void ui_layer_free(struct uiLayer *layer);
void ui_resize(int width, int height);
static inline void setPx(struct uiLayer *layer, int x, int y, int c);

int ui_init() {
    SDL_SetMainReady();
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS);
    SDL_DisableScreenSaver();
    atexit(ui_cleanup);

    window = SDL_CreateWindow(
    	"PIXELNUKE",
    	SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    	640, 480,
    	SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);//|SDL_WINDOW_BORDERLESS);

    SDL_GL_CreateContext(window);

    //glShadeModel(GL_FLAT);            // shading mathod: GL_SMOOTH or GL_FLAT
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
    //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    //glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    //glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glewInit();
    
    layers[0] = ui_layer_create(1024, 1024, 0);
    layers[1] = ui_layer_create(1024, 1024, 1);
    return 0;
}

static void ui_cleanup() {
    ui_layer_free(layers[0]);
    ui_layer_free(layers[1]);
    SDL_Quit();
}

static void ui_draw() {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glViewport(0, 0, (GLsizei) w, (GLsizei) h);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ui_layer_draw(layers[0], w, h);
    ui_layer_draw(layers[1], 128, 128);
    
    SDL_GL_SwapWindow(window);
}

void ui_loop() {
	SDL_Event event;
	uiLoopAlive = 1;
	unsigned int currentTime = SDL_GetTicks();
    while (uiLoopAlive) {
    	while(SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_WINDOWEVENT_RESIZED:
                ui_resize(event.window.data1, event.window.data2);
            break;
            case SDL_KEYDOWN:
                printf("Oh! Key press\n");
                break;
            case SDL_MOUSEMOTION:
                break;
            case SDL_QUIT:
	            uiLoopAlive = 0;
            default:
                printf("I don't know what this event is!\n");
            }
        }

        ui_draw();

        unsigned int timePassed = SDL_GetTicks() - currentTime;
        int delay = 1000/FPS - timePassed;
        SDL_Delay(delay > 0 ? delay : 10);
	    currentTime += timePassed;
    }
}

static char hasAlpha(struct uiLayer *layer) {
    return layer->texFormat == GL_RGBA8;
}

static size_t getTexSize(struct uiLayer *layer) {
    return layer->texSize * layer->texSize * (hasAlpha(layer) ? 4 : 3);
}

static GLubyte* getTexOffset(struct uiLayer *layer, int x, int y) {
    if(x < 0 || x > layer->texSize || y < 0 || y > layer->texSize)
        return NULL;
    
    return layer->texData + (x * layer->texSize + y);
}

static struct uiLayer * ui_layer_create(int width, int height, char alpha) {
    struct uiLayer *layer = calloc(1, sizeof(struct uiLayer));
    layer->width = width;
    layer->height = height;

    layer->texSize = 64;
    while(layer->texSize < width || layer->texSize < height)
        layer->texSize = layer->texSize * 2;

    layer->texFormat = alpha ? GL_RGBA8 : GL_RGB;
    size_t data_size = getTexSize(layer);

    layer->texData = calloc(1, data_size);

    size_t i = data_size;
    while(i--) {
       layer->texData[i] = i%255;
    }
	
    // Create texture object
    glGenTextures(1, &layer->texId);
    glBindTexture(GL_TEXTURE_2D, layer->texId);
    glTexImage2D(GL_TEXTURE_2D, 0, layer->texFormat, layer->texSize, layer->texSize,
                 0, layer->texFormat, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Create two PBOs
    glGenBuffers(1, &layer->texPBO1);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, layer->texPBO1);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, data_size, NULL, GL_STREAM_DRAW);
    glGenBuffers(1, &layer->texPBO2);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, layer->texPBO2);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, data_size, NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    
    return layer;
}


static void ui_layer_free(struct uiLayer *layer) {
    glDeleteTextures(1, &layer->texId);
    glDeleteBuffers(1, &layer->texPBO1);
    glDeleteBuffers(1, &layer->texPBO2);
    free(layer);
}


static void ui_layer_draw(struct uiLayer *layer, int winWidth, int winHeight) {
    GLuint pboNext  = layer->texPBO1;
    GLuint pboIndex = layer->texPBO2;
    layer->texPBO1 = pboIndex;
    layer->texPBO2 = pboNext;

    // Switch PBOs on each call. One is updated, one is drawn.

    // Update texture from first PBO
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIndex);
    glBindTexture(GL_TEXTURE_2D, layer->texId);
    glTexImage2D(GL_TEXTURE_2D, 0, layer->texFormat, layer->texSize, layer->texSize, 0, layer->texFormat, GL_UNSIGNED_BYTE, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
    // Update second PBO with new pixel data
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboNext);
    GLubyte *ptr = (GLubyte*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(ptr, layer->texData, getTexSize(layer));
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    // Release PBOs
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    //// Actually draw stuff. The texture should be updated in the meantime.

    int quadsize;
    if(winWidth > layer->texSize || winHeight > layer->texSize)
        quadsize = (winWidth > winHeight) ? winWidth : winHeight;
    else
        quadsize = layer->texSize;
        
    if(hasAlpha(layer)) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    } else{
        glDisable(GL_BLEND);
    }

    glPushMatrix();
    glTranslatef(0, -winHeight, 0); // Align quad top left instead of bottom.
    glBindTexture(GL_TEXTURE_2D, layer->texId);
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);   glVertex3f(0.0f, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);   glVertex3f(quadsize, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);   glVertex3f(quadsize, quadsize, 0.0f);
        glTexCoord2f(0.0f, 0.0f);   glVertex3f(0.0f, quadsize, 0.0f);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glPopMatrix();
}



static inline void setPx(struct uiLayer *layer, int x, int y, int c) {
	if(x > layer->texSize || y > layer->texSize)
		return;

    GLubyte* ptr = getTexOffset(layer, x, y);
    GLubyte r = (c & 0xff000000) >> 24;
    GLubyte g = (c & 0x00ff0000) >> 16;
    GLubyte b = (c & 0x0000ff00) >> 8;
    GLubyte  a = (c & 0x000000ff) >> 0;

    if(a == 0)
    	return;
    if (a < 0xff) {
    	GLuint na = 0xff-a;
    	r = (a * r + na * (ptr[0])) / 0xff;
    	g = (a * g + na * (ptr[1])) / 0xff;
    	b = (a * b + na * (ptr[2])) / 0xff;
    }
    ptr[0] = r;
    ptr[1] = g;
    ptr[2] = b;
}


void ui_resize(int width, int height) {
    layers[0]->width = width;
    layers[0]->height = height;
    layers[1]->width = width;
    layers[1]->height = height;
};


