//============================================================================
// Name        : HmmTokenize.hpp
// Author      : chenjy 
// Version     : v0.1
// Copyright   : sthtors@126.com
// Description : 
//============================================================================

#ifndef HMM_TOKENIZE_H
#define HMM_TOKENIZE_H

#include <fstream>
#include <tr1/unordered_map>
#include "StringUtil.hpp"

using namespace std::tr1;
using std::string;
using std::cout;
using std::endl;
using std::tr1::unordered_map;
namespace qaservice{
typedef unordered_map<uint32_t, double> EmitProbMap;
enum {B = 0, E = 1, M = 2, S = 3, STATUS_NUMS = 4};
const double MIN_DOUBLE = -3.14e+100;
class HmmTokenize{
  public:
  	HmmTokenize(const string& path):modelpath(path){
  	  if(!ModuleLoaded){
  	  	cout<<"start to load module..."<<endl;
  	  	LoadModel();
  	  	ModuleLoaded = true;
  	  	cout<<"module is loaded..."<<endl;
        cout<<endl;
  	  }
  	}
  	~HmmTokenize(){}
    void HmmCut(string& sentences,vector<string>& Data){
      vector<uint32_t> words;
      // Convert string to vector<unicode> words
      Normalize(sentences, words, true);
      HmmCut(words, Data);
    }
  	void HmmCut(vector<uint32_t>& words,vector<string>& Data){
      vector<size_t> status;
      size_t start = 0;
      size_t end = 0;
      vector<uint32_t> tmp;
      string str;
      if(words.size()==1){
        Normalize(str, words, false);
        Data.push_back(str);
        return;
      }
      // cout<<"start to cut sentence..."<<endl;
      // Get the max prob status for each word
  	  Viterbi(words, status);
  	  for(size_t i=0; i<status.size(); i++){
  	  	tmp.push_back(words[i]);
  	  	if(status[i]==E || status[i]==S){
  	  	  Normalize(str, tmp, false);
  	  	  Data.push_back(str);
  	  	  tmp.clear();
  	  	  str.clear();
  	  	}
  	  }
  	  // cout<<"sentence is cut..."<<endl;
  	}
  	void Viterbi(vector<uint32_t>& words, vector<size_t>& status){
      size_t WordSize = words.size();
      size_t totalSize = STATUS_NUMS * WordSize; 
      size_t now, old, stat;
      double tmpEmitProb, tmp, endE, endS;

      vector<int> path(totalSize);
      vector<double> weight(totalSize);
      // cout<<"WordSize="<<WordSize<<", STATUS_NUMS="<<STATUS_NUMS<<", totalSize="<<totalSize<<endl;
      // cout<<"viterbi ...\npath.size()="<<path.size()<<" weight.size()="<<weight.size()<<endl;
      //N*4 path (if current status is A, record max preStat)
      //N*4 weight (max prob for word&stat)
      for(size_t i=0; i<STATUS_NUMS; i++){
      	path[i*WordSize] = -1;
        weight[i*WordSize] = startProb[i] + GetObsProb(words[0], i);
      }

      for(size_t i=1; i<WordSize; i++){
      	for(size_t j=0; j<STATUS_NUMS; j++){
          now = i+j*WordSize;
          weight[now] = MIN_DOUBLE;
          path[now] = E;
          tmpEmitProb = GetObsProb(words[i], j);
          for(size_t preStat=0; preStat<STATUS_NUMS; preStat++){
          	old = i-1+preStat*WordSize;
            tmp = weight[old]+transProb[preStat][j]+tmpEmitProb;
            if(tmp>weight[now]){
              weight[now] = tmp;
              path[now] = preStat;
            }
          }
      	}
      }

      endE = weight[WordSize-1+E*WordSize];
      endS = weight[WordSize-1+S*WordSize];
      stat = 0;
      if(endE >= endS){
      	stat = E;
      } else {
      	stat = S;
      }
      status.resize(WordSize);
      for(int i=WordSize-1; i>=0; i--){
      	status[i] = stat;
      	stat = path[i+stat*WordSize];
      }
  	}

  private:
    EmitProbMap emitProbB;
    EmitProbMap emitProbE;
    EmitProbMap emitProbM;
    EmitProbMap emitProbS;
    EmitProbMap* emitProbArray[STATUS_NUMS] = {&emitProbB, &emitProbE, &emitProbM, &emitProbS};
    double transProb[STATUS_NUMS][STATUS_NUMS];
    double startProb[STATUS_NUMS];
    bool ModuleLoaded = false;
    const string modelpath;
    double GetObsProb(const uint32_t word, size_t emitSrc){
      unordered_map<uint32_t, double>::const_iterator got = emitProbArray[emitSrc]->find(word);
      if(got == emitProbArray[emitSrc]->end()){
      	return MIN_DOUBLE;
      } else {
      	return got->second;
      }
    }
    void LoadEmitProb(const string& line, EmitProbMap& prob){
  	  vector<string> tmp, tmp2;
  	  uint32_t word;  	  
      Split(line, tmp, ",");
      for(size_t i=0; i<tmp.size(); i++){
      	Split(tmp[i], tmp2, ":");
      	if(2 != tmp2.size()){
      		continue;
      	}
      	size_t start=0;
      	if(Utf8ToUnicode32(tmp2[0], word, &start)){
      	  prob[word] = atof(tmp2[1].c_str());
      	}
      }
  	}
  	void LoadModel(){
  	  ifstream ifs(modelpath);
  	  string line;
  	  vector<string> tmp;
  	  //load startPorb
  	  GetLine(ifs, line);
  	  Split(line, tmp, " ");
  	  for(size_t i=0; i<tmp.size(); i++){
  	  	startProb[i] = atof(tmp[i].c_str());
  	  }
  	  //load transProb
  	  for(size_t i=0; i<STATUS_NUMS; i++){
  	  	GetLine(ifs, line);
  	  	Split(line, tmp, " ");
        for(size_t j=0; j<STATUS_NUMS; j++){
          transProb[i][j] = atof(tmp[j].c_str());
        }
  	  }
  	  //load emitProbB
  	  GetLine(ifs, line);
  	  LoadEmitProb(line, emitProbB);
  	  //load emitProbE
  	  GetLine(ifs, line);
  	  LoadEmitProb(line, emitProbE);
  	  //load emitProbM
  	  GetLine(ifs, line);
  	  LoadEmitProb(line, emitProbM);
  	  //load emitProbS
  	  GetLine(ifs, line);
  	  LoadEmitProb(line, emitProbS);
  	}
};

}

#endif
