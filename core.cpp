
#include "lgl.h"
#include "lal.h"

#include <AL/al.h>
#include <AL/alc.h>
  
#ifndef LINUX
  // can't include on linux because GLee includes X.h which conflicts with the Font class
  #include "GLee.h"
#endif

#include <Glop/Base.h>
#include <Glop/Font.h>
#include <Glop/GlopFrame.h>
#include <Glop/GlopWindow.h>
#include <Glop/Image.h>
#include <Glop/Input.h>
#include <Glop/OpenGl.h>
#include <Glop/System.h>
#include <Glop/ThinLayer.h>
#include <Glop/Thread.h>
#include <Glop/glop3d/Camera.h>
#include <Glop/glop3d/Mesh.h>
#include <Glop/Sound.h>
#include <Glop/Os.h>
#ifndef LINUX
  #include <Glop/Os_Hacky.h>
#endif

#include <iostream>

#include <lua.hpp>

#include <luabind/luabind.hpp>
#include <luabind/operator.hpp>
#include <luabind/discard_result_policy.hpp>
#include <luabind/adopt_policy.hpp>

#include <boost/random.hpp>

#include "debug.h"
#include "util.h"
#include "core.h"
#include "args.h"
#include "init.h"
#include "perfbar.h"
#include "version.h"
#include "os.h"
#include "os_ui.h"
#include "core_glutil.h"
#include "core_alutil.h"
#include "core_xutil.h"

#include <png.h>

#ifdef IPHONE
#include "main_iphone.h"
#endif

#ifdef GLORP_BOX2D
#include "box2d.h"
#endif

using namespace std;

lua_State *L;

int phys_screenx, phys_screeny;
int get_screenx() { return phys_screenx; }
int get_screeny() { return phys_screeny; }

#ifdef WIN32
  void ods(const string &str) {
    OutputDebugString(str.c_str());
  }

  void log_to_debugstring(const string &str) {
    OutputDebugString(str.c_str());
    dbgrecord().push_back(str);
    if(dbgrecord().size() > 10000)
      dbgrecord().pop_front();
  }
#endif

#if defined(MACOSX) || defined(LINUX)
  #undef printf
  void ods(const string &str) {
    if(str.size() && str[str.size() - 1] == '\n')
      printf("%s", str.c_str());
    else
      printf("%s\n", str.c_str());
  }

  void log_to_debugstring(const string &str) {
    if(str.size() && str[str.size() - 1] == '\n')
      printf("%s", str.c_str());
    else
      printf("%s\n", str.c_str());
    dbgrecord().push_back(str);
    if(dbgrecord().size() > 10000)
      dbgrecord().pop_front();
  }
  #define printf FAILURE
#endif
  
// iphone equivalent stashed in main_iphone.mm

extern "C" {
static int debug_print(lua_State *L) {
  string acul = "";
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      return luaL_error(L, LUA_QL("tostring") " must return a string to "
                           LUA_QL("print"));
    if (i>1) acul += "\t";
    acul += s;
    lua_pop(L, 1);  /* pop result */
  }
  dprintf("%s", acul.c_str());
  return 0;
}
}

luaL_Reg regs[] = {
  {"printy", debug_print},
  {NULL, NULL},
};

void stackDump (lua_State *L) {
  dprintf("stackdump");
  int i;
  int top = lua_gettop(L);
  for (i = 1; i <= top; i++) {  /* repeat for each level */
    int t = lua_type(L, i);
    switch (t) {

      case LUA_TSTRING:  /* strings */
        dprintf("`%s'", lua_tostring(L, i));
        break;

      case LUA_TBOOLEAN:  /* booleans */
        dprintf("%s", lua_toboolean(L, i) ? "true" : "false");
        break;

      case LUA_TNUMBER:  /* numbers */
        dprintf("%g", lua_tonumber(L, i));
        break;

      default:  /* other values */
        dprintf("%s", lua_typename(L, t));
        break;

    }
    dprintf("  ");  /* put a separator */
  }
  dprintf("\n");  /* end the listing */
}

