/*
  This is the mpc_pk component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Daniil Svyatskiy

  Process kernel for coupling of Transport PK and Chemistry PK.
*/

#include "mpc_coupled_reactive_transport_pk.hh"

namespace Amanzi { 

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
Coupled_ReactiveTransport_PK_ATS::Coupled_ReactiveTransport_PK_ATS(
                                          Teuchos::ParameterList& pk_tree,
                                          const Teuchos::RCP<Teuchos::ParameterList>& global_list,
                                          const Teuchos::RCP<State>& S,
                                          const Teuchos::RCP<TreeVector>& soln) :
  Amanzi::PK_MPCAdditive<PK>(pk_tree, global_list, S, soln)
 { 

  storage_created = false;
  chem_step_succeeded = true;
  std::string pk_name = pk_tree.name();
  
  boost::iterator_range<std::string::iterator> res = boost::algorithm::find_last(pk_name,"->"); 
  if (res.end() - pk_name.end() != 0) boost::algorithm::erase_head(pk_name,  res.end() - pk_name.begin());

// Create miscaleneous lists.
  Teuchos::RCP<Teuchos::ParameterList> pk_list = Teuchos::sublist(global_list, "PKs", true);
  //std::cout<<*pk_list;
  crt_pk_list_ = Teuchos::sublist(pk_list, pk_name, true);
  
  transport_pk_index_ = crt_pk_list_->get<int>("transport index", 0);
  chemistry_pk_index_ = crt_pk_list_->get<int>("chemistry index", 1 - transport_pk_index_);

  transport_subcycling_ = crt_pk_list_->get<bool>("transport subcycling", false);

  tranport_pk_ = Teuchos::rcp_dynamic_cast<CoupledTransport_PK>(sub_pks_[transport_pk_index_]);
  ASSERT(tranport_pk_ != Teuchos::null);
  
  chemistry_pk_ = Teuchos::rcp_dynamic_cast<WeakMPC>(sub_pks_[chemistry_pk_index_]);
  ASSERT(chemistry_pk_ != Teuchos::null);

  tranport_pk_subsurface_ = 
    Teuchos::rcp_dynamic_cast<Transport::Transport_PK_ATS>(tranport_pk_->get_subpk(0));
  ASSERT(tranport_pk_subsurface_!= Teuchos::null);
  tranport_pk_overland_ = 
    Teuchos::rcp_dynamic_cast<Transport::Transport_PK_ATS>(tranport_pk_->get_subpk(1));
  ASSERT(tranport_pk_overland_!= Teuchos::null);

  chemistry_pk_subsurface_ = 
    Teuchos::rcp_dynamic_cast<AmanziChemistry::Chemistry_PK>(chemistry_pk_->get_subpk(0));
  ASSERT(chemistry_pk_subsurface_!= Teuchos::null);
  chemistry_pk_overland_ = 
    Teuchos::rcp_dynamic_cast<AmanziChemistry::Chemistry_PK>(chemistry_pk_->get_subpk(1));
  ASSERT(chemistry_pk_overland_!= Teuchos::null);

  // std::cout<<tranport_pk_subsurface_->name()<<"\n";
  // std::cout<<tranport_pk_overland_->name()<<"\n";
  // std::cout<<chemistry_pk_subsurface_->name()<<"\n";
  // std::cout<<chemistry_pk_overland_->name()<<"\n";


 }

// -----------------------------------------------------------------------------
// Calculate the min of sub PKs timestep sizes.
// -----------------------------------------------------------------------------
double Coupled_ReactiveTransport_PK_ATS::get_dt() {

  dTtran_ = tranport_pk_->get_dt();
  dTchem_ = chemistry_pk_->get_dt();

  if (!chem_step_succeeded && (dTchem_/dTtran_ > 0.99)) {
     dTchem_ *= 0.5;
  } 

  if (dTtran_ > dTchem_) dTtran_= dTchem_; 

  if (transport_subcycling_){ 
    return dTchem_;
  } else {
    return dTtran_;
  }

}

void Coupled_ReactiveTransport_PK_ATS::set_states(const Teuchos::RCP<const State>& S,
                                                  const Teuchos::RCP<State>& S_inter,
                                                  const Teuchos::RCP<State>& S_next) {
  //  PKDefaultBase::set_states(S, S_inter, S_next);
  //S_ = S;
  S_inter_ = S_inter;
  S_next_ = S_next;

  //chemistry_pk_->set_states(S, S_inter, S_next);
  tranport_pk_->set_states(S, S_inter, S_next);

}



void Coupled_ReactiveTransport_PK_ATS::Setup(const Teuchos::Ptr<State>& S){

  Amanzi::PK_MPCAdditive<PK>::Setup(S);

  // communicate chemistry engine to transport.
#ifdef ALQUIMIA_ENABLED
  tranport_pk_subsurface_->SetupAlquimia(Teuchos::rcp_static_cast<AmanziChemistry::Alquimia_PK>(chemistry_pk_subsurface_),
                                            chemistry_pk_subsurface_->chem_engine());
  tranport_pk_overland_->SetupAlquimia(Teuchos::rcp_static_cast<AmanziChemistry::Alquimia_PK>(chemistry_pk_overland_),
                                            chemistry_pk_overland_->chem_engine());
#endif


}

// void Coupled_ReactiveTransport_PK_ATS::Initialize(const Teuchos::Ptr<State>& S){

//   // Key subsurface_domain_key = tranport_pk_subsurface_->get_domain_name();
//   // Key overland_domain_key = tranport_pk_overland_->get_domain_name();
//   // Key tcc_sub_key = getKey(subsurface_domain_key, "total_component_concentration");
//   // Key tcc_over_key = getKey(overland_domain_key, "total_component_concentration");

//   // ttc_sub_stor_ = Teuchos::rcp(new Epetra_MultiVector(*S->GetFieldCopyData(tcc_sub_key,"subcycling")
//   //                                            ->ViewComponent("cell", true)));

//   // ttc_over_stor_ = Teuchos::rcp(new Epetra_MultiVector(*S->GetFieldCopyData(tcc_over_key,"subcycling")
//   //                                            ->ViewComponent("cell", true)));

// }


bool Coupled_ReactiveTransport_PK_ATS::AdvanceStep(double t_old, double t_new, bool reinit) {


  bool fail = false;
  chem_step_succeeded = false;
  Key subsurface_domain_key = tranport_pk_subsurface_->get_domain_name();
  Key overland_domain_key = tranport_pk_overland_->get_domain_name();
  Key tcc_sub_key = getKey(subsurface_domain_key, "total_component_concentration");
  Key tcc_over_key = getKey(overland_domain_key, "total_component_concentration");
  Key sub_mol_den_key = getKey(subsurface_domain_key,  "molar_density_liquid");
  Key over_mol_den_key = getKey(overland_domain_key,  "molar_density_liquid");


  if (!storage_created){
    ttc_sub_stor_ = Teuchos::rcp(new Epetra_MultiVector(*S_->GetFieldCopyData(tcc_sub_key,"subcycling")
                                                      ->ViewComponent("cell", true)));

    ttc_over_stor_ = Teuchos::rcp(new Epetra_MultiVector(*S_->GetFieldCopyData(tcc_over_key,"subcycling")
                                                       ->ViewComponent("cell", true)));
    storage_created = true;
  }

  // First we do a transport step.

  bool pk_fail = false;
  pk_fail = tranport_pk_->AdvanceStep(t_old, t_new, reinit);

  if (pk_fail){
    Errors::Message message("MPC: Coupled Transport PK returned an unexpected error.");
    Exceptions::amanzi_throw(message);
  }


  try {
    Teuchos::RCP<Epetra_MultiVector> tcc_sub = 
      S_->GetFieldCopyData(tcc_sub_key,"subcycling","state")->ViewComponent("cell", true);

    Teuchos::RCP<Epetra_MultiVector> tcc_over =
      S_->GetFieldCopyData(tcc_over_key,"subcycling", "state")->ViewComponent("cell", true);

    Teuchos::RCP<const Epetra_MultiVector> mol_dens_sub =
      S_->GetFieldData(sub_mol_den_key)->ViewComponent("cell", true);

    Teuchos::RCP<const Epetra_MultiVector> mol_dens_over =
      S_->GetFieldData(over_mol_den_key)->ViewComponent("cell", true);

    Teuchos::RCP<const AmanziMesh::Mesh> mesh_sub = S_->GetMesh(subsurface_domain_key);
    Teuchos::RCP<const AmanziMesh::Mesh> mesh_over = S_->GetMesh(overland_domain_key);
    int ncells_owned_sub  = mesh_sub->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
    int ncells_owned_over = mesh_over->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);

  // //   // convert from mole fraction[-] to mol/L
    // for (int c=0; c<ncells_owned_sub; c++) (*tcc_sub)[0][c]  *= (*mol_dens_sub)[0][c] / 1000.;
    // for (int c=0; c<ncells_owned_over; c++) (*tcc_over)[0][c] *= (*mol_dens_over)[0][c] / 1000.;
    for (int c=0; c<ncells_owned_sub; c++) 
      (*ttc_sub_stor_)[0][c] = (*tcc_sub)[0][c] * (*mol_dens_sub)[0][c] / 1000.;
    for (int c=0; c<ncells_owned_over; c++) 
      (*ttc_over_stor_)[0][c] = (*tcc_over)[0][c] * (*mol_dens_over)[0][c] / 1000.;



    
    //std::cout<<tcc_sub<<" "<<tcc_over<<"\n";
    // chemistry_pk_subsurface_->set_aqueous_components(tcc_sub);
    // chemistry_pk_overland_->set_aqueous_components(tcc_over);

    chemistry_pk_subsurface_-> set_aqueous_components(ttc_sub_stor_);
    chemistry_pk_overland_->set_aqueous_components(ttc_over_stor_);

    //std::cout<<tcc_sub<<" "<<tcc_over<<"\n";

    pk_fail = chemistry_pk_->AdvanceStep(t_old, t_new, reinit);
    chem_step_succeeded = true;

    // *tcc_sub = *chemistry_pk_subsurface_->aqueous_components();
    // *tcc_over = *chemistry_pk_overland_->aqueous_components();
   
    *ttc_sub_stor_ = *chemistry_pk_subsurface_->aqueous_components();
    *ttc_over_stor_ = *chemistry_pk_overland_->aqueous_components();
    //std::cout<<tcc_sub<<" "<<tcc_over<<"\n";

  //   // convert from mol/L fraction to mole fraction[-]
    for (int c=0; c<ncells_owned_sub; c++) 
      (*tcc_sub)[0][c]  = (*ttc_sub_stor_)[0][c] / ((*mol_dens_sub)[0][c] / 1000.);
    for (int c=0; c<ncells_owned_over; c++) 
      (*tcc_over)[0][c] = (*ttc_over_stor_)[0][c] / ((*mol_dens_over)[0][c] / 1000.);

    // for (int c=0; c<ncells_owned_sub; c++) 
    //   (*tcc_sub)[0][c]  /=  ((*mol_dens_sub)[0][c] / 1000.);
    // for (int c=0; c<ncells_owned_over; c++) 
    //   (*tcc_over)[0][c] /=  ((*mol_dens_over)[0][c] / 1000.);

  //   //std::cout << *tcc_over<<"\n";

  }
  catch (const Errors::Message& chem_error) {
    fail = true;
  }
    
  return fail;
};


// -----------------------------------------------------------------------------
// 
// -----------------------------------------------------------------------------
void Coupled_ReactiveTransport_PK_ATS::CommitStep(double t_old, double t_new, const Teuchos::RCP<State>& S) {

  tranport_pk_->CommitStep(t_old, t_new, S);
  chemistry_pk_->CommitStep(t_old, t_new, S);

}

   

}// namespace
