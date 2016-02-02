#include <GL/gl.h>

struct uiLayer {
	GLuint width, height;
	GLuint texSize;
	GLenum texFormat;
	GLuint texId;
	GLuint texPBO1;
	GLuint texPBO2;
	GLubyte *texData;
};

int ui_init();
void ui_loop();
