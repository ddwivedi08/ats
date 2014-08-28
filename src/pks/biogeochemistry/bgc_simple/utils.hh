/*
Utility functions for Vegetation.

Author: Ethan Coon (ecoon@lanl.gov)
        Chonggang Xu (cxu@lanl.gov)

Licencse: BSD
*/


#ifndef ATS_BGC_QSAT_HH_
#define ATS_BGC_QSAT_HH_

#include "Epetra_SerialDenseVector.h"

namespace Amanzi {
namespace BGC {


struct MetData {
  double qSWin;
  double tair;
  double windv;
  double wind_ref_ht;
  double relhum;
  double CO2a;
  double lat;
};

double PermafrostDepth(const Epetra_SerialDenseVector& SoilTArr,
                       const Epetra_SerialDenseVector& SoilDArr,
                       double freeze_temp=273.15);

int PermafrostDepthIndex(const Epetra_SerialDenseVector& SoilTArr,
                       const Epetra_SerialDenseVector& SoilDArr,
                       double freeze_temp=273.15);

// This function calculate the effect of temperature on biological process.
double TEffectsQ10(double Q10, double T, double refT);


} // namespace
} // namespace


#endif