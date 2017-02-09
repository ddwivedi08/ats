/*
  Transport PK 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Derived class for transport sources and sinks.
*/

#ifndef AMANZI_TRANSPORT_DOMAIN_FUNCTION_HH_
#define AMANZI_TRANSPORT_DOMAIN_FUNCTION_HH_

#include <vector>
#include <map>
#include <string>

#include "Epetra_MultiVector.h"
#include "Teuchos_RCP.hpp"

#include "CommonDefs.hh"
#include "Mesh.hh"
#include "DenseVector.hh"

namespace Amanzi {
namespace Transport {

template <class Value_Type>
class TransportDomainFunction {
 public:
  TransportDomainFunction() : domain_volume_(-1.0) {};
  TransportDomainFunction(const Teuchos::ParameterList& plist) : domain_volume_(-1.0) {};
  ~TransportDomainFunction() {};

  // source term on time interval (t0, t1]
  virtual void Compute(double t0, double t1) { ASSERT(false); }

  // model name
  virtual std::string name() const { return "undefined"; } 

  // access
  // -- volume of the regions
  double domain_volume() { return domain_volume_; }
  // -- nick-name of the function
  std::string keyword() { return keyword_; }

  std::vector<std::string>& tcc_names() { return tcc_names_; }
  std::vector<int>& tcc_index() { return tcc_index_; }
  virtual void set_state(const Teuchos::RCP<State>& S) {S_ = S;}

// iterator methods
  typename std::map<int, Value_Type>::iterator begin() {return value_.begin(); }
  typename std::map<int, Value_Type>::iterator end() {return value_.end(); }
  typename std::map<int, Value_Type>::size_type size() {return value_.size(); }

 protected:
  double domain_volume_;
  std::map<int, WhetStone::DenseVector> value_;  // tcc values on boundary faces
  std::string keyword_;
  Teuchos::RCP<const State> S_;

  std::vector<std::string> tcc_names_;  // list of component names
  std::vector<int> tcc_index_;  // index of component in the global list
};

}  // namespace Transport
}  // namespace Amanzi

#endif