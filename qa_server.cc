//============================================================================
// Name        : qa_server.cc
// Author      : chenjy 
// Version     : v0.1
// Copyright   : sthtors@126.com
// Description : 
//============================================================================

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "qa.grpc.pb.h"
#include "include/HmmTokenize.hpp"
#include "include/DictTokenize.hpp"
#include "include/word2vec.hpp"
#include "include/vectorCal.h"

#define HMM_MODEL_PATH "include/dict/hmm_model.utf8"
#define DICT_FILE_PATH "include/dict/jieba.dict.utf8"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using qa::wordVector;
using qa::sentenceVector;
using qa::wordVectorList;
using qa::sentenceVectorList;
using qa::docFreqVector;
using qa::docFreqVectorList;
using qa::docIdVector;
using qa::docIdVectorList;
using qa::QARequest;
using qa::QARequest2;
using qa::QAReply;
using qa::QAReply2;
using qa::QuaAndAns;
using namespace std::chrono;
using std::string;

qaservice::DictTokenize dictCut("include/dict/jieba.dict.utf8");

std::unordered_map<string, vector<float>> vocab_p;
std::vector<std::vector<float> > model_vector;
std::unordered_map<int, float> top5;
std::unordered_map<string, float> idocFreq;
std::unordered_map<string, vector<int32_t>> docIds;
std::unordered_map<string, int32_t>  docFreq;

std::string vocabFilePath = "vocab.mod";
std::string modelFilePath = "model.mod";
std::string idfFilePath = "idf.mod";
std::string docIdFilePath = "docId.mod";

#define LAYER_SIZE 64

void getTfidf(vector<string>& words, std::unordered_map<string, float>& weight){
    int nums = 0;
    std::unordered_map<string, int> termFreq;
    float sum = 0;
    for(auto& w: words){
      auto wd = vocab_p.find(w);
      if(wd != vocab_p.end()){
        termFreq[w]+=1;
        ++nums;
      }
    }
    for(auto& w: termFreq){   
      weight[w.first] = ((w.second)/(float)nums)*idocFreq[w.first];
      sum += weight[w.first];
    }
    for(auto& w: termFreq){
      weight[w.first] = weight[w.first]/sum;
    }
    return;
}

void getVec(vector<string>& words, vector<float>& vec){
    std::unordered_map<string, float> weight;
    getTfidf(words, weight);
    for(auto& w: weight){
      int i = 0;
      for(auto& v: vocab_p[w.first]){
        vec[i] += v*weight[w.first];
        ++i;
      }
    }
    return;
}

int getAnswerIndex(const string& ss){
  std::vector<string> words;
  std::unordered_map<string, float> weight;
  vector<float> sv(LAYER_SIZE);
  std::unordered_map<int, float> IdScore;
  int index=0;
  float tmp,tmp1,tmp2,maxValue,maxscore;
  std::vector<int> maxIds;

  //auto cstart = std::chrono::high_resolution_clock::now();
  dictCut.DictCut(ss, words);
  getTfidf(words, weight);
  for(auto& w:weight){
    for(auto& id: docIds[w.first]){
      IdScore[id]+=w.second;
    }
  }
  getVec(words, sv);
  maxscore = 0;
  for(auto& score: IdScore){
    if(score.second > maxscore){
      maxscore = score.second;
    }
  }
  for(auto& score: IdScore){
    if(maxscore == score.second){
      maxIds.push_back(score.first);
    }
  }
  cout<<"maxIds=";
  maxValue = 0;
  for(int i=0; i<maxIds.size(); ++i){
    cout<<maxIds[i]<<" ";
    tmp = vectorCal::dot(sv, model_vector[maxIds[i]]);
    tmp1 = sqrt(vectorCal::dot(model_vector[maxIds[i]], model_vector[maxIds[i]]));
    tmp2 = sqrt(vectorCal::dot(sv, sv));
    tmp = tmp/(tmp1*tmp2);
    if(tmp > maxValue){
      maxValue = tmp;
      index = maxIds[i];
    }
  }
  cout<<";"<<std::endl;
  //auto cend = std::chrono::high_resolution_clock::now();
  //auto duration = std::chrono::duration_cast<std::chrono::microseconds>(cend - cstart).count();
  return index;
}

bool addVocab(vector<string>& input, int32_t doc_id){
  std::set<string> tmp;
  if(input.empty()){
    return false;
  }
  for(auto& w: input){
    tmp.insert(w);
  }
  for(auto& w: tmp){
    docFreq[w]+=1;
    docIds[w].push_back(doc_id);
  }
}

