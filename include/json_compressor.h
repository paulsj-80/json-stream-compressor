#ifndef JSON_COMPRESSOR_H
#define JSON_COMPRESSOR_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <map>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/operations.hpp>
#include "jp_wrapper.h"


namespace io = boost::iostreams;


#define JSC_LINE_BUFF_SIZE 4096
#define JSC_NUMBER_BUFF_SIZE 16
#define JSC_LINEFEED 10


using namespace std;

namespace jsc {

    class Compressor : public io::multichar_output_filter,
                       public jpw::Listener {
    public:
        enum SerializedConstants {
            T_NULL=0x0,
            T_INT=0xa0,        // | len (in bytes), then value;
                               // value is big-endian
            T_FLOAT=0xb0,      // | len (in bytes), then value
            T_BOOL=0xc0,       // | val
            T_STRING=0xd0,     // | byte count of len; followed by
                               // len bytes, then value itself
            END_RECORD=0xe0,
            START_DICTIONARY=0xf0
        };
    private:
        char buff[JSC_LINE_BUFF_SIZE];
        int buffLen;
        jpw::Parser parser;
        function<void (char*, streamsize)> sinkWrite;
        map<string, int> keyDictionary;

        inline void serialize_char(char c);
        inline void serialize_chars(char* s, int n);

        void serialize_int_val(int num, SerializedConstants sconst);
        void serialize_float_val(float f);
        void serialize_str_val(char* s, int n);

        static float parse_float(char* s, int n);
        static int parse_int(char* s, int n);

        void serialize(char* s, streamsize n);
        
    public:
        
        Compressor();
        Compressor(Compressor const& other);
        Compressor& operator=(const Compressor& other);
        virtual ~Compressor();

    public:
        
        template<typename Sink>
        streamsize write(Sink& dest, const char* s, streamsize n);

        template<typename Source>
        void close(Source&);

    public:
        virtual void handleKey(char* str, unsigned int len);
        virtual void handleValue(char* str, unsigned int len);
        virtual void handleObjStart();
        virtual void handleObjEnd();
        virtual void handleArrStart();
        virtual void handleArrEnd();
    };


    void compress(string infileName, string outfileName);
    
}

#endif
