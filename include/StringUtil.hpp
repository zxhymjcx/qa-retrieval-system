//============================================================================
// Name        : StringUtil.hpp
// Author      : chenjy 
// Version     : v0.1
// Copyright   : sthtors@126.com
// Description : 
//============================================================================

#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <stdint.h>
#include <sys/types.h>

using std::string;
using std::vector;
using std::ifstream;
using std::ofstream;
using std::endl;
using std::stringstream;
namespace qaservice{
inline bool IsSpace(unsigned c) {
  // when passing large int as the argument of isspace, it core dump, so here need a type cast.
  return c > 0xff ? false : std::isspace(c & 0xff);
}

inline std::string& LTrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<unsigned, bool>(IsSpace))));
  return s;
}

inline std::string& RTrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<unsigned, bool>(IsSpace))).base(), s.end());
  return s;
}

inline std::string& Trim(std::string &s) {
  return LTrim(RTrim(s));
}

inline void Split(const string& src, vector<string>& res, const string& pattern) {
  res.clear();
  size_t Start = 0;
  size_t end = 0;
  string sub;
  while(Start < src.size()) {
    end = src.find_first_of(pattern, Start);
    if(string::npos == end) {
      sub = src.substr(Start);
      res.push_back(sub);
      return;
    }
    sub = src.substr(Start, end - Start);
    res.push_back(sub);
    Start = end + 1;
  }
  return;
}

inline bool StartsWith(const string& str, const string& prefix) {
  if(prefix.length() > str.length()) {
    return false;
  }
  return 0 == str.compare(0, prefix.length(), prefix);
}

inline bool EndsWith(const string& str, const string& suffix) {
  if(suffix.length() > str.length()) {
    return false;
  }
  return 0 == str.compare(str.length() -  suffix.length(), suffix.length(), suffix);
}

inline bool GetLine(ifstream& ifs, string& line){
  while(getline(ifs, line)){
    Trim(line);
    if(line.empty()){
      continue;
    }
    if(StartsWith(line, "#")){
      continue;
    }
    return true;
  }
  return false;
}

/*
   |  Unicode符号范围      |  UTF-8编码方式  
 n |  (十六进制)           | (二进制)  
---+-----------------------+------------------------------------------------------  
 1 | 0000 0000 - 0000 007F |                                              0xxxxxxx  
 2 | 0000 0080 - 0000 07FF |                                     110xxxxx 10xxxxxx  
 3 | 0000 0800 - 0000 FFFF |                            1110xxxx 10xxxxxx 10xxxxxx  
 4 | 0001 0000 - 0010 FFFF |                   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx  
 5 | 0020 0000 - 03FF FFFF |          111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx  
 6 | 0400 0000 - 7FFF FFFF | 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 
*/

bool Utf8ToUnicode32(string& str, uint32_t& word, size_t* start=0){
  size_t begin = *start;
  size_t len;
  uint8_t first;

  len = str.size() - begin;
  first = str.at(begin);
  if(len == 0){
    return false;
  }
  if(!(first&0x80) && len>=1){
    word = (uint32_t)(first&0x7f);
    begin = begin+1;
  } else if((first <= 0xdf) && (len>=2)){
    word = (uint32_t)(first&0x1f);
    word <<= 6;
    word |= (uint32_t)(((uint8_t)str.at(begin+1))&0x3f);
    begin = begin+2;
  } else if((first <= 0xef) && (len>=3)){
    word = (uint32_t)(first&0x0f);
    word <<= 6;
    word |= (uint32_t)(((uint8_t)str.at(begin+1))&0x3f);
    word <<= 6;
    word |= (uint32_t)(((uint8_t)str.at(begin+2))&0x3f);
    begin = begin+3;
  } else if((first <= 0xf7) && (len>=4)){
    word = (uint32_t)(first&0x07);
    word <<= 6;
    word |= (uint32_t)(((uint8_t)str.at(begin+1))&0x3f);
    word <<= 6;
    word |= (uint32_t)(((uint8_t)str.at(begin+2))&0x3f);
    word <<= 6;
    word |= (uint32_t)(((uint8_t)str.at(begin+3))&0x3f);
    begin = begin+4;
  } else {
    return false;
  }
  *start = begin;
  return true;
}

bool Unicode32ToUtf8(string& str, uint32_t& word){
  str.clear();
  if(word <= 0x7f){
    str += char(word);
  } else if (word <= 0x7ff){
    str += char(((word >> 6) & 0x1f) | 0xc0);
    str += char((word & 0x3f) | 0x80);
  } else if (word <= 0xffff){
    str += char(((word >> 12) & 0x0f) | 0xe0);
    str += char(((word >> 6) & 0x3f) | 0x80);
    str += char((word & 0x3f) | 0x80);
  } else {
    str += char(((word >> 18) & 0x03) | 0xf0);
    str += char(((word >> 12) & 0x3f) | 0x80);
    str += char(((word >> 6) & 0x3f) | 0x80);
    str += char((word & 0x3f) | 0x80);
  }
  return true;
}

bool filter(uint32_t& word){
  //(code < 0x3400 || code > 0x4db5)
  //"([\u4E00-\u9FD5a-zA-Z0-9+#&\._%\-]+)"
  if((word >= 0x4e00 && word <= 0x9fef) || (word >= 0x61 && word <= 0x7a) || 
    (word >= 0x30 && word <= 0x39) || (word >= 0x41 && word <= 0x5a)){
    return false;
  }
  return true;
}

void Normalize(string& sentences, vector<uint32_t>& words, bool ToUnicode){
  string::iterator it;
  size_t start=0;
  uint32_t wd;

  if(ToUnicode){
    while(start != sentences.size()){
      if(Utf8ToUnicode32(sentences, wd, &start)){
        words.push_back(wd);
      } else {
        return;
      }
    }
  } else {
    sentences.clear();
    for(vector<uint32_t>::iterator it=words.begin(); it!=words.end(); ++it)
    {
      string str;
      if(Unicode32ToUtf8(str, *it)){
        sentences += str;
      } else {
        return;
      }
    }
  }
}

void splitWords(const string& sentences, vector<vector<uint32_t> >& words)
{
  vector<uint32_t> tmp;
  vector<uint32_t> wd;
  string ss = sentences;
  Normalize(ss, tmp, true);
  for(auto& it: tmp){
    if(!filter(it)){
      wd.push_back(it);
    } else {
      if(wd.size()>=1){
        words.push_back(wd);
        wd.clear();
      }      
    }
  }
  if(wd.size()>0){
    words.push_back(wd);
  }
}

void printToFile(vector<string> Data){
  ofstream ofs("data.log");
  for(vector<string>::iterator it=Data.begin(); it!=Data.end(); ++it)
  {
    ofs<<*it<<"......."<<endl;
  }
  ofs.close();
}

string readFromFile(const string filepath){
  ifstream ifs(filepath.c_str());
  stringstream buffer;
  buffer<<ifs.rdbuf();
  string ss(buffer.str());
  return ss;
  ifs.close();
}

}
#endif