void loadfile(lua_State *L, const char *file) {
  lua_getglobal(L, "core__loadfile");
  lua_pushstring(L, file);
  int rv = lua_pcall(L, 1, 0, 0);
  if (rv) {
    CHECK(0, "%s", lua_tostring(L, -1));
  }
}


string last_preserved_token;

void meltdown() {
  lua_getglobal(L, "generic_wrap");
  lua_getglobal(L, "fuckshit");
  int rv = lua_pcall(L, 1, 1, 0);
  if (rv) {
    CHECK(0, "%s", lua_tostring(L, -1));
  }
  
  if(lua_isstring(L, -1)) {
    last_preserved_token = lua_tostring(L, -1);
  }
  lua_pop(L, 1);
}

bool perfbar_enabled = false;
void set_perfbar(bool x) {perfbar_enabled = x;}

class GlorpThinLayer : public ThinLayer {
public:
  void Render() const {
    {
      PerfStack pb(0.5, 0.0, 0.0);
      
      lua_getglobal(L, "generic_wrap");
      lua_getglobal(L, "render");
      int rv = lua_pcall(L, 1, 1, 0);
      if (rv) {
        dprintf("%s", lua_tostring(L, -1));
        meltdown();
        CHECK(0);
      }
      
      if(lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
        // Something broke, so we're displaying the error screen
        GlUtils::SetNoTexture();
        glBegin(GL_QUADS);
        int scal = phys_screenx / 20;
        for(int x = -5; x < 5; x++) {
          for(int y = -2; y < 2; y++) {
            if((x + y) % 2 == 0) {
              glColor3d(1., 1., 1.);
            } else {
              glColor3d(1., 0., 0.);
            }
            glVertex2d((x + 0.) * scal + phys_screenx / 2, (y + 0.) * scal + phys_screeny / 2);
            glVertex2d((x + 1.) * scal + phys_screenx / 2, (y + 0.) * scal + phys_screeny / 2);
            glVertex2d((x + 1.) * scal + phys_screenx / 2, (y + 1.) * scal + phys_screeny / 2);
            glVertex2d((x + 0.) * scal + phys_screenx / 2, (y + 1.) * scal + phys_screeny / 2);
          }
        }
        glEnd();
      }
      lua_pop(L, 1);
    }
    
    if(perfbar_enabled)
      drawPerformanceBar();
    startPerformanceBar();
  }
};

class WrappedTex {
private:
  Texture *tex;
  Image *img;
  string fname;

public:
  WrappedTex(Image *intex, const string &inf) {
    img = intex;
    tex = new Texture(img);
    fname = inf;
  }
    
  int GetWidth() const {
    return tex->GetWidth();
  }
  int GetHeight() const {
    return tex->GetHeight();
  }
  int GetInternalWidth() const {
    return tex->GetInternalWidth();
  }
  int GetInternalHeight() const {
    return tex->GetInternalHeight();
  }
  
  unsigned long GetPixel(int x, int y) const {
    CHECK(x >= 0 && x < tex->GetWidth() && y >= 0 && y < tex->GetHeight());
    return *(const unsigned long*)tex->GetImage()->Get(x, y);
  }
  
