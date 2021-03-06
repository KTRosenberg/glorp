
#include "args.h"

#include "debug.h"
#include "init.h"
#include "parse.h"

#include <fstream>

using namespace std;

namespace Glorp
{
  
  class LinkageData {
  public:
    enum { LINKAGE_BOOL, LINKAGE_STRING, LINKAGE_INT, LINKAGE_FLOAT, LINKAGE_LAST };
    int type;
    
    string descr;
    
    FlagSource *source;
    
    string str_def;
    int int_def;
    bool bool_def;
    float float_def;
    
    string *str_link;
    int *int_link;
    bool *bool_link;
    float *float_link;
    
    LinkageData();
  };

  string canonize(const string &in) {
    string tbx;
    for(int i = 0; i < in.size(); i++)
      if(isalpha(in[i]))
        tbx.push_back(tolower(in[i]));
    return tbx;
  }

  map<string, LinkageData> &getLinkageSingleton() {
    static map<string, LinkageData> singy;
    return singy;
  }

  class SimpleBind {
  public:
    virtual void operator()() const = 0;

    virtual ~SimpleBind() { };
  };

  template<typename T> class SimpleBindImp : public SimpleBind {
  private:
    T *item;
    void (*f)(T *);

  public:
    SimpleBindImp<T>(void (&f_in)(T *), T *item_in)
    {
      item = item_in;
      f = &f_in;
    }
    
    void operator()() const {
      f(item);
    }
  };

  vector<SimpleBind *> &getMassageSingleton() {
    static vector<SimpleBind *> singy;
    return singy;
  }

  ARGS_LinkageObject::ARGS_LinkageObject(const string &id, string *writeto, FlagSource *source, const string &def, const string &descr) {
    map<string, LinkageData> &links = getLinkageSingleton();
    LinkageData ld;
    ld.descr = descr;
    ld.source = source;
    ld.type = LinkageData::LINKAGE_STRING;
    ld.str_def = def;
    ld.str_link = writeto;
    links[canonize(id)] = ld;
  }
  ARGS_LinkageObject::ARGS_LinkageObject(const string &id, int *writeto, FlagSource *source, int def, const string &descr) {
    map<string, LinkageData> &links = getLinkageSingleton();
    LinkageData ld;
    ld.descr = descr;
    ld.source = source;
    ld.type = LinkageData::LINKAGE_INT;
    ld.int_def = def;
    ld.int_link = writeto;
    links[canonize(id)] = ld;
  }
  ARGS_LinkageObject::ARGS_LinkageObject(const string &id, bool *writeto, FlagSource *source, bool def, const string &descr) {
    map<string, LinkageData> &links = getLinkageSingleton();
    LinkageData ld;
    ld.descr = descr;
    ld.source = source;
    ld.type = LinkageData::LINKAGE_BOOL;
    ld.bool_def = def;
    ld.bool_link = writeto;
    links[canonize(id)] = ld;
  }
  ARGS_LinkageObject::ARGS_LinkageObject(const string &id, float *writeto, FlagSource *source, float def, const string &descr) {
    map<string, LinkageData> &links = getLinkageSingleton();
    LinkageData ld;
    ld.descr = descr;
    ld.source = source;
    ld.type = LinkageData::LINKAGE_FLOAT;
    ld.float_def = def;
    ld.float_link = writeto;
    links[canonize(id)] = ld;
  }

  LinkageData::LinkageData() {
    type = -1;
    source = NULL;
    str_link = NULL;
    int_link = NULL;
    bool_link = NULL;
    float_link = NULL;
  }

  map< string, string > getFlagDescriptions() {
    map<string, string> rv;
    map<string, LinkageData> &ls = getLinkageSingleton();
    for(map<string, LinkageData>::const_iterator it = ls.begin(); it != ls.end(); it++)
      rv.insert(make_pair(it->first, it->second.descr));
    return rv;
  }

