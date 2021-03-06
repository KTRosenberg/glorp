
#include "init.h"
#include "debug.h"
#include "args.h"
#include "os.h"
#include "util.h"
#include "parse.h"

#include <sys/stat.h>
#include <fstream>

#include <vector>

using namespace std;

DEFINE_bool(addr2line, false, "Call addr2line for stack traces");

namespace Glorp {
  int exeSize();
  
  static string loc_exename;
  void set_exename(int *argc, const char ***argv) {
    CHECK(*argc >= 1);
    if(*argc < 1) return;
    
    loc_exename = "/proc/self/exe";

    struct stat lulz;
    
    // see if we can get /proc/self/exe. if we can't, defer to our first parameter
    if(stat(loc_exename.c_str(), &lulz)) {
      loc_exename = (*argv)[0];
    }
    
    // see if we can get that. if we can't, append .exe to it because windows is a cock
    if(stat(loc_exename.c_str(), &lulz)) {
      loc_exename += ".exe";  // lazy hack
    }
    
    exeSize(); // check to make sure we can get the actual exe size, cache the result
  }
    
  ADD_INITTER(set_exename, -100);

  int exeSize() {
    static int exesize = 0;
    if (exesize) return exesize;
    
    CHECK(loc_exename.size());
    struct stat lulz;
    if(stat(loc_exename.c_str(), &lulz)) {
      dprintf("Couldn't stat file %s\n", loc_exename.c_str());
      CHECK(0);
    }
    exesize = lulz.st_size;
    return exesize;
  }
  
  string exeName() {
    return loc_exename;
  }
  
  inline bool verifyInlined(const void *const p) {
    return __builtin_return_address(1) == p;
  }

  bool testInlined() {
    return verifyInlined(__builtin_return_address(1));
  }

  bool isUnoptimized() {
    return !testInlined();
  }
  
  void stackOutput() {
    vector<const void *> stack;
    stackDump(&stack);
    
    vector<pair<string, string> > tokens;
    if(FLAGS_addr2line) {
      string line = "addr2line -f -e " + exeName() + " ";
      for(int i = 0; i < stack.size(); i++)
        line += Format("%08x ", (unsigned int)stack[i]);
      line += "> addr2linetmp.txt";
      int rv = system(line.c_str());
      if(!rv) {
        {
          ifstream ifs("addr2linetmp.txt");
          string lin;
          while(getline(ifs, lin)) {
            string tlin;
            getline(ifs, tlin);
            tokens.push_back(make_pair(lin, tlin));
          }
        }
        unlink("addr2linetmp.txt");
      } else {
        dprintf("Couldn't call addr2line\n");
        return;
      }
    
      {
        string line = "c++filt -n -s gnu-v3 ";
        for(int i = 0; i < tokens.size(); i++)
          line += tokens[i].first + " ";
        line += "> cppfilttmp.txt";
        int rv = system(line.c_str());
        if(!rv) {
          {
            ifstream ifs("cppfilttmp.txt");
            string lin;
            int ct = 0;
            while(getline(ifs, lin)) {
              if(lin.size() && lin[0] == '_')
                lin.erase(lin.begin());
              dprintf("  %s - %s", tokens[ct].second.c_str(), lin.c_str());
              ct++;
            }
          }
          unlink("cppfilttmp.txt");
        } else {
          dprintf("Couldn't call c++filt\n");
          return;
        }
      }
    } else {
      for(int i = 0; i < stack.size(); i++)
        dprintf("  %08x", (unsigned int)stack[i]);
    }

    dprintf("\n");
  }
  
  void directoryConfigMake() {
    string directory_delimiter = directoryDelimiter();
    
    CHECK(directory_delimiter.size() == 1);
    vector<string> tok = tokenize(directoryConfig(), directory_delimiter);
    
    string cc;
    if(directoryConfig().size() && directoryConfig()[0] == directory_delimiter[0])
      cc = directory_delimiter;
    
    for(int i = 0; i < tok.size(); i++) {
      if(cc.size())
        cc += directory_delimiter;
      cc += tok[i];
      directoryMkdir(cc);
    }
  }
}