  void SetTexture() {
    GlUtils::SetTexture(tex);
    
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glTranslatef(-1 / (float)tex->GetInternalWidth() / 2, -1 / (float)tex->GetInternalHeight() / 2, 0);
    glMatrixMode(GL_MODELVIEW);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  
  const string &getfname() const {
    return fname;
  }
  
  ~WrappedTex() {
    delete tex;
    delete img;
  };
};
std::ostream& operator<<(std::ostream&ostr, WrappedTex const&ite) {
  ostr << "<texture \"" << ite.getfname() << ">";
  return ostr;
}


WrappedTex *GetTex(const string &image) {
  Image *tex = Image::Load("data/" + image + ".png");
  if(!tex) tex = Image::Load("data/" + image + ".jpg");
  if(!tex) tex = Image::Load(image + ".png");
  if(!tex) tex = Image::Load(image + ".jpg");
  if(!tex) tex = Image::Load(string("build/") + game_platform + "/glorp/" + image + ".png");
  if(!tex) return NULL;
  
  return new WrappedTex(tex, image);
}

void SetNoTex() {
  GlUtils::SetNoTexture();
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
}

GlopKey adapt(const string &id) {
  CHECK(id.size() > 0);
  
  GlopKey ki;
  
  if(id == "arrow_left") ki = kKeyLeft; else
  if(id == "arrow_right") ki = kKeyRight; else
  if(id == "arrow_up") ki = kKeyUp; else
  if(id == "arrow_down") ki = kKeyDown; else
  if(id == "enter") ki = kKeyEnter; else
  if(id == "mouse_left") ki = kMouseLButton; else
  if(id == "mouse_right") ki = kMouseRButton; else
  if(id == "mouse_middle") ki = kMouseMButton; else
  if(id == "mouse_wheel_up") ki = kMouseWheelUp; else
  if(id == "mouse_wheel_down") ki = kMouseWheelUp; else
  if(id == "shift") ki = kKeyLeftShift; else
  if(id == "shift_left") ki = kKeyLeftShift; else
  if(id == "shift_right") ki = kKeyRightShift; else
  if(id == "ctrl") ki = kKeyLeftControl; else
  if(id == "printscreen") ki = kKeyPrintScreen; else
  if(id == "escape") ki = kKeyEscape; else
    ki = GlopKey(id[0]);
  
  return ki;
}

bool IsKeyDownFrameAdapter(const string &id) {
  return input()->IsKeyDownFrame(adapt(id));
};

class KeyList : public KeyListener {
  void OnKeyEvent(const KeyEvent &event) {
    string keyvent;
    if(event.GetMainKey() == kMouseLButton) { keyvent = "mouse_left"; }
    if(event.GetMainKey() == kMouseRButton) { keyvent = "mouse_right"; }
    if(event.GetMainKey() == kMouseMButton) { keyvent = "mouse_middle"; }
    if(event.GetMainKey() == kMouseWheelUp) { keyvent = "mouse_wheel_up"; }
    if(event.GetMainKey() == kMouseWheelDown) { keyvent = "mouse_wheel_down"; }
    
    if(event.GetMainKey() == kKeyLeft) { keyvent = "arrow_left"; }
    if(event.GetMainKey() == kKeyRight) { keyvent = "arrow_right"; }
    if(event.GetMainKey() == kKeyUp) { keyvent = "arrow_up"; }
    if(event.GetMainKey() == kKeyDown) { keyvent = "arrow_down"; }
    
    if(event.GetMainKey().IsKeyboardKey() && event.GetMainKey().index >= 'a' && event.GetMainKey().index <= 'z') { keyvent = string(1, event.GetMainKey().index); }
    if(event.GetMainKey().IsKeyboardKey() && event.GetMainKey().index >= '0' && event.GetMainKey().index <= '9') { keyvent = string(1, event.GetMainKey().index); }
    if(event.GetMainKey().IsKeyboardKey() && event.GetMainKey().index == '`') { keyvent = string(1, event.GetMainKey().index); }
    
    if(event.GetMainKey() == kKeyF2) { keyvent = "f2"; }
    if(event.GetMainKey() == kKeyDelete) { keyvent = "delete"; }
    if(event.GetMainKey() == kKeyPrintScreen) { keyvent = "printscreen"; }
    
    if(event.GetMainKey() == kKeyEnter) { keyvent = "enter"; }
    if(event.GetMainKey() == kKeyEscape) { keyvent = "escape"; }
    
    unsigned char aski = input()->GetAsciiValue(event.GetMainKey());
    string bleep;
    if(aski) {
      bleep = string(1, aski);
    }
    
    if(keyvent.size() || aski) {
      string typ;
      if(event.IsDoublePress()) {
        typ = "press_double";
      } else if(event.IsRepeatPress()) {
        typ = "press_repeat";
      } else if(event.IsNonRepeatPress()) {
        typ = "press";
      } else if(event.IsRelease()) {
        typ = "release";
      } else if(event.IsNothing()) {
        typ = "nothing";
      }
      
      lua_getglobal(L, "generic_wrap");
      lua_getglobal(L, "UI_Key");
      if(keyvent.size()) {
        lua_pushstring(L, keyvent.c_str());
      } else {
        lua_pushnil(L);
      }
      if(bleep.size()) {
        lua_pushstring(L, bleep.c_str());
      } else {
        lua_pushnil(L);
      }
      lua_pushstring(L, typ.c_str());
      int rv = lua_pcall(L, 4, 0, 0);
      if (rv) {
        dprintf("%s", lua_tostring(L, -1));
        meltdown();
        CHECK(0);
      }
    }
  }
};

class PerfBarManager {
  PerfStack *ps;
  
public:
  PerfBarManager(float r, float g, float b) {
    ps = new PerfStack(r, g, b);
  }
  