  void initFlags(int *argcp, const char ***argvp, int ignoreargs) {
    int argc = *argcp;
    const char **argv = *argvp;
    
    map<string, LinkageData> &links = getLinkageSingleton();
    for(map<string, LinkageData>::iterator itr = links.begin(); itr != links.end(); itr++) {
      if(itr->second.type == LinkageData::LINKAGE_BOOL) {
        *itr->second.bool_link = itr->second.bool_def;
      } else if(itr->second.type == LinkageData::LINKAGE_INT) {
        *itr->second.int_link = itr->second.int_def;
      } else if(itr->second.type == LinkageData::LINKAGE_STRING) {
        *itr->second.str_link = itr->second.str_def;
      } else if(itr->second.type == LinkageData::LINKAGE_FLOAT) {
        *itr->second.float_link = itr->second.float_def;
      } else {
        CHECK(0);
      }
      *itr->second.source = FS_DEFAULT;
    }
    for(map<string, LinkageData>::iterator itr = links.begin(); itr != links.end(); itr++) {
      if(itr->second.type == LinkageData::LINKAGE_BOOL) {
        //dprintf("Initted bool %s to %d\n", itr->first.c_str(), *itr->second.bool_link);
      } else if(itr->second.type == LinkageData::LINKAGE_INT) {
        //dprintf("Initted int %s to %d\n", itr->first.c_str(), *itr->second.int_link);
      } else if(itr->second.type == LinkageData::LINKAGE_STRING) {
        //dprintf("Initted string %s to %s\n", itr->first.c_str(), itr->second.str_link->c_str());
      } else if(itr->second.type == LinkageData::LINKAGE_FLOAT) {
        //dprintf("Initted float %s to %f\n", itr->first.c_str(), *itr->second.float_link);
      } else {
        CHECK(0);
      }
    }
    
    vector<pair<string, FlagSource> > lines;
    
    {
      ifstream ifs("glorp/settings.local");
      string dt;
      while(getLineStripped(ifs, &dt))
        lines.push_back(make_pair(dt, FS_FILE));
    }
    
    vector<const char *> remains;
    for(int i = 0; i < ignoreargs + 1; i++)
      remains.push_back(argv[i]);
    
    for(int i = ignoreargs + 1; i < argc; i++) {
      if(argv[i][0] == '-' && argv[i][1] == '-') {
        lines.push_back(make_pair(argv[i] + 2, FS_CLI));
      } else {
        remains.push_back(argv[i]);
      }
    }
    
    remains.push_back(NULL);
    std::copy(remains.begin(), remains.end(), argv);
    
    for(int i = 0; i < lines.size(); i++) {
      const char *arg = lines[i].first.c_str();
      bool isBoolNo = false;
      if(strlen(arg) >= 2 && tolower(arg[0]) == 'n' && tolower(arg[1]) == 'o') {
        isBoolNo = true;
        arg += 2;
      }
      const char *eq = strchr(arg, '=');
      string realarg;
      if(eq) {
        realarg = string(arg, eq);
        eq++;
      } else {
        realarg = arg;
      }
      
      realarg = canonize(realarg);
      
      if(!links.count(realarg)) {
        dprintf("Invalid commandline argument %s\n", arg);
        CHECK(0);
        continue;
      }
      
      LinkageData &ld = links[realarg];
      if(ld.type == LinkageData::LINKAGE_BOOL) {
        CHECK(!eq || string(eq) == "true" || string(eq) == "false");
        CHECK(!eq || !isBoolNo);
        CHECK(ld.bool_link);
        if(isBoolNo || (eq && string(eq) == "false")) {
          *ld.bool_link = false;
        } else {
          *ld.bool_link = true;
        }
      } else if(ld.type == LinkageData::LINKAGE_STRING) {
        CHECK(!isBoolNo && eq);
        CHECK(ld.str_link);
        *ld.str_link = eq;
      } else if(ld.type == LinkageData::LINKAGE_INT) {
        CHECK(!isBoolNo && eq);
        CHECK(ld.int_link);
        *ld.int_link = atoi(eq);
      } else if(ld.type == LinkageData::LINKAGE_FLOAT) {
        CHECK(!isBoolNo && eq);
        CHECK(ld.float_link);
        *ld.float_link = atof(eq);
      } else {
        CHECK(0);
      }
      
      *ld.source = lines[i].second;
    }
    
    for(int i = 0; i < getMassageSingleton().size(); i++) {
      (*getMassageSingleton()[i])();
      delete getMassageSingleton()[i];
    }
    getMassageSingleton().clear();
    
    for(map<string, LinkageData>::iterator itr = links.begin(); itr != links.end(); itr++) {
      if(itr->second.type == LinkageData::LINKAGE_BOOL) {
        //dprintf("Set bool %s to %d\n", itr->first.c_str(), *itr->second.bool_link);
      } else if(itr->second.type == LinkageData::LINKAGE_INT) {
        //dprintf("Set int %s to %d\n", itr->first.c_str(), *itr->second.int_link);
      } else if(itr->second.type == LinkageData::LINKAGE_STRING) {
        //dprintf("Set string %s to %s\n", itr->first.c_str(), itr->second.str_link->c_str());
      } else if(itr->second.type == LinkageData::LINKAGE_FLOAT) {
        //dprintf("Set float %s to %f\n", itr->first.c_str(), *itr->second.float_link);
      } else {
        CHECK(0);
      }
    }
  }

  int fignore = 0;
  void setInitFlagIgnore(int args) {
    fignore = args;
  }

  void initFlagSpawner(int *argc, const char ***argv) {
    initFlags(argc, argv, fignore);
  }
  ADD_INITTER(initFlagSpawner, 0);

}
