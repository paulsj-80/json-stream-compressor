#include "json_compressor.h"
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

#define INT_SIZE sizeof(int)
#define FLOAT_SIZE sizeof(float)

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


        // possible to make buffer here for better performance
        inline void serialize_char(char c) {
            sinkWrite(&c, 1);
        }
        inline void serialize_chars(char* s, int n) {
            sinkWrite(s, n);
        }

        void serialize_int_val(int num, SerializedConstants sconst) {
            char byte_count = -1;
            char bytes[INT_SIZE];
            memset((void*)bytes, 0, INT_SIZE);
            int fn = 0xff << (8 * (INT_SIZE - 1));
            for (int i = 0; i < INT_SIZE; i++) {
                bytes[i] = (num & fn) >> ((INT_SIZE - i - 1) * 8);
                fn = fn >> 8;
                if (byte_count == -1 && bytes[i])
                    byte_count = INT_SIZE - i;
            }
            if (byte_count == -1)
                byte_count = 1; // supporting num == 0
            
            serialize_char(sconst | byte_count);
            for (int i = INT_SIZE - byte_count; i < INT_SIZE; i++)
                serialize_char(bytes[i]);
        }
        void serialize_float_val(float f) {
            serialize_char(T_FLOAT | FLOAT_SIZE);
            char bytes[FLOAT_SIZE];
            memset((void*)bytes, 0, FLOAT_SIZE);
            memcpy(bytes, (void*)&f, FLOAT_SIZE);
            for (int i = 0; i < FLOAT_SIZE; i++) {
                serialize_char(bytes[i]);                
            }
        }
        void serialize_str_val(char* s, int n) {
            serialize_int_val(n, T_STRING);
            serialize_chars(s, n);
        }

        static float parse_float(char* s, int n) {
            if (n > JSC_NUMBER_BUFF_SIZE - 1)
                throw runtime_error("number buffer overflow");
            char buff[JSC_NUMBER_BUFF_SIZE];
            memcpy((void*)buff, (void*)s, n);
            buff[n] = 0;
            float res;
            sscanf(buff, "%f", &res);
            return res;
        }

        static int parse_int(char* s, int n) {
            if (n > JSC_NUMBER_BUFF_SIZE - 1)
                throw runtime_error("number buffer overflow");
            char buff[JSC_NUMBER_BUFF_SIZE];
            memcpy((void*)buff, (void*)s, n);
            buff[n] = 0;
            int res;
            sscanf(buff, "%d", &res);
            return res;
        }

        void serialize(char* s, streamsize n) {

            switch (s[0]) {
            case '"':
                serialize_str_val(s + 1, n - 2);
                break;
            case 't':
                serialize_char(T_BOOL | 1);
                break;
            case 'f':
                serialize_char(T_BOOL | 0);
                break;
            case 'n':
                serialize_char(T_NULL);
                break;
            default:
                if (s[0] >= '0' && s[0] < '9' || s[0] == '-' ||
                    s[0] == '.') {
                    if (memchr((void*)s, '.', n)) {
                        serialize_float_val(parse_float(s, n));
                    } else {
                        serialize_int_val(parse_int(s, n), T_INT);
                    }
                }
                break;
            }
        }
    public:

        
        Compressor() : buffLen(0), parser(this, 2) {
            memset(buff, 0, JSC_LINE_BUFF_SIZE);
        }
        Compressor(Compressor const& other) : buffLen(other.buffLen), parser(other.parser) {
            memcpy(buff, other.buff, JSC_LINE_BUFF_SIZE);
            parser.setListener(this);
        }
        Compressor& operator=(const Compressor& other) {
            buffLen = other.buffLen;
            parser = other.parser;
            parser.setListener(this);
            memcpy(buff, other.buff, JSC_LINE_BUFF_SIZE);
            return *this;
        }
        virtual ~Compressor() {
        }
    public:

        template<typename Sink>
        streamsize write(Sink& dest, const char* s, streamsize n)
        {
            // TODO: finish this
            //            throw std::ios_base::failure("dirsa");
            //            throw exception();
            // TODO: what happens if file doesn't have newline in the end?
            // TODO: what if n > JSC_LINE_BUFF_SIZE?...
            // TODO: catch exceptions for line

            streamsize bytesToCopy =
                n + buffLen > JSC_LINE_BUFF_SIZE ?
                JSC_LINE_BUFF_SIZE - buffLen : n;

            if (bytesToCopy == 0)
                throw runtime_error("buffer overflow");
            
            memcpy(buff + buffLen, s, bytesToCopy);
            buffLen += bytesToCopy;

            sinkWrite = [&dest](char* buff, streamsize n) {
                io::write(dest, buff, n);
            };

            char* newLine = (char*)memchr((void*)buff, 10, buffLen);

            while (newLine) {
                // newline included
                streamsize llen = newLine - buff + 1; 
                parser.reset();
                parser.feed(buff, llen);
                serialize_char(END_RECORD);
                memcpy(buff, buff + llen, buffLen - llen);
                buffLen = buffLen - llen;
                memset(buff + buffLen, 0, JSC_LINE_BUFF_SIZE -
                       buffLen);
                newLine = (char*)memchr((void*)buff, 10, buffLen);
            }
            return n;
        }

        template<typename Source>
        void close(Source&) {
            parser.feeding_done();
            serialize_char(START_DICTIONARY);
            for (auto& kv : keyDictionary) {
                serialize_str_val((char*)kv.first.c_str(),
                                  (int)kv.first.length());
                serialize_int_val(kv.second, T_INT);
            }
        }

    public:
        virtual void handleKey(char* str, unsigned int len) {
            // str + (len - 1) itself is excluded
            string keyString(str + 1, str + (len - 1)); 
            auto it = keyDictionary.find(keyString);

            int key = 0;
            if (it != keyDictionary.end()) {
                key = it->second;
            } else {
                key = keyDictionary.size() + 1;
                keyDictionary[keyString] = key;
            }

            serialize_int_val(key, T_INT);
        }
        virtual void handleValue(char* str, unsigned int len) {
            serialize(str, len);
        }
        virtual void handleObjStart() {
            // nothing here
        }
        virtual void handleObjEnd() {
            // nothing here
        }
        virtual void handleArrStart() {
            // nothing here
        }
        virtual void handleArrEnd() {
            // nothing here
        }
    };


    // TODO: why "make" causes recompilation always?...
    // TODO: should align size of buffers, probably

    void compress(string infileName, string outfileName) {

        io::filtering_ostream out;
        // TODO : finish this FUCKING SHIT
        //        out.exceptions(ifstream::failbit | ifstream::badbit);
        //        out.exceptions(std::ios::failbit | std::ios::badbit);

        out.push(Compressor());
        out.push(boost::iostreams::file_sink(outfileName));

        try {
            string line;
            ifstream infile;
            infile.open(infileName);
            char data[1024];
            unsigned int bytesRead = 0;
            do {
                infile.read(data, 1024);
                bytesRead = infile.gcount();
                out.write(data, bytesRead);
            } while (bytesRead > 0);
        } catch (const exception& ex) {
            cerr << ex.what() << endl;
        }
    }
}