  void Destroy() {
    delete ps;
    ps = NULL;
  }
};

bool exiting = false;
void TriggerExit() {
  exiting = true;
};

void adaptaload(const string &fname) {
  int error = luaL_dofile(L, ("data/" + fname).c_str());
  if(error) {
    error = luaL_dofile(L, ("glorp/" + fname).c_str());
  }
  if(error) {
    CHECK(0, "%s", lua_tostring(L, -1));
  }
}

int gmx() {return input()->GetMouseX();};
int gmy() {return input()->GetMouseY();};

// arrrgh
boost::lagged_fibonacci9689 rngstate(time(NULL));
static int math_random (lua_State *L) {
  /* the `%' avoids the (rare) case of r==1, and is needed also because on
     some systems (SunOS!) `rand()' may return a value larger than RAND_MAX */
  lua_Number r = rngstate();
  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, r);  /* Number between 0 and 1 */
      break;
    }
    case 1: {  /* only upper limit */
      int u = luaL_checkint(L, 1);
      luaL_argcheck(L, 1<=u, 1, "interval is empty");
      lua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
      break;
    }
    case 2: {  /* lower and upper limits */
      int l = luaL_checkint(L, 1);
      int u = luaL_checkint(L, 2);
      luaL_argcheck(L, l<=u, 2, "interval is empty");
      lua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
      break;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  return 1;
}
static int math_randomseed (lua_State *L) {
  rngstate = boost::lagged_fibonacci9689(luaL_checkint(L, 1));
  return 0;
}

void sms(bool bol) {
  input()->ShowMouseCursor(bol);
}
void lms(bool bol) {
  window()->LockMouseCursor(bol);
}
void setmousepos(int x, int y) {
  input()->SetMousePosition(x, y);
}

void get_stack_entry(lua_State *L, int level) {
  lua_pushliteral(L, "");
  
  lua_Debug ar;
  CHECK(lua_getstack(L, level, &ar));
  
  lua_getinfo(L, "Snl", &ar);
  
  lua_pushfstring(L, "%s:", ar.short_src);
  if (ar.currentline > 0)
    lua_pushfstring(L, "%d:", ar.currentline);

  if (*ar.namewhat != '\0')  /* is there a name? */
      lua_pushfstring(L, " in function " LUA_QS, ar.name);
  else {
    if (*ar.what == 'm')  /* main? */
      lua_pushfstring(L, " in main chunk");
    else if (*ar.what == 'C' || *ar.what == 't')
      lua_pushliteral(L, " ?");  /* C function or tail call */
    else
      lua_pushfstring(L, " in function <%s:%d>",
                         ar.short_src, ar.linedefined);
  }
  lua_concat(L, lua_gettop(L) - 1);
}

