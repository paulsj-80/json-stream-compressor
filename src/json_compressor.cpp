#include "json_compressor.h"

namespace io = boost::iostreams;

#define INT_SIZE sizeof(int)
#define FLOAT_SIZE sizeof(float)

namespace jsc {


    // TODO: possible to make buffer here for better performance
    void Compressor::serialize_char(char c) {
        sinkWrite(&c, 1);
    }
    void Compressor::serialize_chars(char* s, int n) {
        sinkWrite(s, n);
    }

    void Compressor::serialize_int_val(int num,
                                       SerializedConstants sconst) {
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
    void Compressor::serialize_float_val(float f) {
        serialize_char(T_FLOAT | FLOAT_SIZE);
        char bytes[FLOAT_SIZE];
        memset((void*)bytes, 0, FLOAT_SIZE);
        memcpy(bytes, (void*)&f, FLOAT_SIZE);
        for (int i = 0; i < FLOAT_SIZE; i++) {
            serialize_char(bytes[i]);                
        }
    }
    void Compressor::serialize_str_val(char* s, int n) {
        serialize_int_val(n, T_STRING);
        serialize_chars(s, n);
    }

    float Compressor::parse_float(char* s, int n) {
        if (n > JSC_NUMBER_BUFF_SIZE - 1)
            throw runtime_error("number buffer overflow");
        char buff[JSC_NUMBER_BUFF_SIZE];
        memcpy((void*)buff, (void*)s, n);
        buff[n] = 0;
        float res;
        sscanf(buff, "%f", &res);
        return res;
    }

    int Compressor::parse_int(char* s, int n) {
        if (n > JSC_NUMBER_BUFF_SIZE - 1)
            throw runtime_error("number buffer overflow");
        char buff[JSC_NUMBER_BUFF_SIZE];
        memcpy((void*)buff, (void*)s, n);
        buff[n] = 0;
        int res;
        sscanf(buff, "%d", &res);
        return res;
    }

    void Compressor::serialize(char* s, streamsize n) {

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
        
    Compressor::Compressor() : buffLen(0), parser(this, 2) {
        memset(buff, 0, JSC_LINE_BUFF_SIZE); 
    }
    Compressor::Compressor(Compressor const& other) :
        buffLen(other.buffLen), parser(other.parser) {
        memcpy(buff, other.buff, JSC_LINE_BUFF_SIZE);
        parser.setListener(this);
    }
    Compressor& Compressor::operator=(const Compressor& other) {
        buffLen = other.buffLen;
        parser = other.parser;
        parser.setListener(this);
        memcpy(buff, other.buff, JSC_LINE_BUFF_SIZE);
        return *this;
    }
    Compressor::~Compressor() {
    }

    // TODO: fix situation when n > JSC_LINE_BUFF_SIZE
    // TODO: what happens if file doesn't have newline in the
    // end?
    template<typename Sink>
    streamsize Compressor::write(Sink& dest, const char* s,
                                 streamsize n)
    {


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
            try {
                // ignoring bad lines for now
                parser.feed(buff, llen);
            } catch (const exception& ex) {
                cerr << ex.what() << endl;
            }

            // TODO: this is not safe yet; should finish
            // any started token
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
    void Compressor::close(Source&) {
        parser.feeding_done();
        serialize_char(START_DICTIONARY);
        for (auto& kv : keyDictionary) {
            serialize_str_val((char*)kv.first.c_str(),
                              (int)kv.first.length());
            serialize_int_val(kv.second, T_INT);
        }
    }

    void Compressor::handleKey(char* str, unsigned int len) {
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
    void Compressor::handleValue(char* str, unsigned int len) {
        serialize(str, len);
    }
    void Compressor::handleObjStart() {
        // nothing here
    }
    void Compressor::handleObjEnd() {
        // nothing here
    }
    void Compressor::handleArrStart() {
        // nothing here
    }
    void Compressor::handleArrEnd() {
        // nothing here
    }



    // TODO: should align size of buffers, probably

    void compress(string infileName, string outfileName) {

        io::filtering_ostream out;
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


