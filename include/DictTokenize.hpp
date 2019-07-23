//============================================================================
// Name        : DictTokenize.hpp
// Author      : chenjy 
// Version     : v0.1
// Copyright   : sthtors@126.com
// Description : 
//============================================================================

#ifndef DICT_TOKENIZE_H
#define DICT_TOKENIZE_H

#include <tr1/unordered_map>
#include <cmath>
#include "StringUtil.hpp"

using std::make_pair;
using std::shared_ptr;
using std::string;
namespace qaservice{

class TrieNode{
public:
  TrieNode():next(NULL), weight(1){}
  typedef unordered_map<uint32_t, TrieNode*> NextMap;
  NextMap* next;
  int32_t weight;
};

class DictTokenize
{
  public:
    DictTokenize(const string path):DictFilePath(path){
      if(!DictLoaded){
      	CreateTrie();
        DictLoaded = true;
        hmmCut = new HmmTokenize("sinclude/dict/hmm_model.utf8");
        cout<<"dict loaded, total is "<<total<<endl;
      }
    }
    ~DictTokenize(){
      DeleteNode(root);
    }
    HmmTokenize* hmmCut;
    // 0 1 2 3 4
    void DictCut(const string& sentence, vector<string>& Data){
      vector<vector<uint32_t> > words;
      splitWords(sentence, words);
      for(auto& it: words){
      	DictCut(it, Data);
      }
    }
    void DictCut(vector<uint32_t>& words,vector<string>& Data){
      vector<uint32_t> tmp;
      vector<Token> p;
      double tprob;
      int tvalue;
      Token ttoken;
      string tstr;
      double max;
      if(words.size()<1){
      	return;
      }
      if(words.size()==1){
      	string str;
      	Normalize(str, words, false);
      	Data.push_back(str);
      	return;
      }
      p.resize(words.size()+1);
      for(int i=words.size()-1; i>=0; i--){
        for(int j=0; j<words.size()-i; j++){
          tmp.push_back(words[i+j]);
          if(GetRouteValue(tmp, tvalue)){
            tprob = log((double)tvalue/total)+p[i+j+1].Prob;
            // cout<<" GetRouteValue=true"<<", tprob="<<tprob<<", max="<<max<<", p[i+j+1].Prob="<<p[i+j+1].Prob<<endl;
          } else {
            tprob = (j+1)*log((double)tvalue/total)+p[i+j+1].Prob;
            // cout<<" GetRouteValue=false"<<", tprob="<<tprob<<", max="<<max<<endl;
          }
          // cout<<"i = "<<i<<", j = "<<j<<", weight = "<<tvalue<<", prob = "<<tprob<<endl;
          if(tprob>max || j==0){
            p[i].Prob = tprob;
            p[i].CutLoc = j;
            max = tprob;
          }
        }
        // cout<<"i = "<<i<<", j = "<<p[i].CutLoc<<", weight = "<<p[i].Prob<<endl;
        tmp.clear();
      }
      vector<uint32_t> unknown;
      vector<uint32_t> digit;
      vector<uint32_t> english;
      for(int i=0; i<words.size(); i++){
        // cout<<"i = "<<i<<", p[i].CutLoc = "<<p[i].CutLoc<<endl;
        if((words[i] >= 0x61 && words[i] <= 0x7a) || (words[i] >= 0x41 && words[i] <= 0x5a)){
          CutDigit(digit, Data);
          CutUnknown(unknown, Data);
          english.push_back(words[i]);
          continue;
        }
        if((words[i] >= 0x30 && words[i] <= 0x39)){
          CutUnknown(unknown, Data);
          CutEnglish(english, Data);
          digit.push_back(words[i]);
          continue;
        }
        if(p[i].CutLoc == 0){
          CutDigit(digit, Data);
          CutEnglish(english, Data);
          unknown.push_back(words[i]);
          continue;
        }
        // cout<<"size of unkonwn = "<<unknown.size()<<endl;
        CutDigit(digit, Data);
        CutUnknown(unknown, Data);
        CutEnglish(english, Data);
        for(int j=0; j<=p[i].CutLoc; j++){
          tmp.push_back(words[i+j]);
        }
        i += p[i].CutLoc;
        Normalize(tstr, tmp, false);
        Data.push_back(tstr);
        tstr.clear();
        tmp.clear();
      }
      CutUnknown(unknown, Data);
      CutDigit(digit, Data);
      CutEnglish(english, Data);
    }
    void CutEnglish(vector<uint32_t>& english, vector<string>& Data){
      string str;
      str.clear();
      if(english.size()>=1){
        Normalize(str, english, false);
        Data.push_back(str);
      }
      english.clear();
    }
    void CutDigit(vector<uint32_t>& digit, vector<string>& Data){
      string str;
      str.clear();
      if(digit.size()>=1){
        Normalize(str, digit, false);
        Data.push_back(str);
      }
      digit.clear();
    }
    void CutUnknown(vector<uint32_t>& unknown, vector<string>& Data){
      vector<string> hmmWords;
      if(unknown.size()>=1){
        // for(auto& it: unknown) cout<<it<<" ";
        hmmCut->HmmCut(unknown, hmmWords);
        Data.insert(Data.end(), hmmWords.begin(), hmmWords.end());
        hmmWords.clear();
        unknown.clear();
      }    	
    }
  private:
  	string DictFilePath;
  	bool DictLoaded=false;
    TrieNode* root=new TrieNode();
    int total = 0;
    struct Token {
      size_t CutLoc;
      double Prob; // TODO
      Token():CutLoc(0), Prob(0) {
      }
    };
    bool GetRouteValue(vector<uint32_t> words, int& value){
      TrieNode* pNode = root;
      unordered_map<uint32_t, TrieNode*>::const_iterator tmp;
      for(vector<uint32_t>::iterator it=words.begin(); it!=words.end(); ++it){
        if(pNode->next == NULL){
          value = 1;
          return false;
        }
        tmp = pNode->next->find(*it);
        if(pNode->next->end() == tmp){
          value = 1;
          return false;
        } else {
          pNode = tmp->second;
        }
      }
      value = pNode->weight;
      return true;
    }
    void InsertNode(string& sentence, int value){
      vector<uint32_t> words;
      TrieNode* pNode = root;
      unordered_map<uint32_t, TrieNode*>::const_iterator tmp;
      Normalize(sentence, words, true);
      for(vector<uint32_t>::iterator it=words.begin(); it!=words.end(); ++it){
        if(pNode->next == NULL){
          pNode->next = new TrieNode::NextMap;
        }
        tmp = pNode->next->find(*it);
        if(pNode->next->end() == tmp){
          TrieNode* NextNode = new TrieNode();
          pNode->next->insert(make_pair(*it, NextNode));
          pNode = NextNode;
        } else {
          pNode = tmp->second;
        }
      }
      pNode->weight = value;
      total += value;
    }
  	void CreateTrie(){
  	  ifstream ifs(DictFilePath.c_str());
  	  string line;
  	  vector<string> strs;
      for(size_t i=0; GetLine(ifs, line); i++){
        Split(line, strs, " ");
        InsertNode(strs[0], atoi(strs[1].c_str()));
      }
  	}
    void DeleteNode(TrieNode* node) {
      if (NULL == node) {
        return;
      }
      if (NULL != node->next) {
        for (TrieNode::NextMap::iterator it = node->next->begin(); it != node->next->end(); ++it) {
          DeleteNode(it->second);
        }
        delete node->next;
      }
      delete node;
    }
};
}

#endif
