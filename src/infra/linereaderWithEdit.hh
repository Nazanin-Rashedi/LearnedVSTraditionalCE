#ifndef LINEREADER_WITH_EDIT_HH
#define LINEREADER_WITH_EDIT_HH

#include <stdint.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <string.h>
#include <string>
#include <set>
#include <vector>

#include "infra/simpleexception.hh"
#include "infra/decimal.hh"
#include "infra/val_tt.hh"
#include "infra/datejd.hh"
#include "infra/stime.hh"
#include "infra/stimestamp.hh"

class LineReaderWithEdit {
  public:
    typedef unsigned int uint;
    typedef std::set<std::string> string_st;
  private:
    LineReaderWithEdit(const LineReaderWithEdit&);
    LineReaderWithEdit& operator=(const LineReaderWithEdit&);
  public:
    LineReaderWithEdit(const char* aPrompt, char aCommentChar = '#');
    ~LineReaderWithEdit();
  public:
    bool open();
    bool next();
    void close();
  public:
    inline const char* prompt() const { return _prompt; }
    inline       char  commentchar() const { return _commentchar; }
    inline const char* line() const { return _line; }
    inline       uint  linesize() const { return _linesize; }
    inline       uint  commentlinecount() const { return _commentlinecount; }
    inline       uint  linecount() const { return _linecount; }
    inline       bool  ok() const { return _ok; }
    inline const char* begin() const { return _line; }
    inline const char* end() const { return (_line + _linesize); }
    inline       bool  isEmpty() const { return (begin() == end()); }
    inline       uint64_t no_bytes_read() const { return _no_bytes_read; }
  public:
    /* utilities */
    /* x is pointer into the linebuffer from whereon item is to be read/converted */
    /* the result will be written into the typed variable out */
    /* functions return true on success, false on failure */
    bool skipws(const char*& x);
    bool skipuntil(const char*& x, const char aCharToStopAt); // returns true if something was found
    bool skipuntil(const char*& x, const char aCharToStopAt, const char aCharEnd); // returns true if at end
    bool read_char(const char*& x, char& out);
    bool read_uint(const char*& x, unsigned int& out);
    bool read_int(const char*& x, int& out);
    bool read_float(const char*& x, float& out);
    bool read_double(const char*& x, double& out);
    bool read_ival(const char*& x, ival_t& out);
    bool read_uval(const char*& x, uval_t& out);
    bool read_fval(const char*& x, fval_t& out);
    bool read_dval(const char*& x, dval_t& out);
    bool read_stimestamp(const char*& x, STimestamp& out, char sep = ':');

    /* here, for strings, we need an additional delimiter for strings (e.g. '"') */
    /* the output is the b(egin) and e(nd) of the string (e: one after) */
    /* the string may not contain the delimiter!, not even with a backslash in front */
    bool read_string(const char*& x, char aDelimiter, const char*& b, const char*& e);
    bool read_string_no_delim(const char*& x, char aSep, const char*& b, const char*& e);

    bool read_string_set(const char*& x, string_st& aSetOut, const char aStringSep, const char aSep);

    inline bool read_datejd(const char*& x, DateJd& out, bool aYearHigh, char aSep) { return out.set(x, aYearHigh, aSep); }

    // the following destructively splits the _line at aSep and may perform stripping blanks
    int  split_line(const char aSep, const bool aStrip);
    const std::vector<char*> splits() const { return _splits; }
  private:
    void getNonCommentLine();
  private:
    const char*    _prompt;
    char           _commentchar;
    char*          _line;
    bool           _ok;
    uint           _linesize; // for successfully read lines: line()[linesize()] == '\0'
    uint           _linecount;
    uint           _commentlinecount;
    uint64_t       _no_bytes_read;
    uint           _noSplit;    // current number of splits
    std::vector<char*>  _splits;     // array for splits
};


#endif
