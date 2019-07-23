//============================================================================
// Name        : word2vec.hpp
// Author      : chenjy 
// Version     : v0.1
// Copyright   : sthtors@126.com
// Description : 
//============================================================================

#ifndef WORD_TO_VECTOR_H
#define WORD_TO_VECTOR_H

#include <unordered_map>
#include <math.h>
#include <memory>
#include <vector>
#include <list>
#include <stdio.h>
#include "vectorCal.h"

using std::tuple;
using std::max;
using std::make_tuple;
using std::default_random_engine;
using std::get;
using std::uniform_real_distribution;
using std::min;

namespace qaservice{
struct Word
{
  int32_t index_;
  string text_;
  uint32_t count_;
  Word *left_, *right_;
	
  vector<uint8_t> codes_;
  vector<uint32_t> points_;
	
  Word(int32_t index, string text, uint32_t count, Word *left = 0, Word *right = 0) : index_(index), text_(text), count_(count), left_(left), right_(right) {}
  Word(const Word&) = delete;
  const Word& operator = (const Word&) = delete;
};
typedef shared_ptr<Word> WordP;

class Word2Vec{
  public:
    Word2Vec(int size=64, int window = 5, float alpha = 0.025, float min_alpha = 0.0001)
            :layer_size_(size), window_(window), alpha_(alpha), min_alpha_(min_alpha){}
    ~Word2Vec(){}
    bool addVocab(vector<string>& input){
      if(input.empty()){
        return false;
      }
      for(auto& w: input){
        vocab_[w]+=1;
      }
      sentences.push_back(input);
    }
    void CreateHaffmanTree(){
      n_words_ = vocab_.size();
      words_.reserve(n_words_); 
      auto comp = [](Word *w1, Word *w2) { return w1->count_ > w2->count_; };   
      for(auto& p: vocab_){
        auto r = vocab_p.emplace(p.first, WordP(new Word{0, p.first, p.second}));
        words_.push_back((r.first->second.get()));
      }
      sort(words_.begin(), words_.end(), comp);
      int32_t index=0;
      for(auto& w: words_){
        w->index_ = index++;
      }
      vector<Word *> heap = words_;
      make_heap(heap.begin(), heap.end(), comp);

      for(int i=0; i<n_words_-1; ++i){
        pop_heap(heap.begin(), heap.end(), comp);
        auto min1 = heap.back(); heap.pop_back();
        pop_heap(heap.begin(), heap.end(), comp);
        auto min2 = heap.back(); heap.pop_back();
        heap.push_back(new Word{i+n_words_, "", min1->count_+min2->count_, min1, min2});
        push_heap(heap.begin(), heap.end(), comp);
      }

      int max_depth = 0;
      int count = 0;
      std::list<tuple<Word *, vector<uint32_t>, vector<uint8_t> >> stack;
      stack.push_back(make_tuple(heap[0], vector<uint32_t>(), vector<uint8_t>()));
      while(!stack.empty()){
        auto t = stack.back();
        stack.pop_back();
        Word *word = get<0>(t);
        if(word->index_ < n_words_){
          word->points_ = get<1>(t);
          word->codes_ = get<2>(t);
          max_depth = max((int)word->codes_.size(), max_depth);
        } else {
          auto points = get<1>(t);
          points.emplace_back(word->index_ - n_words_);
          auto codes1 = get<2>(t);
          auto codes2 = codes1;
          codes1.push_back(0);
          codes2.push_back(1);
          stack.emplace_back(make_tuple(word->left_, points, codes1));
          stack.emplace_back(make_tuple(word->right_, points, codes2));
        }
      }

      syn0_.resize(n_words_);
      syn1_.resize(n_words_);
      default_random_engine eng(::time(NULL));
      uniform_real_distribution<float> rng(0.0, 1.0);
      for(auto& s: syn0_){
        s.resize(layer_size_);
        for(auto& x: s) x = (rng(eng) - 0.5) / layer_size_;
      }
      for(auto& s: syn1_) s.resize(layer_size_);
    }

    int train(){
      int total_words = 0;
      for(auto& p: vocab_){
        total_words += p.second;
      }
      float alpha0 = alpha_, min_alpha = min_alpha_;
      int max_size = 1000;
      float max_exp = 6.0;
      const static vector<float> table = [&](){
        vector<float> x(max_size);
        for (size_t i=0; i<max_size; ++i) { float f = exp( (i / float(max_size) * 2 -1) * max_exp); x[i] = f / (f + 1); }
        return x;
      }();
      
      default_random_engine eng(::time(NULL));
      uniform_real_distribution<float> rng(0.0, 1.0);

      int n_sentences = sentences.size();
      printf("training %d sentences\n", n_sentences);

      #pragma omp parallel for
      int count = 0;
      for(int i=0; i<n_sentences; ++i){
        auto sentence = sentences[i];
        int len = sentence.size();
        //cout<<"i = "<<i<<", size of sentence = "<<len<<endl;
        int reduced_window = rand() % window_;
        float alpha = max(min_alpha, float(alpha0*(1.0 - 1.0*count/total_words)));
        Vector work(layer_size_);
        for(int j=0; j<len; ++j){
          auto it = vocab_p.find(sentence[j]);
          if(it == vocab_p.end()) continue;
          Word* current = it->second.get();
          size_t codelen = current->codes_.size();
          int m = max(0, j-window_+reduced_window);
          int n = min(len, j+window_+1-reduced_window);
          for(;m<n;++m){
            Word* word = vocab_p.find(sentence[m])->second.get();
            if(m==n || word->codes_.empty()) continue;
            int32_t word_index = word->index_;
            auto& l1 = syn0_[word_index];
            fill(work.begin(), work.end(), 0);
            for(size_t b=0; b<codelen; ++b){
              int idx = current->points_[b];
              auto& l2 = syn1_[idx];
              float f = vectorCal::dot(l1, l2);
              if(f <= -max_exp || f >= max_exp) continue;
              int fi = int((f+max_exp)*(max_size/max_exp/2.0));
              f = table[fi];
              float g = (1-current->codes_[b] - f)*alpha;
              vectorCal::saxpy(work, g, l2);
              vectorCal::saxpy(l2, g, l1);
            }
            vectorCal::saxpy(l1, 1.0, work);
          }
          ++count;
        }
        #pragma omp atomic
      }
      for(auto& v: syn0_) vectorCal::unit(v);
      return 0;
    }

    bool buildModel(){
      if(vocab_.empty()){
        return false;
      } else {
        CreateHaffmanTree();
        train();
        return true;
      }
    }

    std::unordered_map<string, uint32_t> vocab_;
    std::unordered_map<string, WordP> vocab_p;
    int n_words_ = 0;
    vector<Word *> words_;
    vector<vector<float> > syn0_, syn1_;
    int layer_size_;
    float alpha_, min_alpha_;
    int window_;
    std::vector<std::vector<string> > sentences;
}; 
}

#endif
