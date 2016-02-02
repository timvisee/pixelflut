#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glew.h>
#include <sys/time.h>

static int c = 0;
GLuint pboIds[2];                   // IDs of PBO
GLuint textureId;                   // ID of texture
GLubyte* imageData;             // pointer to texture buffer

int    IMAGE_WIDTH     = 512;
int    IMAGE_HEIGHT    = 512;
int    CHANNEL_COUNT   = 3;
GLenum PIXEL_FORMAT    = GL_RGB;
int    FPS = 100;
int    DATA_SIZE;

int screenWidth = 800;
int screenHeight = 600;

static void draw(void);
static void cleanup(void);

void reshape(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screenWidth, 0, screenHeight, -1, 1);
    glViewport(0, 0, (GLsizei)screenWidth, (GLsizei)screenHeight);

    // switch to modelview matrix in order to set scene
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
};

static void cleanup(void) {
    free(imageData);
    glDeleteTextures(1, &textureId);
    glDeleteBuffers(2, pboIds);
}

void tick(int millisec) {
    glutTimerFunc(millisec, tick, millisec);
    glutPostRedisplay();
}

int main(int argc, char *argv[]) {
    DATA_SIZE = IMAGE_WIDTH * IMAGE_HEIGHT * CHANNEL_COUNT;
    imageData = malloc(DATA_SIZE);

    int x, y;
    for(x=0; x<IMAGE_WIDTH; x+=4) {
      for(y=0; y<IMAGE_HEIGHT; y+=4) {
        GLubyte *ptr = imageData+(y*IMAGE_WIDTH+x)*CHANNEL_COUNT;
        float px = (float) x / IMAGE_WIDTH;
        float py = (float) y / IMAGE_HEIGHT;
        *(ptr+0) = px*255;
        *(ptr+1) = py*255;
        *(ptr+2) = 0;
      }
    }

    imageData = malloc(DATA_SIZE);


    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_ALPHA); // display mode
    glutInitWindowSize(screenWidth, screenHeight);         // window size
    glutInitWindowPosition(100, 100);           // window location

    // finally, create a window with openGL context
    // Window will not displayed until glutMainLoop() is called
    // it returns a unique ID
    int handle = glutCreateWindow(argv[0]);     // param is the title of window
    // register GLUT callback functions
    glutDisplayFunc(draw);
    glutTimerFunc(1000/FPS, tick, 1000/FPS);             // redraw only every given millisec
    //glutIdleFunc(idleCB);                       // redraw only every given millisec
    glutReshapeFunc(reshape);
    //glutKeyboardFunc(keyboardCB);
    //glutMouseFunc(mouseCB);
    //glutMotionFunc(mouseMotionCB);

    glShadeModel(GL_FLAT);                      // shading mathod: GL_SMOOTH or GL_FLAT
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    // track material ambient and diffuse from surface color, call it before glEnable(GL_COLOR_MATERIAL)
    //glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    //glEnable(GL_COLOR_MATERIAL);

    //glClearColor(0, 0, 0, 0);                   // background color
    //glClearStencil(0);                          // clear stencil buffer
    //glClearDepth(1.0f);                         // 0 is near, 1 is far
    //glDepthFunc(GL_LEQUAL);

    glewInit();
    // init texture object
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, IMAGE_WIDTH, IMAGE_HEIGHT, 0, PIXEL_FORMAT, GL_UNSIGNED_BYTE, (GLvoid*)imageData);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // create 2 pixel buffer objects, you need to delete them when program exits.
    // glBufferData with NULL pointer reserves only memory space.
    glGenBuffers(2, pboIds);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[0]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, DATA_SIZE, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[1]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, DATA_SIZE, 0, GL_STREAM_DRAW); 
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    
    printf("done %d %d\n", imageData[0], imageData[100]);    
    atexit(cleanup);
    
    reshape(screenWidth, screenHeight);
    draw();
    draw();
    glutMainLoop(); /* Start GLUT event-processing loop */
    return 0;
}






static void drawPixel() {
    static int index = 0;
    int nextIndex = 0; // pbo index used for next frame
    
    // In dual PBO mode, increment current index first then get the next index
    index = (index + 1) % 2;
    nextIndex = (index + 1) % 2;

    // bind the texture and PBO
    glBindTexture(GL_TEXTURE_2D, textureId);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[index]);

    // copy pixels from PBO to texture object
    // Use offset instead of ponter.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT, PIXEL_FORMAT, GL_UNSIGNED_BYTE, 0);

    // bind PBO to update pixel values
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIds[nextIndex]);

    // map the buffer object into client's memory
    // Note that glMapBuffer() causes sync issue.
    // If GPU is working with this buffer, glMapBuffer() will wait(stall)
    // for GPU to finish its job. To avoid waiting (stall), you can call
    // first glBufferData() with NULL pointer before glMapBuffer().
    // If you do that, the previous data in PBO will be discarded and
    // glMapBuffer() returns a new allocated pointer immediately
    // even if GPU is still working with the previous data.
    glBufferData(GL_PIXEL_UNPACK_BUFFER, DATA_SIZE, NULL, GL_STREAM_DRAW);
    
    GLubyte * ptr = (GLubyte*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memcpy(ptr, imageData, DATA_SIZE);
    *(imageData+(c++)*CHANNEL_COUNT%DATA_SIZE) = 0xff;
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    // it is good idea to release PBOs with ID 0 after use.
    // Once bound with 0, all pixel operations behave normal ways.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);



    // clear buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // save the initial ModelView matrix before modifying ModelView matrix
    glPushMatrix();

    int msize;
    if(screenWidth > IMAGE_WIDTH || screenHeight > IMAGE_HEIGHT)
        msize = (screenWidth > screenHeight) ? screenWidth : screenHeight;
    else
        msize = IMAGE_WIDTH;

    glTranslatef(0,screenHeight-msize,0);
    
    // draw a point with texture
    glBindTexture(GL_TEXTURE_2D, textureId);
    //glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
        //glNormal3f(0, 0, 1);
        glTexCoord2f(0.0f, 1.0f);   glVertex3f( 0.0f, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);   glVertex3f( msize, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);   glVertex3f( msize, msize, 0.0f);
        glTexCoord2f(0.0f, 0.0f);   glVertex3f( 0.0f, msize, 0.0f);
    glEnd();

    // unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    glPopMatrix();
}

static void
draw(void) {

    clock_t t = clock();
    
    drawPixel();
    
    t = clock() - t;
    printf ("It took me %d clicks.\n",t);
    glutSwapBuffers();
}



