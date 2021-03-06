#ifndef __LOSS_HPP__
#define __LOSS_HPP__

#include <utility>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <omp.h>

#include "elements.hpp"
#include "model.hpp"
#include "ratings.hpp"

enum loss_option_t {L1_HINGE, L2_HINGE, LOGISTIC, SQUARED};

//sum of squares loss 
//if choice==0 evaluate personalized ranking, if choice==1, evaluate user targeting performance 
double compute_ndcg(const TestMatrix& test, const Model& model, int choice) {
    double ndcg_sum = 0.;
    std::vector<double> score;
    int n_valid_ids = 0;
    //compute the ndcg for personalized ranking
    
    if(choice == 0){
        for(int uid=0; uid < model.n_users; ++uid) {
            double dcg = 0.;
            score.clear();
            vector<pair<int, double>> user_pairs = test.itemwise_test_pairs[uid];
            if(!user_pairs.empty()){
                for(int i = 0; i< user_pairs.size(); ++i) {
                    int iid = user_pairs[i].first;
                    if (iid < model.n_items) {
                        double prod = 0.;
                        for(int k=0; k< model.rank; ++k) 
                            prod += model.U[uid * model.rank + k] * model.V[iid * model.rank + k]; //estimated socres
                        score.push_back(prod);
                    }else {
                        score.push_back(-1e10);
                    }
                }
                ndcg_sum += test.compute_user_ndcg(uid, score);
                n_valid_ids++;
            } 
        }
        ndcg_sum = ndcg_sum/(double)n_valid_ids;
    }

    //computer the ndcg for the user targeting
    if(choice == 1){
        for(int iid=0; iid < model.n_items; iid++){
            double dcg = 0.;
            score.clear();

            vector<pair<int, double>> item_pairs = test.userwise_test_pairs[iid];

            if(!item_pairs.empty()){
                for(int i=0; i < item_pairs.size(); i++){
                    int uid = item_pairs[i].first;
                    if(uid < model.n_users){
                        double prod = 0.;
                        for(int k=0; k< model.rank; ++k)
                            prod += model.U[uid * model.rank + k] * model.V[iid * model.rank + k]; //estimated socres
                        score.push_back(prod);
                    }else {
                      score.push_back(-1e10);
                    }
                }
                ndcg_sum += test.compute_item_ndcg(iid, score);
                n_valid_ids++;
            }            
        }
        ndcg_sum = ndcg_sum/(double)n_valid_ids;
    }
    return ndcg_sum;
}

/*
double compute_loss(const Model& model, const TestMatrix& test, int choice) {
  double p = 0.;
  #pragma omp parallel for reduction(+:p) 
  for(int i=0; i<test.ratings.size(); ++i) {
    double *user_vec  = &(model.U[test.ratings[i].user_id * model.rank]);
    double *item_vec  = &(model.V[test.ratings[i].item_id * model.rank]);
    double d = 0.;
    for(int j=0; j<model.rank; ++j) d += user_vec[j] * item_vec[j];
    p += .5 * pow(test.ratings[i].score - d, 2.);
  }
     
  return p;   
}
*/

/*
// binary classification loss
double compute_loss(const Model& model, const std::vector<comparison>& TestComps, loss_option_t option) {
  double p = 0.;
  #pragma omp parallel for reduction(+:p)
  for(int i=0; i<TestComps.size(); ++i) {
    double *user_vec  = &(model.U[TestComps[i].user_id  * model.rank]);
    double *item1_vec = &(model.V[TestComps[i].item1_id * model.rank]);
    double *item2_vec = &(model.V[TestComps[i].item2_id * model.rank]);
    double d = 0., loss;
    for(int j=0; j<model.rank; ++j) {
      d += user_vec[j] * (item1_vec[j] - item2_vec[j]);
    }
    
    switch(option) {
      case SQUARED:
        loss = .5*pow(1.-d, 2.);
        break;
      case LOGISTIC:
        loss = log(1.+exp(-d));
        break;
      case L1_HINGE:
        loss = std::max(0., 1.-d);
        break;
      case L2_HINGE:
        loss = pow(std::max(0., 1.-d), 2.);
    }

    p += loss;
  }
     
  return p;		
}
*/


