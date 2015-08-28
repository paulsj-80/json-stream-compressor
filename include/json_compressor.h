#ifndef JSON_COMPRESSOR_H
#define JSON_COMPRESSOR_H

#include <string>

#define JSC_LINE_BUFF_SIZE 4096
#define JSC_NUMBER_BUFF_SIZE 16
#define JSC_LINEFEED 10


using namespace std;

namespace jsc {

    void compress(string infileName, string outfileName);
    
}

#endif
