
#include "core_glutil.h"

#include <luabind/luabind.hpp>
#include <luabind/operator.hpp>
#include <luabind/discard_result_policy.hpp>

#include "GLee.h"

#include "debug.h"

#include <vector>

#include <boost/noncopyable.hpp>

using namespace std;


#ifndef IPHONE
static vector<int> to_delete_lists;
static vector<int> to_delete_shaders;
static vector<int> to_delete_programs;
static vector<GLuint> to_delete_framebuffers;
static vector<GLuint> to_delete_renderbuffers;
static vector<GLuint> to_delete_textures;
static vector<GLuint> to_delete_buffers;

class GlListID : boost::noncopyable {
  int id;
  
public:
  GlListID() {
    id = glGenLists(1);
  }
  ~GlListID() {
    to_delete_lists.push_back(id);
  }
  
  int get() const {
    return id;
  }
};

class GlShader : boost::noncopyable {
  int id;
  
public:
  GlShader(string typ) {
    int typi;
    if(typ == "VERTEX_SHADER") typi = GL_VERTEX_SHADER; else
    if(typ == "FRAGMENT_SHADER") typi = GL_FRAGMENT_SHADER; else
      CHECK(0);
    id = glCreateShader(typi);
  }
  ~GlShader() {
    to_delete_shaders.push_back(id);
  }
  
  int get() const {
    return id;
  }
};

class GlProgram : boost::noncopyable {
  int id;
  
public:
  GlProgram() {
    id = glCreateProgram();
  }
  ~GlProgram() {
    to_delete_programs.push_back(id);
  }
  
  int get() const {
    return id;
  }
};

class GlFramebuffer : boost::noncopyable {
  GLuint id;
  
public:
  GlFramebuffer() {
    glGenFramebuffers(1, &id);
  }
  ~GlFramebuffer() {
    to_delete_framebuffers.push_back(id);
  }
  
  int get() const {
    return id;
  }
};

class GlRenderbuffer : boost::noncopyable {
  GLuint id;
  
public:
  GlRenderbuffer() {
    glGenRenderbuffers(1, &id);
  }
  ~GlRenderbuffer() {
    to_delete_renderbuffers.push_back(id);
  }
  
  int get() const {
    return id;
  }
};

class GlTexture : boost::noncopyable {
  GLuint id;
  
public:
  GlTexture() {
    glGenTextures(1, &id);
  }
  ~GlTexture() {
    to_delete_textures.push_back(id);
  }
  
  int get() const {
    return id;
  }
};

class GlBuffer : boost::noncopyable {
  GLuint id;
  
public:
  GlBuffer() {
    glGenBuffers(1, &id);
  }
  ~GlBuffer() {
    to_delete_buffers.push_back(id);
  }
  
  int get() const {
    return id;
  }
};
#endif

void glorp_glutil_init(lua_State *L) {
  {
    using namespace luabind;
    
    module(L)
    [
      #ifndef IPHONE
      class_<GlListID>("GlListID")
        .def(constructor<>())
        .def("get", &GlListID::get),
      class_<GlShader>("GlShader")
        .def(constructor<const std::string &>())
        .def("get", &GlShader::get),
      class_<GlProgram>("GlProgram")
        .def(constructor<>())
        .def("get", &GlProgram::get),
      class_<GlFramebuffer>("GlFramebuffer")
        .def(constructor<>())
        .def("get", &GlFramebuffer::get),
      class_<GlRenderbuffer>("GlRenderbuffer")
        .def(constructor<>())
        .def("get", &GlRenderbuffer::get),
      class_<GlTexture>("GlTexture")
        .def(constructor<>())
        .def("get", &GlTexture::get),
      class_<GlBuffer>("GlBuffer")
        .def(constructor<>())
        .def("get", &GlBuffer::get)
      #endif
    ];
  }
}
void glorp_glutil_tick() {
  #ifndef IPHONE      
  // siiiigh
  for(int i = 0; i < to_delete_lists.size(); i++) glDeleteLists(to_delete_lists[i], 1);
  for(int i = 0; i < to_delete_shaders.size(); i++) glDeleteShader(to_delete_shaders[i]);
  for(int i = 0; i < to_delete_programs.size(); i++) glDeleteProgram(to_delete_programs[i]);
  for(int i = 0; i < to_delete_framebuffers.size(); i++) glDeleteFramebuffers(1, &to_delete_framebuffers[i]);
  for(int i = 0; i < to_delete_renderbuffers.size(); i++) glDeleteRenderbuffers(1, &to_delete_renderbuffers[i]);
  for(int i = 0; i < to_delete_textures.size(); i++) glDeleteTextures(1, &to_delete_textures[i]);
  for(int i = 0; i < to_delete_buffers.size(); i++) glDeleteBuffers(1, &to_delete_buffers[i]);
  to_delete_lists.clear();
  to_delete_shaders.clear();
  to_delete_programs.clear();
  to_delete_framebuffers.clear();
  to_delete_renderbuffers.clear();
  to_delete_textures.clear();
  to_delete_buffers.clear();
  #endif
}