#ifndef IPHONE
bool screenshot_to(const string &fname) {
  FILE *fil = fopen(fname.c_str(), "wb");
  if(!fil) return false;
  
  // alright let's get this shit done
  glReadBuffer(GL_FRONT);
  vector<char> dat;
  int x = window()->GetWidth();
  int y = window()->GetHeight();
  dprintf("screeny %dx%d\n", x, y);
  dat.resize(x * y * 4);
  vector<char*> ptrs;
  for(int i = 0; i < y; i++)
    ptrs.push_back(&dat[(y - i - 1) * x * 4]);
  glReadPixels(0, 0, x, y, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, &dat[0]);
  
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  CHECK(png_ptr);
  png_infop info_ptr = png_create_info_struct(png_ptr);
  CHECK(info_ptr);
  CHECK(!setjmp(png_jmpbuf(png_ptr)));
  png_init_io(png_ptr, fil);
  
  png_set_IHDR(png_ptr, info_ptr, x, y, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png_ptr, info_ptr);
  png_write_image(png_ptr, (png_byte**)&ptrs[0]); // yaaaaaay
  png_write_end(png_ptr, NULL);
  
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fil);
  return true;
}
#endif

string get_mid_name() {
  return game_midname;
}

#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

void debugstack_annotated(lua_State *L) {
  int level = 1;
  
  lua_pushliteral(L, "stack traceback:");
  
  lua_Debug ar;
  int firstpart = 1;  /* still before eventual `...' */
  while (lua_getstack(L, level++, &ar)) {
    if (level > LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L, level+LEVELS2, &ar))
        level--;  /* keep going */
      else {
        lua_pushliteral(L, "\n\t...");  /* too many levels */
        while (lua_getstack(L, level+LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    lua_pushliteral(L, "\n\t");
    lua_getinfo(L, "Snl", &ar);
    lua_pushfstring(L, "%s:", ar.short_src);
    if (ar.currentline > 0)
      lua_pushfstring(L, "%d:", ar.currentline);
    if (*ar.namewhat != '\0')  /* is there a name? */
        lua_pushfstring(L, " in function " LUA_QS, ar.name);
    else {
      if (*ar.what == 'm')  /* main? */
        lua_pushfstring(L, " in main chunk");
      else if (*ar.what == 'C' || *ar.what == 't')
        lua_pushliteral(L, " ?");  /* C function or tail call */
      else
        lua_pushfstring(L, " in function <%s:%d>",
                           ar.short_src, ar.linedefined);
    }
    
    lua_getlocal(L, &ar, 1);
    if(lua_istable(L, -1)) {
      lua_pushliteral(L, "__name");
      lua_rawget(L, -2);
      if(lua_isstring(L, -1)) {
        lua_pushliteral(L, " in frame (");
        lua_insert(L, -2);
        lua_pushliteral(L, ")");
        lua_concat(L, 3);
        
        lua_insert(L, -2);
      } else {
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
    
    lua_concat(L, lua_gettop(L));
  }
  lua_concat(L, lua_gettop(L));
}
/*
void CrashHorribly() {
  *(int*)0 = 0;
}*/

int toaddress(lua_State *L) {
  CHECK(lua_gettop(L) == 1);
  lua_pushfstring(L, "%p", lua_topointer(L, 1));
  return 1;
}

void glewp(double a, double b, double c, double d) {
  gluPerspective(a, b, c, d);
}

bool window_in_focus() {
  return window()->IsInFocus();
}

#define ll_subregister(L, cn, sn, f) (lua_getglobal(L, cn), lua_pushstring(L, sn), lua_pushcfunction(L, f), lua_settable(L, -3))

double time_micro() {
  return system()->GetTimeMicro();
}



class DontKillMeBro_KillerBase;

vector<DontKillMeBro_KillerBase*> to_be_killed;

class DontKillMeBro_KillerBase {
public:
  virtual ~DontKillMeBro_KillerBase() {};
};
template <typename T> class DontKillMeBro_Killer : public DontKillMeBro_KillerBase {
  T *ite;
public:
  DontKillMeBro_Killer<T>(T *item) {
    ite = item;
  }
  ~DontKillMeBro_Killer<T>() {
    delete ite;
  }
};

template<typename T> class DontKillMeBro : public auto_ptr_customized<T, DontKillMeBro<T> > {
public:
  
  DontKillMeBro<T>(T *item) : auto_ptr_customized<T, DontKillMeBro<T> >(item) { };
  static void cleanup(T *item) {
    to_be_killed.push_back(new DontKillMeBro_Killer<T>(item));
  }
};

void luainit(int argc, const char **argv) {
  L = lua_open();   /* opens Lua */
  luaL_openlibs(L);
  
  luaopen_lgl(L);
  luaopen_lal(L);
  
  lua_register(L, "print", debug_print);
  lua_register(L, "toaddress", toaddress);
  
  ll_subregister(L, "math", "random", math_random);
  ll_subregister(L, "math", "randomseed", math_randomseed);  

  {
    using namespace luabind;
    
    luabind::open(L);
    
    module(L)
    [
      class_<WrappedTex, DontKillMeBro<WrappedTex> >("WrappedTex_Internal")
        .def("GetWidth", &WrappedTex::GetWidth)
        .def("GetHeight", &WrappedTex::GetHeight)
        .def("GetInternalWidth", &WrappedTex::GetInternalWidth)
        .def("GetInternalHeight", &WrappedTex::GetInternalHeight)
        .def("SetTexture", &WrappedTex::SetTexture)
        .def("GetPixel", &WrappedTex::GetPixel)
        .def(tostring(self)),
      class_<PerfBarManager, DontKillMeBro<PerfBarManager> >("Perfbar_Init")
        .def(constructor<float, float, float>())
        .def("Destroy", &PerfBarManager::Destroy),
      //class_<SoundSource, DontKillMeBro<SoundSource> >("SourceSource_Make")
        //.def("Stop", &SoundSource::Stop),
      def("Texture", &GetTex, adopt(result)),
      def("SetNoTexture", &SetNoTex),
      def("IsKeyDown", &IsKeyDownFrameAdapter),
      //def("PlaySound_Core", &DoASound),
      //def("ControlSound_Core", &ControllableSound),
      def("TriggerExit", &TriggerExit),
      def("GetMouseX", &gmx),
      def("GetMouseY", &gmy),
      def("ShowMouseCursor", &sms),
      def("LockMouseCursor", &lms),
      def("SetMousePosition", &setmousepos),
      #ifndef IPHONE
      def("ScreenshotTo", &screenshot_to),
      #endif
      def("TimeMicro", &time_micro),
      def("GetMidName", &get_mid_name),
      def("GetDesktopDirectory", &getDesktopDirectory),
      def("GetConfigDirectory", &getConfigDirectory),
      def("MakeConfigDirectory", &makeConfigDirectory),
      def("Perfbar_Set", &set_perfbar),
      def("get_stack_entry", &get_stack_entry),
      def("debugstack_annotated", &debugstack_annotated),
      
      #ifdef IPHONE
      def("touch_getCount", &os_touch_getCount),
      def("touch_getActive", &os_touch_getActive),
      def("touch_getX", &os_touch_getX),
      def("touch_getY", &os_touch_getY),
      #endif
      
      def("GetScreenX", &get_screenx),
      def("GetScreenY", &get_screeny),
      
      def("WindowInFocus", &window_in_focus),
      def("gluPerspective", &glewp)
      //def("CrashHorribly", &CrashHorribly)
    ];
    
    glorp_glutil_init(L);
    glorp_alutil_init(L);
    
    #ifdef GLORP_BOX2D
    glorp_box2d_init(L);
    #endif
  }
  
  adaptaload("wrap.lua");
  
  {
    lua_getglobal(L, "generic_wrap");
    lua_getglobal(L, "wrap_init");
    lua_pushstring(L, game_platform);
    for(int i = 0; i < argc; i++) {
      lua_pushstring(L, argv[i]);
    }
    int error = lua_pcall(L, 2 + argc, 0, 0);
    if(error) {
      CHECK(0, "%s", lua_tostring(L, -1));
    }
  }
  
  if(last_preserved_token.size()) {
    lua_getglobal(L, "generic_wrap");
    lua_getglobal(L, "de_fuckshit");
    lua_pushstring(L, last_preserved_token.c_str());
    int rv = lua_pcall(L, 2, 0, 0);
    if (rv) {
      CHECK(0, "%s", lua_tostring(L, -1));
    }
  }
}
void luashutdown() {
  
  CHECK(!luaL_dostring(L, StringPrintf("if pepperfish_profiler then pepperfish_profiler:stop()  local outfile = io.open(\"profile_%d.txt\", \"w\") pepperfish_profiler:report(outfile) outfile:close() end", (int)time(NULL)).c_str()));
  
  dprintf("lua closing");
  lua_close(L);
  dprintf("lua closed");
  L = NULL;
}

void fatal(const string &message) {
  dprintf("FATAL EXPLOSION OF FAILURE: %s", message.c_str()); fflush(stdout);
  CHECK(0);
}

DEFINE_bool(help, false, "Get help");
DEFINE_bool(development, false, "Development tools");
void glorp_init(const string &name, int width, int height, int argc, const char **argv) {

  #ifdef IPHONE
  width = 320;
  height = 480; // welp
  #endif
  
  phys_screenx = width;
  phys_screeny = height;

  LogToFunction(&log_to_debugstring);
  SetFatalErrorHandler(&fatal);

  System::Init();
  
  ALCdevice* pDevice = alcOpenDevice(NULL);
  CHECK(pDevice);
  ALCcontext* pContext = alcCreateContext(pDevice, NULL);
  CHECK(pContext);
  alcMakeContextCurrent(pContext);
  CHECK(alGetError() == AL_NO_ERROR);
  
  #ifdef IPHONE
    init_osx();
  #endif

  setInitFlagFile("glorp/settings");
  initProgram(&argc, const_cast<const char ***>(&argv));
  
  if(FLAGS_help) {
    map<string, string> flags = getFlagDescriptions();
    #undef printf
    printf("note: CLI help does not really exist in any meaningful enduser fashion, sorry, but that's OK 'cause there aren't really any usable enduser parameters\n");
    for(map<string, string>::iterator itr = flags.begin(); itr != flags.end(); itr++) {
      dprintf("%s: %s\n", itr->first.c_str(), itr->second.c_str());
      printf("%s: %s\n", itr->first.c_str(), itr->second.c_str());
    }
    return;
  }
  
  window()->SetTitle(name);
  //window()->SetVSync(true);
  
  {
    GlopWindowSettings gws;
    gws.min_aspect_ratio = (float)width / height;
    gws.min_inverse_aspect_ratio = (float)height / width;
    gws.is_resizable = false;
    CHECK(window()->Create(width, height, false, gws));
    window()->SetVSync(true);
    
    #ifdef WIN32
      SendMessage(get_first_handle(), WM_SETICON, ICON_BIG, (LPARAM)LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(101), IMAGE_ICON, 32, 32, 0));
      SendMessage(get_first_handle(), WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(101), IMAGE_ICON, 16, 16, 0));
    #endif
    
    #ifdef LINUX
      glorp_set_icons();
    #endif
  }

  dprintf("EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
  dprintf("VENDOR: %s\n", glGetString(GL_VENDOR));
  dprintf("RENDERER: %s\n", glGetString(GL_RENDERER));
  dprintf("VERSION: %s\n", glGetString(GL_VERSION));
    
  if(atof((const char*)glGetString(GL_VERSION)) < 2.0) {
    //CHECK_MESSAGE(false, "%s currently requires OpenGL 2.0, which your computer doesn't seem to have.\n\nUpdating your video drivers might fix the problem, or it might not. Sorry!\n\nI've created a datafile including some information that may help Mandible Games fix\nthe error in future versions. It contains no personally identifying information.\n\nMay I send this to Mandible?");
    //return;
  }
  
  GlorpThinLayer thinlayer;
  window()->SetThinLayer(&thinlayer);
  
  luainit(argc, argv);
  
  {
    KeyList listen_to_shit;
    
    bool wasdown = false;
    
    int lasttick = system()->GetTime();
    while(window()->IsCreated()) {
      system()->Think();
      
      // let's put the IPhone key stuff here
      #ifdef IPHONE
      {
        vector<TouchEvent> kiz = os_touch_getEvents();
        for(int i = 0; i < kiz.size(); i++) {
          if(kiz[i].type == EVENT_TOUCH || kiz[i].type == EVENT_RELEASE) {
            lua_getglobal(L, "generic_wrap");
            lua_getglobal(L, "UI_Key");
            
            lua_pushstring(L, StringPrintf("finger_%d", kiz[i].id).c_str());
            lua_pushnil(L); // no ascii ever
            if(kiz[i].type == EVENT_TOUCH) {
              lua_pushstring(L, "press");
            } else {
              lua_pushstring(L, "release");
            }
            int rv = lua_pcall(L, 4, 0, 0);
            if (rv) {
              dprintf("%s", lua_tostring(L, -1));
              meltdown();
              CHECK(0);
            }
          }
        }
      }
      #endif
      
      {
        PerfStack pb(0.0, 0.0, 0.5);
        
        int thistick = system()->GetTime();
        lua_getglobal(L, "generic_wrap");
        lua_getglobal(L, "UI_Loop");
        lua_pushnumber(L, thistick - lasttick);
        lasttick = thistick;
        int rv = lua_pcall(L, 2, 0, 0);
        if (rv) {
          dprintf("Crash\n");
          dprintf("%s", lua_tostring(L, -1));
          meltdown();
          CHECK(0);
        }
      }
      
      if(input()->IsKeyDownFrame(kKeyF12) && FLAGS_development) {
        if(!wasdown) {
          meltdown();
          luashutdown();
          luainit(argc, argv);
        }
        wasdown = true;
      } else {
        wasdown = false;
      }
      
      {
        PerfStack pb(0, 0.5, 0);
        lua_getglobal(L, "generic_wrap");
        lua_getglobal(L, "gcstep");
        int rv = lua_pcall(L, 1, 0, 0);
        if (rv) {
          dprintf("Crash\n");
          dprintf("%s", lua_tostring(L, -1));
          meltdown();
          CHECK(0);
        }
      }

      {
        vector<DontKillMeBro_KillerBase*> splatter;
        splatter.swap(to_be_killed);
        for(int i = 0; i < splatter.size(); i++)
          delete splatter[i];
      }
      
      glorp_glutil_tick();
      glorp_alutil_tick();
      
      if(exiting) {
        window()->Destroy();
      }
    }
  }
  
  meltdown();
  luashutdown();
  
  glorp_glutil_tick();
  glorp_alutil_tick();
  
  alcMakeContextCurrent(NULL);
  alcDestroyContext(pContext);
  alcCloseDevice(pDevice);

  System::ShutDown();
  
  dprintf("exiting");
}

/*
SoundSample *SSLoad(const string &fname_base, float vol) {
  SoundSample *rv;
  rv = SoundSample::Load("data/" + fname_base + ".caf", false, vol);  // hurr
  if(rv) return rv;
  rv = SoundSample::Load("data/" + fname_base + ".ogg", false, vol);
  if(rv) return rv;
  rv = SoundSample::Load("data/" + fname_base + ".wav", false, vol);
  if(rv) return rv;
  rv = SoundSample::Load("data/" + fname_base + ".flac", false, vol);
  if(rv) return rv;
  dprintf("Cannot find sound effect %s\n", fname_base.c_str());
  
  if(sound_manager()->IsInitialized())
    CHECK(0);
  
  return NULL;
}
*/