void save_model(){
  wordVector* wv = new wordVector();
  wordVectorList wvList;
  sentenceVector* sv = new sentenceVector();
  sentenceVectorList svList;
  docFreqVector* idf = new docFreqVector();
  docFreqVectorList idfList;
  docIdVector* docIdVec = new docIdVector();
  docIdVectorList docIdList;
  int fdv = open(vocabFilePath.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0664);
  int fdm = open(modelFilePath.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0664);
  int fdi = open(idfFilePath.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0664);
  int fdd = open(docIdFilePath.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0664);
  std::cout<<"start to save model: ";
  for(auto& v: vocab_p){
    wv = wvList.add_wvec();
    wv->set_word(v.first);
    for(auto& x: v.second){
      wv->add_score(x);
    }
  }
  wvList.SerializeToFileDescriptor(fdv);

  for(auto& v: model_vector){
    sv = svList.add_svec();
    for(auto&x: v){
      sv->add_score(x);
    }
  }
  svList.SerializeToFileDescriptor(fdm);

  for(auto& v: idocFreq){
    idf = idfList.add_idf();
    idf->set_word(v.first);
    idf->set_freq(v.second);
  }
  idfList.SerializeToFileDescriptor(fdi);

  for(auto& v: docIds){
    docIdVec = docIdList.add_didvec();
    docIdVec->set_word(v.first);
    for(auto& x: v.second){
      docIdVec->add_docid(x);
    }
  }
  docIdList.SerializeToFileDescriptor(fdd);
  std::cout<<"success"<<std::endl;
}

void restore_model(){
  std::vector<float> v_;
  wordVector wv;
  wordVectorList wvList;
  sentenceVector sv;
  sentenceVectorList svList;
  docFreqVector idf;
  docFreqVectorList idfList;
  docIdVector docIdVec;
  docIdVectorList docIdList;
  int32_t tmp;
  std::vector<int32_t> d_;
  std::cout<<"start to restore model: ";
  int fdv = open(vocabFilePath.c_str(), O_RDWR, 0664);
  int fdm = open(modelFilePath.c_str(), O_RDWR, 0664);
  int fdi = open(idfFilePath.c_str(), O_RDWR, 0664);
  int fdd = open(docIdFilePath.c_str(), O_RDWR, 0664);

  wvList.ParseFromFileDescriptor(fdv);
  for(tmp=0; tmp<wvList.wvec_size(); tmp++){
    wv = wvList.wvec(tmp);
    for(auto& x: wv.score()){
      v_.push_back(x);
    }
    vocab_p.emplace(wv.word(), std::move(v_));
    v_.clear();
  }

  svList.ParseFromFileDescriptor(fdm);
  for(tmp=0; tmp<svList.svec_size(); tmp++){
    sv = svList.svec(tmp);
    for(auto& x: sv.score()){
      v_.push_back(x);
    }
    model_vector.push_back(std::move(v_));
    v_.clear();
  }

  idfList.ParseFromFileDescriptor(fdi);
  for(tmp=0; tmp<idfList.idf_size(); tmp++){
    idf = idfList.idf(tmp);
    idocFreq.emplace(idf.word(), idf.freq());
  }

  docIdList.ParseFromFileDescriptor(fdd);
  for(tmp=0; tmp<docIdList.didvec_size(); tmp++){
    docIdVec = docIdList.didvec(tmp);
    for(auto& x: docIdVec.docid()){
      d_.push_back(x);
    }
    docIds.emplace(docIdVec.word(), std::move(d_));
    d_.clear();
  }

  std::cout<<"success"<<std::endl;
}

class QuaAndAnsServiceImpl final : public QuaAndAns::Service {
  Status BuildModel(ServerContext* context, const QARequest* request, QAReply* reply) override {
    qaservice::Word2Vec cvt2Vec(LAYER_SIZE);
    std::vector<string> words;
    int nums = 0;
    if(request->filepath() == ""){
      reply->set_isok(false);
    } else {
      std::cout<<"build model...... "<<request->filepath()<<std::endl;
      ifstream ifs((request->filepath()).c_str());
      string line;
      std::vector<string> words;
      std::vector<float> vec(LAYER_SIZE);
      for(size_t i=0; getline(ifs, line); i++){
        dictCut.DictCut(line, words);
        cvt2Vec.addVocab(words);
        addVocab(words, nums);
        words.clear();
        ++nums;
      }
      for(auto& w: docFreq){
        idocFreq[w.first] = log(nums/(1+w.second));
      }
      if(cvt2Vec.buildModel()){
        for(auto& v: cvt2Vec.vocab_p){
          vocab_p.emplace(v.first, std::move(cvt2Vec.syn0_[(v.second)->index_]));
        }
        model_vector.clear();
        for(auto& s: cvt2Vec.sentences){
          getVec(s, vec);
          model_vector.push_back(vec);
        }
        save_model();
        reply->set_isok(true);
      } else {
        reply->set_isok(false);
      }
    }
    return Status::OK;
  }
  Status GetTop5(ServerContext* context, const QARequest2* request, QAReply2* reply) override {
    if(vocab_p.empty() || model_vector.empty() || idocFreq.empty() || docIds.empty()){
      restore_model();
    }
    reply->set_index(getAnswerIndex(request->content()));
    return Status::OK;
  }
};

void RunServer(){
  //std::string server_address("192.168.0.105:50051");
  std::string server_address("172.20.10.12:50051");
  QuaAndAnsServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout<<"Server listening on "<<server_address<<std::endl;
  server->Wait();
}

int main(){
  RunServer();
  return 0;
}
