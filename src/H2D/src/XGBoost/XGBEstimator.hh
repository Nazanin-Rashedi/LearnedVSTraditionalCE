#ifndef XGB_HH
#define XGB_HH
#include "infra/EstimatorBase2dim.hh"
#include "infra/cb.hh"
#include "infra/types.hh"
#include </home/rashedi/xgboost/include/xgboost/c_api.h>
#include <assert.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#define safe_xgboost(call)                                                     \
  {                                                                            \
    int err = (call);                                                          \
    if (err != 0) {                                                            \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call,   \
              XGBGetLastError());                                              \
      exit(1);                                                                 \
    }                                                                          \
  }

namespace H2D {

class XGBEstimator : public EstimatorBase2dim {
public:
  XGBEstimator( // const uint num_trees,const uint num_train,
      const H2D::Cb &aCb, const uint theta, const double q, const uint max_card,
      const std::string &file_base);
  XGBEstimator( // const uint num_trees,const uint num_train,
      const H2D::Cb &aCb, const uint max_card, const std::string &file_base,
      const query_vt &queryvec, const query_vt &trainQueryvec);
  XGBEstimator(XGBEstimator &oxgb);
  ~XGBEstimator();
  XGBEstimator &operator=(const XGBEstimator &oxgb);

public:
  void run_estimate();
  void run_prediction(const char *testFileName);
  void train_model(const char *trainFileName, const char *evalFileName);
  query_vt read_query_file(const std::string &aFilename);
  virtual void fill_libsvm_trainfiles() const;
  virtual void fill_libsvm_testfiles() const;
  double estimate(const query_t &lQuery) const;
  double estimate(const rectangle_t &r) const;
  std::ostream &print_name_param(std::ostream &os) const;
  uint size() const;
  inline const std::string &filebase() const { return _filebase; }
  inline const query_vt &queries() const { return _queries; }
  inline const query_vt &trainQueries() const { return _trainQueries; }
  inline uint num_trees() const { return _num_trees; }
  inline uint num_train_queries() const { return _num_train_queries; }
  inline uint max_card() const { return _max_card; }

protected:
  inline void setEnriched(uint value) { _enriched = value; }

private:
  uint _max_card;
  uint _num_trees;
  uint _num_train_queries;
  std::string _filebase;
  query_vt _queries;
  query_vt _trainQueries;
  std::vector<float> _preds;
  uint _silent;
  uint _use_gpu;
  BoosterHandle booster;
  DMatrixHandle dtrain, dtest, deval;
  uint _enriched;

protected:
  H2D::Cb _aCb;
};

}

#endif
