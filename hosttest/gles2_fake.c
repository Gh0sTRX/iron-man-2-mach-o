/* hosttest: funciones GLES2 de vitaGL ausentes en el GLES 1.1 de Mesa (Feanor no las llama). */
#include <stdio.h>
#include "vitaGL.h"
#define STUB(decl) decl { fprintf(stderr, "[HOSTTEST] %s sin implementar\n", __func__); }
STUB(void glBlendEquation(GLenum m))
STUB(void glBlendFuncSeparate(GLenum a, GLenum b, GLenum c, GLenum d))
STUB(void glVertexAttribPointer(GLuint i, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void *p))
STUB(void glEnableVertexAttribArray(GLuint i))
STUB(void glDisableVertexAttribArray(GLuint i))
STUB(void glVertexAttrib4f(GLuint i, GLfloat x, GLfloat y, GLfloat z, GLfloat w))
STUB(void glDeleteProgram(GLuint p))
STUB(void glGenerateMipmap(GLenum t))