/*
double compute_loss_v2(const Model& model, std::vector<std::vector<int> >& Iu, std::vector<std::vector<int> >& noIu) {
  double p = 0.;
  #pragma omp parallel for reduction (+ : p)
  for (int uid = 0; uid < Iu.size(); ++uid) {
    for (int idx1 = 0; idx1 < Iu[uid].size(); ++idx1) {
      for (int idx2 = 0; idx2 < noIu[uid].size(); ++idx2) {
        int iid1 = Iu[uid][idx1];
        int iid2 = noIu[uid][idx2];

        double *user_vec  = &(model.U[uid  * model.rank]);
        double *item1_vec = &(model.V[iid1 * model.rank]);
        double *item2_vec = &(model.V[iid2 * model.rank]);

        double d = 0., loss;
        for(int j=0; j<model.rank; ++j) {
          d += user_vec[j] * (item1_vec[j] - item2_vec[j]);
        }
        
        p += log(1. + exp(-d) );
      }
    }
  }

  return p;



}





double compute_pairwiseError(const RatingMatrix& TestRating, const RatingMatrix& PredictedRating) {

  std::vector<double> score(TestRating.n_items);
 
  double sum_correct = 0.;
  for(int uid=0; uid<TestRating.n_users; ++uid) {
    score.resize(TestRating.n_items,-1e10);    
    double max_sc = -1.;

    int j = PredictedRating.idx[uid];
    for(int i=TestRating.idx[uid]; i<TestRating.idx[uid+1]; ++i) {
      int iid = TestRating.ratings[i].item_id;
      while((j < PredictedRating.idx[uid+1]) && (PredictedRating.ratings[j].item_id < iid)) ++j;
      if ((PredictedRating.ratings[j].user_id == uid) && (PredictedRating.ratings[j].item_id == iid)) score[iid] = PredictedRating.ratings[j].score;
      if (TestRating.ratings[i].score > max_sc) max_sc = TestRating.ratings[i].score;
    }

    max_sc = max_sc - .1;

    unsigned long long cor_this = 0, discor_this = 0;
    for(int i=TestRating.idx[uid]; i<TestRating.idx[uid+1]-1; ++i) {
      for(int j=i+1; j<TestRating.idx[uid+1]; ++j) {
        int item1_id = TestRating.ratings[i].item_id;
        int item2_id = TestRating.ratings[j].item_id; 
        double item1_sc = TestRating.ratings[i].score;
        double item2_sc = TestRating.ratings[j].score;      

        if ((item1_sc > item2_sc) && (score[item1_id] > score[item2_id])) {
		++cor_this;
	}
        if ((item1_sc < item2_sc) && (score[item1_id] >= score[item2_id])) {
		++discor_this;
	}
  //      ++n_comps_this;
      }
    }

    sum_correct += (double)cor_this / ((double)cor_this);
  }

  return sum_correct / (double)TestRating.n_users; 
}


double compute_pairwiseError(const RatingMatrix& TestRating, const Model& PredictedModel) {
  double sum_correct = 0.;
  int neg_user = 0;
  #pragma omp parallel for reduction(+:sum_correct, neg_user) 
  for(int uid=0; uid < TestRating.n_users; ++uid) {
    std::vector<double> score(TestRating.n_items,-1e10);
    //double max_sc = -1.;
    for(int i=TestRating.idx[uid]; i<TestRating.idx[uid+1]; ++i) {
      int iid = TestRating.ratings[i].item_id;
      
      if (iid < PredictedModel.n_items) {
        double prod = 0.;
        for(int k=0; k<PredictedModel.rank; ++k) prod += PredictedModel.U[uid * PredictedModel.rank + k] * PredictedModel.V[iid * PredictedModel.rank + k];
        score[iid] = prod;
      }
      else {
        score[iid] = -1e10;
      }
//    if (TestRating.ratings[i].score > max_sc) max_sc = TestRating.ratings[i].score;
    }

//    max_sc = max_sc - .1;

    unsigned long cor_this = 0, discor_this = 0;
    for(int i=TestRating.idx[uid]; i < TestRating.idx[uid+1]-1; ++i) {
      for(int j=i+1; j<TestRating.idx[uid+1]; ++j) {
        int item1_id = TestRating.ratings[i].item_id;
        int item2_id = TestRating.ratings[j].item_id; 
        double item1_sc = TestRating.ratings[i].score;
        double item2_sc = TestRating.ratings[j].score;      

        if ((item1_sc > item2_sc) && (score[item1_id] > score[item2_id]))  ++cor_this;     //true positive
        if ((item1_sc < item2_sc) && (score[item1_id] >= score[item2_id])) ++discor_this;  //false negative
      }
    }
   
    if(cor_this==0 && discor_this==0) {
	    neg_user++;
	    continue;	
    } 	 
    sum_correct += (double)cor_this / ((double)cor_this+(double) discor_this);   
  }

  //comp_error.first  = (double)error / (double)n_comps;
  //comp_error.second = (double)errorT / (double)n_compsT; 
  return sum_correct / (double)(TestRating.n_users - neg_user); 
}

double compute_ndcg(const RatingMatrix& TestRating, const std::string& Predict_filename) {

  double ndcg_sum;
 
  std::vector<double> score;
  std::string user_str, attribute_str;
  std::stringstream attribute_sstr;  

  std::ifstream f;
 
  f.open(Predict_filename);
  if (f.is_open()) {
     
    for(int uid=0; uid<TestRating.n_users; ++uid) { 
      getline(f, user_str);

      size_t pos1 = 0, pos2;
     
      score.clear();
      for(int idx=TestRating.idx[uid]; idx<TestRating.idx[uid+1]; ++idx) {
        int iid = -1;
        double sc;

        while(iid < TestRating.ratings[idx].item_id) {
          pos2 = user_str.find(':', pos1); if (pos2 == std::string::npos) break; 
          attribute_str = user_str.substr(pos1, pos2-pos1);
          attribute_sstr.clear(); attribute_sstr.str(attribute_str);
          attribute_sstr >> iid;
          --iid;
          pos1 = pos2+1;

          pos2 = user_str.find(' ', pos1); attribute_str = user_str.substr(pos1, pos2-pos1);
          attribute_sstr.clear(); attribute_sstr.str(attribute_str);
          attribute_sstr >> sc;
          pos1 = pos2+1;
        }
      
        if (iid == TestRating.ratings[idx].item_id)
          score.push_back(sc);
        else
          score.push_back(-1e10);

      }         
 
      ndcg_sum += TestRating.compute_user_ndcg(uid, score);
    } 

  } else {
      printf("Error in opening the extracted rating file!\n");
      std::cout << Predict_filename << std::endl;
      exit(EXIT_FAILURE);
  }
  
  f.close();
}

double compute_ndcg(const RatingMatrix& TestRating, const RatingMatrix& PredictedRating) {

  double ndcg_sum = 0.;
  std::vector<double> score; 
 
  for(int uid=0; uid<TestRating.n_users; ++uid) {
    double dcg = 0.;
 
    score.clear();
    int j = PredictedRating.idx[uid];
    for(int i=TestRating.idx[uid]; i<TestRating.idx[uid+1]; ++i) {
      double prod = 0.;
      while((j < PredictedRating.idx[uid+1]) && (PredictedRating.ratings[j].item_id < TestRating.ratings[i].item_id)) ++j;
      if ((PredictedRating.ratings[j].user_id == TestRating.ratings[i].user_id) && (PredictedRating.ratings[j].item_id == TestRating.ratings[i].item_id))
        score.push_back(PredictedRating.ratings[j].score);
      else
        score.push_back(-1e10);
    }
  
    ndcg_sum += TestRating.compute_user_ndcg(uid, score);
  }

  return ndcg_sum / (double)PredictedRating.n_users;
}

double compute_ndcg(const RatingMatrix& TestRating, const Model& PredictedModel) {
 
  if (!TestRating.is_dcg_max_computed) return -1.; 
 
  double ndcg_sum = 0.;
  std::vector<double> score;

  for(int uid=0; uid<PredictedModel.n_users; ++uid) {
    double dcg = 0.;
  
    score.clear();
    for(int i=TestRating.idx[uid]; i<TestRating.idx[uid+1]; ++i) {
      int iid = TestRating.ratings[i].item_id;

      if (iid < PredictedModel.n_items) {
        double prod = 0.;
        for(int k=0; k<PredictedModel.rank; ++k) prod += PredictedModel.U[uid * PredictedModel.rank + k] * PredictedModel.V[iid * PredictedModel.rank + k];
        score.push_back(prod);
      }
      else {
        score.push_back(-1e10);
      }
    }
    
    ndcg_sum += TestRating.compute_user_ndcg(uid, score);
	}

  return ndcg_sum / (double)PredictedModel.n_users;
}
*/


#endif
