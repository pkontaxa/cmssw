#include "DataFormats/Math/interface/Point3D.h"

#include "DataFormats/ParticleFlowReco/interface/PFLayer.h"

#include "DataFormats/ParticleFlowReco/interface/PFBlock.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElement.h"

#include "RecoParticleFlow/PFClusterAlgo/interface/PFClusterAlgo.h"
// #include "RecoParticleFlow/PFBlockAlgo/interface/PFBlockAlgo.h"
#include "RecoParticleFlow/PFBlockAlgo/interface/PFGeometry.h"
// #include "RecoParticleFlow/PFAlgo/interface/PFAlgo.h"


#include "RecoParticleFlow/PFRootEvent/interface/PFRootEventManager.h"

#include "RecoParticleFlow/PFRootEvent/interface/IO.h"

#include "RecoParticleFlow/PFRootEvent/interface/PFJetAlgorithm.h" 
#include "RecoParticleFlow/PFRootEvent/interface/Utils.h" 
#include "RecoParticleFlow/PFRootEvent/interface/EventColin.h" 
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyCalibration.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyResolution.h"

#include <TFile.h>
#include <TTree.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TMarker.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TCutG.h>
#include <TPolyLine.h>
#include <TColor.h>
#include "TGraph.h"
#include "TMath.h"
#include "TLine.h"
#include "TLatex.h"
#include "TVector3.h"

#include <iostream>
#include <vector>
#include <stdlib.h>

using namespace std;


PFRootEventManager::PFRootEventManager() {
//   energyCalibration_ = new PFEnergyCalibration();
//   energyResolution_ = new PFEnergyResolution();
}



PFRootEventManager::PFRootEventManager(const char* file)
  : 
  iEvent_(0),
  options_(0),
  tree_(0),
  outTree_(0),
  outEvent_(0),
//   clusters_(new reco::PFClusterCollection),
  clustersECAL_(new reco::PFClusterCollection),
  clustersHCAL_(new reco::PFClusterCollection),
  clustersPS_(new reco::PFClusterCollection),
  pfBlocks_(new reco::PFBlockCollection),
  pfCandidates_(new reco::PFCandidateCollection),
  maxERecHitEcal_(-1),
  maxERecHitHcal_(-1) {
  
//   options_ = 0;
//   tree_ = 0;

//   outEvent_ = 0;
//   outTree_ = 0;

//   jetAlgo_ = 0;
  
//   iEvent_=0;
  h_deltaETvisible_MCEHT_ 
    = new TH1F("h_deltaETvisible_MCEHT","Jet Et difference CaloTowers-MC"
	       ,500,-50,50);
  h_deltaETvisible_MCPF_  
    = new TH1F("h_deltaETvisible_MCPF" ,"Jet Et difference ParticleFlow-MC"
	       ,500,-50,50);

  readOptions(file, true, true);

  displayView_.resize(NViews);
  displayHist_.resize(NViews);
  for (unsigned iView = 0; iView < NViews; iView++) {
    displayView_[iView] = 0;
    displayHist_[iView] = 0;
  }

//   maxERecHitEcal_ = -1;
//   maxERecHitHcal_ = -1;

//   energyCalibration_ = new PFEnergyCalibration();
//   energyResolution_ = new PFEnergyResolution();
}

void PFRootEventManager::reset() { 
  maxERecHitEcal_ = -1;
  maxERecHitHcal_ = -1;  
  rechitsECAL_.clear();
  rechitsHCAL_.clear();
  rechitsPS_.clear();
  recTracks_.clear();
  clustersECAL_->clear();
  clustersHCAL_->clear();
  clustersPS_->clear();
  clustersIslandBarrel_.clear();
  trueParticles_.clear();


  if(outEvent_) outEvent_->reset();
  
}

void PFRootEventManager::readOptions(const char* file, 
				     bool refresh, 
				     bool reconnect) {

  reset();
  
  PFGeometry pfGeometry; // initialize geometry
  
  // cout<<"reading options "<<endl;

  try {
    if( !options_ )
      options_ = new IO(file);
    else if( refresh) {
      delete options_;
      options_ = new IO(file);
    }
  }
  catch( const string& err ) {
    cout<<err<<endl;
    return;
  }

  clusteringIsOn_ = true;
  options_->GetOpt("clustering", "on/off", clusteringIsOn_);

//   clusteringMode_ = 0;
//   options_->GetOpt("clustering", "mode", clusteringMode_);  
  

  bool clusteringDebug = false;
  options_->GetOpt("clustering", "debug", clusteringDebug );


  debug_ = false; 
  options_->GetOpt("rootevent", "debug", debug_);

  findRecHitNeighbours_ = true;
  options_->GetOpt("clustering", "findRecHitNeighbours", 
		   findRecHitNeighbours_);
  
  
  // input root file --------------------------------------------

  if( reconnect )
    connect( inFileName_.c_str() );

  // output root file   ------------------------------------------

  outFile_ = 0;
  string outfilename;
  options_->GetOpt("root","outfile", outfilename);
  if(!outfilename.empty() ) {
    outFile_ = TFile::Open(outfilename.c_str(), "recreate");

    bool doOutTree = false;
    options_->GetOpt("root","outtree", doOutTree);
    if(doOutTree) {
      outFile_->cd();
      // cout<<"do tree"<<endl;
      outEvent_ = new EventColin();
      outTree_ = new TTree("Eff","");
      outTree_->Branch("event","EventColin", &outEvent_,32000,2);
    }
    // cout<<"don't do tree"<<endl;
  }


  // various parameters ------------------------------------------

  vector<int> algos;
  options_->GetOpt("display", "cluster_algos", algos);
  algosToDisplay_.clear();
  for(unsigned i=0; i< algos.size(); i++) algosToDisplay_.insert( algos[i] );

  displayClusterLines_ = false;
  options_->GetOpt("display", "cluster_lines", displayClusterLines_);
  
//   if(displayClusterLines_) 
//     cout<<"will display cluster lines "<<endl;

  viewSizeEtaPhi_.clear();
  options_->GetOpt("display", "viewsize_etaphi", viewSizeEtaPhi_);
  if(viewSizeEtaPhi_.size() != 2) {
    cerr<<"PFRootEventManager::ReadOptions, bad display/viewsize_etaphi tag...using 700/350"
	<<endl;
    viewSizeEtaPhi_.clear();
    viewSizeEtaPhi_.push_back(700); 
    viewSizeEtaPhi_.push_back(350); 
  }

  viewSize_.clear();
  options_->GetOpt("display", "viewsize_xy", viewSize_);
  if(viewSize_.size() != 2) {
    cerr<<"PFRootEventManager::ReadOptions, bad display/viewsize_xy tag...using 700/350"
	<<endl;
    viewSize_.clear();
    viewSize_.push_back(600); 
    viewSize_.push_back(600); 
  }

  displayXY_ = true;
  options_->GetOpt("display", "x/y", displayXY_);
  
  displayEtaPhi_ = true;
  options_->GetOpt("display", "eta/phi", displayEtaPhi_);

  displayRZ_ = true;
  options_->GetOpt("display", "r/z", displayRZ_);

 
  displayColorClusters_ = false;
  options_->GetOpt("display", "color_clusters", displayColorClusters_);
 
  displayRecTracks_ = true;
  options_->GetOpt("display", "rectracks", displayRecTracks_);

  displayTrueParticles_ = true;
  options_->GetOpt("display", "particles", displayTrueParticles_);

  displayZoomFactor_ = 10;  
  options_->GetOpt("display", "zoom_factor", displayZoomFactor_);


  // filter --------------------------------------------------------------

  filterNParticles_ = 0;
  options_->GetOpt("filter", "nparticles", filterNParticles_);
  
  filterHadronicTaus_ = true;
  options_->GetOpt("filter", "hadronic_taus", filterHadronicTaus_);
  
  // clustering parameters -----------------------------------------------

  double threshEcalBarrel = 0.1;
  options_->GetOpt("clustering", "thresh_Ecal_Barrel", threshEcalBarrel);
  
  double threshSeedEcalBarrel = 0.3;
  options_->GetOpt("clustering", "thresh_Seed_Ecal_Barrel", 
		   threshSeedEcalBarrel);

  double threshEcalEndcap = 0.2;
  options_->GetOpt("clustering", "thresh_Ecal_Endcap", threshEcalEndcap);

  double threshSeedEcalEndcap = 0.8;
  options_->GetOpt("clustering", "thresh_Seed_Ecal_Endcap",
		   threshSeedEcalEndcap);

  double showerSigmaEcal = 3;  
  options_->GetOpt("clustering", "shower_Sigma_Ecal",
		   showerSigmaEcal);

  int nNeighboursEcal = 4;
  options_->GetOpt("clustering", "neighbours_Ecal", nNeighboursEcal);
  
  int posCalcNCrystalEcal = -1;
  options_->GetOpt("clustering", "posCalc_nCrystal_Ecal", 
		   posCalcNCrystalEcal);

  double posCalcP1Ecal = -1;
  options_->GetOpt("clustering", "posCalc_p1_Ecal", 
		   posCalcP1Ecal);
  

  clusterAlgoECAL_.setThreshBarrel( threshEcalBarrel );
  clusterAlgoECAL_.setThreshSeedBarrel( threshSeedEcalBarrel );
  
  clusterAlgoECAL_.setThreshEndcap( threshEcalEndcap );
  clusterAlgoECAL_.setThreshSeedEndcap( threshSeedEcalEndcap );

  clusterAlgoECAL_.setNNeighbours( nNeighboursEcal );
  clusterAlgoECAL_.setShowerSigma( showerSigmaEcal );

  clusterAlgoECAL_.setPosCalcNCrystal( posCalcNCrystalEcal );
  clusterAlgoECAL_.setPosCalcP1( posCalcP1Ecal );

  clusterAlgoECAL_.enableDebugging( clusteringDebug ); 


  int dcormode = 0;
  options_->GetOpt("clustering", "depthCor_Mode", dcormode);
  
  double dcora = -1;
  options_->GetOpt("clustering", "depthCor_A", dcora);
  double dcorb = -1;
  options_->GetOpt("clustering", "depthCor_B", dcorb);
  double dcorap = -1;
  options_->GetOpt("clustering", "depthCor_A_preshower", dcorap);
  double dcorbp = -1;
  options_->GetOpt("clustering", "depthCor_B_preshower", dcorbp);

//   if( dcormode > 0 && 
//       dcora > -0.5 && 
//       dcorb > -0.5 && 
//       dcorap > -0.5 && 
//       dcorbp > -0.5 ) {

//     cout<<"set depth correction "
// 	<<dcormode<<" "<<dcora<<" "<<dcorb<<" "<<dcorap<<" "<<dcorbp<<endl;
  reco::PFCluster::setDepthCorParameters( dcormode, 
					  dcora, dcorb, 
					  dcorap, dcorbp);
//   }
//   else {
//     reco::PFCluster::setDepthCorParameters( -1, 
// 					    0,0 , 
// 					    0,0 );
//   }

  

  double threshHcalBarrel = 0.8;
  options_->GetOpt("clustering", "thresh_Hcal_Barrel", threshHcalBarrel);
  
  double threshSeedHcalBarrel = 1.4;
  options_->GetOpt("clustering", "thresh_Seed_Hcal_Barrel", 
		   threshSeedHcalBarrel);

  double threshHcalEndcap = 0.8;
  options_->GetOpt("clustering", "thresh_Hcal_Endcap", threshHcalEndcap);

  double threshSeedHcalEndcap = 1.4;
  options_->GetOpt("clustering", "thresh_Seed_Hcal_Endcap",
		   threshSeedHcalEndcap);

  double showerSigmaHcal    = 15;
  options_->GetOpt("clustering", "shower_Sigma_Hcal",
                   showerSigmaHcal);
 
  int nNeighboursHcal = 4;
  options_->GetOpt("clustering", "neighbours_Hcal", nNeighboursHcal);

  int posCalcNCrystalHcal = 5;
  options_->GetOpt("clustering", "posCalc_nCrystal_Hcal",
                   posCalcNCrystalHcal);

  double posCalcP1Hcal = 1.0;
  options_->GetOpt("clustering", "posCalc_p1_Hcal", 
		   posCalcP1Hcal);




  clusterAlgoHCAL_.setThreshBarrel( threshHcalBarrel );
  clusterAlgoHCAL_.setThreshSeedBarrel( threshSeedHcalBarrel );
  
  clusterAlgoHCAL_.setThreshEndcap( threshHcalEndcap );
  clusterAlgoHCAL_.setThreshSeedEndcap( threshSeedHcalEndcap );

  clusterAlgoHCAL_.setNNeighbours( nNeighboursHcal );
  clusterAlgoHCAL_.setShowerSigma( showerSigmaHcal );

  clusterAlgoHCAL_.setPosCalcNCrystal( posCalcNCrystalHcal );
  clusterAlgoHCAL_.setPosCalcP1( posCalcP1Hcal );

  clusterAlgoHCAL_.enableDebugging( clusteringDebug ); 


  // clustering preshower

  double threshPSBarrel = 0.0001;
  options_->GetOpt("clustering", "thresh_PS_Barrel", threshPSBarrel);
  
  double threshSeedPSBarrel = 0.001;
  options_->GetOpt("clustering", "thresh_Seed_PS_Barrel", 
		   threshSeedPSBarrel);

  double threshPSEndcap = 0.0001;
  options_->GetOpt("clustering", "thresh_PS_Endcap", threshPSEndcap);

  double threshSeedPSEndcap = 0.001;
  options_->GetOpt("clustering", "thresh_Seed_PS_Endcap",
		   threshSeedPSEndcap);

  double showerSigmaPS    = 0.1;
  options_->GetOpt("clustering", "shower_Sigma_PS",
                   showerSigmaPS);
 
  int nNeighboursPS = 4;
  options_->GetOpt("clustering", "neighbours_PS", nNeighboursPS);

  int posCalcNCrystalPS = -1;
  options_->GetOpt("clustering", "posCalc_nCrystal_PS",
                   posCalcNCrystalPS);

  double posCalcP1PS = 0.;
  options_->GetOpt("clustering", "posCalc_p1_PS", 
		   posCalcP1PS);




  clusterAlgoPS_.setThreshBarrel( threshPSBarrel );
  clusterAlgoPS_.setThreshSeedBarrel( threshSeedPSBarrel );
  
  clusterAlgoPS_.setThreshEndcap( threshPSEndcap );
  clusterAlgoPS_.setThreshSeedEndcap( threshSeedPSEndcap );

  clusterAlgoPS_.setNNeighbours( nNeighboursPS );
  clusterAlgoPS_.setShowerSigma( showerSigmaPS );

  clusterAlgoPS_.setPosCalcNCrystal( posCalcNCrystalPS );
  clusterAlgoPS_.setPosCalcP1( posCalcP1PS );

  clusterAlgoPS_.enableDebugging( clusteringDebug ); 

  


  // options for particle flow ---------------------------------------------


  string map_ECAL_eta;
  options_->GetOpt("particle_flow", "resolution_map_ECAL_eta", map_ECAL_eta);
  string map_ECAL_phi;
  options_->GetOpt("particle_flow", "resolution_map_ECAL_phi", map_ECAL_phi);
  string map_ECALec_x;
  options_->GetOpt("particle_flow", "resolution_map_ECALec_x", map_ECALec_x);
  string map_ECALec_y;
  options_->GetOpt("particle_flow", "resolution_map_ECALec_y", map_ECALec_y);
  string map_HCAL_eta;
  options_->GetOpt("particle_flow", "resolution_map_HCAL_eta", map_HCAL_eta);
  string map_HCAL_phi;
  options_->GetOpt("particle_flow", "resolution_map_HCAL_phi", map_HCAL_phi);

  //getting resolution maps
  getMap(map_ECAL_eta);
  getMap(map_ECAL_phi);
  getMap(map_HCAL_eta);
  getMap(map_HCAL_phi);


  double chi2TrackECAL=100;
  options_->GetOpt("particle_flow", "chi2_ECAL_Track", chi2TrackECAL);
  double chi2TrackHCAL=100;
  options_->GetOpt("particle_flow", "chi2_HCAL_Track", chi2TrackHCAL);
  double chi2ECALHCAL=100;
  options_->GetOpt("particle_flow", "chi2_ECAL_HCAL", chi2ECALHCAL);


  pfBlockAlgo_.setParameters( map_ECAL_eta.c_str(),
			      map_ECAL_phi.c_str(),
			      map_HCAL_eta.c_str(),
			      map_HCAL_phi.c_str(),
			      chi2TrackECAL,
			      chi2TrackHCAL,
			      chi2ECALHCAL );
  
  bool blockAlgoDebug = false;
  options_->GetOpt("blockAlgo", "debug",  blockAlgoDebug);  

  pfBlockAlgo_.setDebug( blockAlgoDebug );



  double eCalibP0 = 0;
  double eCalibP1 = 1;
  vector<double> ecalib;
  options_->GetOpt("particle_flow", "ecalib", ecalib);
  if(ecalib.size() == 2) {
    eCalibP0 = ecalib[0];
    eCalibP1 = ecalib[1]; 
  }
  else {
    cerr<<"PFRootEventManager::readOptions : WARNING : "
	<<"wrong calibration coefficients for ECAL"<<endl;
  }

//   if(ecalib.size() == 2) {
// // CV: PFBlock::setEcalib(ecalib[0], ecalib[1]);
// //     std::cout << "setting ECAL calibration for electrons/photons:" <<std::endl;
// //     std::cout << " slope = " << ecalib[1] << std::endl;
// //     std::cout << " offset = " << ecalib[0] << std::endl;
// //     energyCalibration_->setCalibrationParametersEm(ecalib[1], ecalib[0]); 
//   } else {
// // CV: PFBlock::setEcalib(0, 1);
//     energyCalibration_->setCalibrationParametersEm(1., 0.);
// =======
//     eCalibP0 = ecalib[0];
//     eCalibP1 = ecalib[1]; 
// >>>>>>> 1.50.2.2
//   }
  

  double nSigmaECAL = 99999;
  options_->GetOpt("particle_flow", "nsigma_ECAL", nSigmaECAL);
  double nSigmaHCAL = 99999;
  options_->GetOpt("particle_flow", "nsigma_HCAL", nSigmaHCAL);


  pfAlgo_.setParameters( eCalibP0, eCalibP1, nSigmaECAL, nSigmaHCAL );

  displayJetColors_ = false;
  options_->GetOpt("display", "jet_colors", displayJetColors_);
  

  // print flags -------------

  printRecHits_ = false;
  options_->GetOpt("print", "rechits", printRecHits_ );
  
  printClusters_ = false;
  options_->GetOpt("print", "clusters", printClusters_ );
  
  printPFBlocks_ = true;
  options_->GetOpt("print", "PFBlocks", printPFBlocks_ );
  
  printPFCandidates_ = true;
  options_->GetOpt("print", "PFCandidates", printPFCandidates_ );
  
  printTrueParticles_ = true;
  options_->GetOpt("print", "true_particles", printTrueParticles_ );
  
  verbosity_ = VERBOSE;
  options_->GetOpt("print", "verbosity", verbosity_ );
  cout<<"verbosity : "<<verbosity_<<endl;

  // jets options ---------------------------------
  doJets_ = false;
  options_->GetOpt("jets", "dojets", doJets_);
  
  jetsDebug_ = false;
  
  if (doJets_) {
    double coneAngle = 0.5;
    options_->GetOpt("jets", "cone_angle", coneAngle);
    
    double seedEt    = 0.4;
    options_->GetOpt("jets", "seed_et", seedEt);
    
    double coneMerge = 100.0;
    options_->GetOpt("jets", "cone_merge", coneMerge);
    
    options_->GetOpt("jets", "jets_debug", jetsDebug_);

    // cout<<"jets debug "<<jetsDebug_<<endl;
    
    if( jetsDebug_ ) {
      cout << "Jet Options : ";
      cout << "Angle=" << coneAngle << " seedEt=" << seedEt 
	   << " Merge=" << coneMerge << endl;
    }

    jetAlgo_.SetConeAngle(coneAngle);
    jetAlgo_.SetSeedEt(seedEt);
    jetAlgo_.SetConeMerge(coneMerge);   
  }

}

void PFRootEventManager::connect( const char* infilename ) {

  string fname = infilename;
  if( fname.empty() ) 
    fname = inFileName_;

  
  cout<<"opening input root file"<<endl;

  options_->GetOpt("root","file", inFileName_);
  
  file_ = TFile::Open(inFileName_.c_str() );

  if(!file_ ) return;
  else if(file_->IsZombie() ) {
    return;
  }
  else 
    cout<<"rootfile "<<inFileName_
	<<" opened"<<endl;

  tree_ = (TTree*) file_->Get("Events");  
  if(!tree_) {
    cerr<<"PFRootEventManager::ReadOptions :";
    cerr<<"input TTree Events not found in file "
	<<inFileName_<<endl;
    return; 
  }
    
  // hits branches ----------------------------------------------

  string rechitsECALbranchname;
  options_->GetOpt("root","rechits_ECAL_branch", rechitsECALbranchname);
  
  rechitsECALBranch_ = tree_->GetBranch(rechitsECALbranchname.c_str());
  if(!rechitsECALBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : rechits_ECAL_branch not found : "
	<<rechitsECALbranchname<<endl;
  }
  else {
    rechitsECALBranch_->SetAddress(&rechitsECAL_);
  }

  string rechitsHCALbranchname;
  options_->GetOpt("root","rechits_HCAL_branch", rechitsHCALbranchname);
  
  rechitsHCALBranch_ = tree_->GetBranch(rechitsHCALbranchname.c_str());
  if(!rechitsHCALBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : rechits_HCAL_branch not found : "
	<<rechitsHCALbranchname<<endl;
  }
  else {
    rechitsHCALBranch_->SetAddress(&rechitsHCAL_);
  }

  string rechitsPSbranchname;
  options_->GetOpt("root","rechits_PS_branch", rechitsPSbranchname);
  
  rechitsPSBranch_ = tree_->GetBranch(rechitsPSbranchname.c_str());
  if(!rechitsPSBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : rechits_PS_branch not found : "
	<<rechitsPSbranchname<<endl;
  }
  else {
    rechitsPSBranch_->SetAddress(&rechitsPS_);
  }


  // clusters branches ----------------------------------------------

  string clustersECALbranchname;
  options_->GetOpt("root","clusters_ECAL_branch", clustersECALbranchname);

  clustersECALBranch_ = tree_->GetBranch(clustersECALbranchname.c_str());
  if(!clustersECALBranch_) {
    cerr <<"PFRootEventManager::ReadOptions : clusters_ECAL_branch not found:"
	 <<clustersECALbranchname<<endl;
  }
  else if(!clusteringIsOn_) {
    // cout<<"clusters ECAL : SetAddress"<<endl;
    clustersECALBranch_->SetAddress( clustersECAL_.get() );
  }    
  
  string clustersHCALbranchname;
  options_->GetOpt("root","clusters_HCAL_branch", clustersHCALbranchname);

  clustersHCALBranch_ = tree_->GetBranch(clustersHCALbranchname.c_str());
  if(!clustersHCALBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : clusters_HCAL_branch not found : "
        <<clustersHCALbranchname<<endl;
  }
  else if(!clusteringIsOn_) {
    clustersHCALBranch_->SetAddress( clustersHCAL_.get() );
  }    
  
  string clustersPSbranchname;
  options_->GetOpt("root","clusters_PS_branch", clustersPSbranchname);

  clustersPSBranch_ = tree_->GetBranch(clustersPSbranchname.c_str());
  if(!clustersPSBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : clusters_PS_branch not found : "
	<<clustersPSbranchname<<endl;
  }
  else if(!clusteringIsOn_) {
    clustersPSBranch_->SetAddress( clustersPS_.get() );
  }    
  
  // other branches ----------------------------------------------
  
  
  string clustersIslandBarrelbranchname;
  clustersIslandBarrelBranch_ = 0;
  options_->GetOpt("root","clusters_island_barrel_branch", 
		   clustersIslandBarrelbranchname);
  if(!clustersIslandBarrelbranchname.empty() ) {
    clustersIslandBarrelBranch_ 
      = tree_->GetBranch(clustersIslandBarrelbranchname.c_str());
    if(!clustersIslandBarrelBranch_) {
      cerr<<"PFRootEventManager::ReadOptions : clusters_island_barrel_branch not found : "
	  <<clustersIslandBarrelbranchname<< endl;
    }
    else {
      // cerr<<"setting address"<<endl;
      clustersIslandBarrelBranch_->SetAddress(&clustersIslandBarrel_);
    }    
  }
  else {
    cerr<<"branch not found: root/clusters_island_barrel_branch"<<endl;
  }

  string recTracksbranchname;
  options_->GetOpt("root","recTracks_branch", recTracksbranchname);

  recTracksBranch_ = tree_->GetBranch(recTracksbranchname.c_str());
  if(!recTracksBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : recTracks_branch not found : "
	<<recTracksbranchname<< endl;
  }
  else {
    recTracksBranch_->SetAddress(&recTracks_);
  }    

  string trueParticlesbranchname;
  options_->GetOpt("root","trueParticles_branch", trueParticlesbranchname);

  trueParticlesBranch_ = tree_->GetBranch(trueParticlesbranchname.c_str());
  if(!trueParticlesBranch_) {
    cerr<<"PFRootEventManager::ReadOptions : trueParticles_branch not found : "
	<<trueParticlesbranchname<< endl;
  }
  else {
    trueParticlesBranch_->SetAddress(&trueParticles_);
  }    

  string caloTowersBranchName;
  caloTowersBranch_ = 0;
  options_->GetOpt("root","caloTowers_branch", caloTowersBranchName);
  if(!caloTowersBranchName.empty() ) {
    caloTowersBranch_ = tree_->GetBranch(caloTowersBranchName.c_str()); 
    if(!caloTowersBranch_) {
      cerr<<"PFRootEventManager::ReadOptions : caloTowers_branch not found : "
	  <<caloTowersBranchName<< endl;
    }
    else {
      // cerr<<"setting address"<<endl;
      caloTowersBranch_->SetAddress(&caloTowers_);
    }           
  }    
} 

PFRootEventManager::~PFRootEventManager() {

  if(outFile_) {
    outFile_->Close();
  }

  if(outEvent_) delete outEvent_;
  if(outTree_)  delete outTree_;

  for( unsigned i=0; i<displayView_.size(); i++) {
    if(displayView_[i]) delete displayView_[i];
  }

  delete options_;
  
//   delete energyCalibration_;
//   PFBlock::setEnergyCalibration(NULL);
//   delete energyResolution_;
//   PFBlock::setEnergyResolution(NULL);
}


void PFRootEventManager::write() {
  if(!outFile_) return;
  else {
    outFile_->cd(); 
    // write histos here
    cout<<"writing output to "<<outFile_->GetName()<<endl;
    h_deltaETvisible_MCEHT_->Write();
    h_deltaETvisible_MCPF_->Write();
    if(outTree_) outTree_->Write();
  }
}


bool PFRootEventManager::processEntry(int entry) {

  reset();
 
  if( outEvent_ ) outEvent_->setNumber(entry);

  if(verbosity_ == VERBOSE  || 
     entry%10 == 0) 
    cout<<"process entry "<< entry << endl;
  

//   if(fromRealData_) {
//     if( !readFromRealData(entry) ) return false;
//   }
//   else {
//     if(! readFromSimulation(entry) ) return false;
//   } 

  if(! readFromSimulation(entry) ) return false;


  if(verbosity_ == VERBOSE ) {
    cout<<"number of recTracks      : "<<recTracks_.size()<<endl;
    cout<<"number of true particles : "<<trueParticles_.size()<<endl;
    cout<<"number of ECAL rechits   : "<<rechitsECAL_.size()<<endl;
    cout<<"number of HCAL rechits   : "<<rechitsHCAL_.size()<<endl;
    cout<<"number of PS rechits     : "<<rechitsPS_.size()<<endl;
  }  

  if( clusteringIsOn_ ) clustering(); 
  else if( verbosity_ == VERBOSE )
    cout<<"clustering is OFF - clusters come from the input file"<<endl; 

  if(verbosity_ == VERBOSE ) {
    if(clustersECAL_.get() ) {
      cout<<"number of ECAL clusters : "<<clustersECAL_->size()<<endl;
    }
    if(clustersHCAL_.get() ) {
      cout<<"number of HCAL clusters : "<<clustersHCAL_->size()<<endl;
    }
    if(clustersPS_.get() ) {
      cout<<"number of PS clusters : "<<clustersPS_->size()<<endl;
    }
  }

  particleFlow();
  if (doJets_) 
    makeJets(); 
  
  if(outTree_) outTree_->Fill();
  
  return true;
  //  if(trueParticles_.size() != 1 ) return false;


}



bool PFRootEventManager::readFromSimulation(int entry) {

  if(!tree_) return false;
  
  // in versions 12X, the rechit neihgbours are stored as a vector
  // of detIds, not as a vector of indices. 
  // in this case, one has to 
  // bool is12X = false;
  //   unsigned pos = releaseVersion_.find("CMSSW_1_2");
  //   if(pos != static_cast<unsigned>(-1) ) {
  //     cout<<"is12X true "<<pos<<endl;
  //     is12X = true;
  //   }


  if(trueParticlesBranch_ ) {
    trueParticlesBranch_->GetEntry(entry);
    // this is a filter to select single particle events.
    // usually not active
    if(filterNParticles_ && 
       trueParticles_.size() != filterNParticles_ ) {
      return false;
    }
    if(filterHadronicTaus_ && !isHadronicTau() ) {
      cout << "PFRootEventManager : leptonic tau discarded " << endl; 
      return false;
    }
  }
  if(rechitsECALBranch_) {
    rechitsECALBranch_->GetEntry(entry);
    PreprocessRecHits( rechitsECAL_ , findRecHitNeighbours_);
  }
  if(rechitsHCALBranch_) {
    rechitsHCALBranch_->GetEntry(entry);
    PreprocessRecHits( rechitsHCAL_ , findRecHitNeighbours_);
  }
  if(rechitsPSBranch_) {
    rechitsPSBranch_->GetEntry(entry);  
    PreprocessRecHits( rechitsPS_ , findRecHitNeighbours_);
  }
  if(clustersECALBranch_ && !clusteringIsOn_) {
    clustersECALBranch_->GetEntry(entry);
    for(unsigned i=0; i<clustersECAL_->size(); i++) 
      (*clustersECAL_)[i].calculatePositionREP();
  }
  if(clustersHCALBranch_ && !clusteringIsOn_) {
    clustersHCALBranch_->GetEntry(entry);
    for(unsigned i=0; i<clustersHCAL_->size(); i++) 
      (*clustersHCAL_)[i].calculatePositionREP();    
  }
  if(clustersPSBranch_ && !clusteringIsOn_) {
    clustersPSBranch_->GetEntry(entry);
    for(unsigned i=0; i<clustersPS_->size(); i++) 
      (*clustersPS_)[i].calculatePositionREP();    
  }
  if(clustersIslandBarrelBranch_) {
    clustersIslandBarrelBranch_->GetEntry(entry);
  }
  if(caloTowersBranch_) {
    caloTowersBranch_->GetEntry(entry);
    if(verbosity_ == VERBOSE )
      cout<<"number of calotowers :"<<caloTowers_.size()<<endl;
  }
    
    
  if(recTracksBranch_) recTracksBranch_->GetEntry(entry);

  return true;
}


bool PFRootEventManager::isHadronicTau() const {

  for ( unsigned i=0;  i < trueParticles_.size(); i++) {
    const reco::PFSimParticle& ptc = trueParticles_[i];
    const std::vector<int>& ptcdaughters = ptc.daughterIds();
    if (abs(ptc.pdgCode()) == 15) {
      for ( unsigned int dapt=0; dapt < ptcdaughters.size(); ++dapt) {
	
	const reco::PFSimParticle& daughter 
	  = trueParticles_[ptcdaughters[dapt]];
	
	unsigned pdgdaugter = daughter.pdgCode();
	
	if (pdgdaugter == 11 || pdgdaugter == 13) { 
	  return false; 
	}//electron or muons?
      }//loop daughter
    }//tau
  }//loop particles


  return true;
}



void 
PFRootEventManager::PreprocessRecHits(reco::PFRecHitCollection& rechits, 
				      bool findNeighbours) {
  
 
  map<unsigned, unsigned> detId2index;

  for(unsigned i=0; i<rechits.size(); i++) { 
    rechits[i].calculatePositionREP();
    
    if(findNeighbours) 
      detId2index.insert( make_pair(rechits[i].detId(), i) );
  }
  
  if(findNeighbours) {
    for(unsigned i=0; i<rechits.size(); i++) { 
      setRecHitNeigbours( rechits[i], detId2index ); 
    }
  }
}


void PFRootEventManager::setRecHitNeigbours
( reco::PFRecHit& rh, 
  const map<unsigned, unsigned>& detId2index ) {

  rh.clearNeighbours();

  vector<unsigned> neighbours4DetId = rh.neighboursIds4();
  vector<unsigned> neighbours8DetId = rh.neighboursIds8();
  
  for( unsigned i=0; i<neighbours4DetId.size(); i++) {
    unsigned detId = neighbours4DetId[i];
//     cout<<"finding n for detId "<<detId<<endl;
    const map<unsigned, unsigned>::const_iterator& it = detId2index.find(detId);
    
    if(it != detId2index.end() ) {
//       cout<<"found n index "<<it->second<<endl;
      rh.add4Neighbour( it->second );
    }
  }

  for( unsigned i=0; i<neighbours8DetId.size(); i++) {
    unsigned detId = neighbours8DetId[i];
//     cout<<"finding n for detId "<<detId<<endl;
    const map<unsigned, unsigned>::const_iterator& it = detId2index.find(detId);
    
    if(it != detId2index.end() ) {
//       cout<<"found n index "<<it->second<<endl;
      rh.add8Neighbour( it->second );
    }
  }

  
}


void PFRootEventManager::clustering() {
  
  // ECAL clustering -------------------------------------------

  clusterAlgoECAL_.doClustering( rechitsECAL_ );
  clustersECAL_ = clusterAlgoECAL_.clusters();

  assert(clustersECAL_.get() );

  fillOutEventWithClusters( *clustersECAL_ );

  // HCAL clustering -------------------------------------------


  clusterAlgoHCAL_.doClustering( rechitsHCAL_ );
  clustersHCAL_ = clusterAlgoHCAL_.clusters();

  fillOutEventWithClusters( *clustersHCAL_ );

  // PS clustering -------------------------------------------

  clusterAlgoPS_.doClustering( rechitsPS_ );
  clustersPS_ = clusterAlgoPS_.clusters();

  fillOutEventWithClusters( *clustersPS_ );
  
}


void 
PFRootEventManager::fillOutEventWithClusters(const reco::PFClusterCollection& 
					     clusters) {

  if(!outEvent_) return;
  
  for(unsigned i=0; i<clusters.size(); i++) {
    EventColin::Cluster cluster;
    cluster.eta = clusters[i].positionXYZ().Eta();
    cluster.phi = clusters[i].positionXYZ().Phi();
    cluster.e = clusters[i].energy();
    cluster.layer = clusters[i].layer();
    cluster.type = clusters[i].type();
    outEvent_->addCluster(cluster);
  }   

}


void PFRootEventManager::particleFlow() {
  
  if( debug_) {
    cout<<"PFRootEventManager::particleFlow start"<<endl;
    cout<<"number of elements in memory: "
	<<reco::PFBlockElement::instanceCounter()<<endl;
  }


  pfBlockAlgo_.setInput( recTracks_, *clustersECAL_, *clustersHCAL_ );
  pfBlockAlgo_.findBlocks();
  
  if( debug_) cout<<pfBlockAlgo_<<endl;

  pfBlocks_ = pfBlockAlgo_.transferBlocks();


  pfAlgo_.reconstructParticles( *pfBlocks_ );
  if( debug_) cout<< pfAlgo_<<endl;
  pfCandidates_ = pfAlgo_.transferCandidates();
 
  if( debug_) cout<<"PFRootEventManager::particleFlow stop"<<endl;
}

void PFRootEventManager::makeJets() {
  //std::cout << "building jets from MC particles," 
  //    << "PF particles and caloTowers" << std::endl;
  
  //initialize Jets Reconstruction
  jetAlgo_.Clear();

  //MAKING TRUE PARTICLE JETS
  TLorentzVector partTOTMC;

  // colin: the following is not necessary
  // partTOTMC.SetPxPyPzE(0.0, 0.0, 0.0, 0.0);

  //MAKING JETS WITH TAU DAUGHTERS
  vector<reco::PFSimParticle> vectPART;
  for ( unsigned i=0;  i < trueParticles_.size(); i++) {
    const reco::PFSimParticle& ptc = trueParticles_[i];
    vectPART.push_back(ptc);
  }//loop

  for ( unsigned i=0;  i < trueParticles_.size(); i++) {
    const reco::PFSimParticle& ptc = trueParticles_[i];
    const std::vector<int>& ptcdaughters = ptc.daughterIds();

    if (abs(ptc.pdgCode()) == 15) {
      for ( unsigned int dapt=0; dapt < ptcdaughters.size(); ++dapt) {

	const reco::PFTrajectoryPoint& tpatvtx 
	  = vectPART[ptcdaughters[dapt]].trajectoryPoint(0);
	TLorentzVector partMC;
	partMC.SetPxPyPzE(tpatvtx.momentum().Px(),
			  tpatvtx.momentum().Py(),
			  tpatvtx.momentum().Pz(),
			  tpatvtx.momentum().E());

	partTOTMC += partMC;
	if (jetsDebug_) {
	  //pdgcode
	  int pdgcode = vectPART[ptcdaughters[dapt]].pdgCode();
	  cout << pdgcode << endl;
	  cout << tpatvtx << endl;
	  cout << partMC.Px() << " " << partMC.Py() << " " 
	       << partMC.Pz() << " " << partMC.E()
	       << " PT=" 
	       << sqrt(partMC.Px()*partMC.Px()+partMC.Py()*partMC.Py()) 
	       << endl;
	}//debug
      }//loop daughter
    }//tau?
  }//loop particles

  EventColin::Jet jetmc;
  jetmc.eta = partTOTMC.Eta();
  jetmc.phi = partTOTMC.Phi();
  jetmc.et = partTOTMC.Et();
  jetmc.e = partTOTMC.E();
  
  if(outEvent_) outEvent_->addJetMC( jetmc );

  /*
  //MC JETS RECONSTRUCTION (visible energy)
  for ( unsigned i=0;  i < trueParticles_.size(); i++) {
    const reco::PFSimParticle& ptc = trueParticles_[i];
    const std::vector<int>& ptcdaughters = ptc.daughterIds();
    
    //PARTICULE NOT DISINTEGRATING BEFORE ECAL
    if(ptcdaughters.size() != 0) continue;
    
    //TAKE INFO AT VERTEX //////////////////////////////////////////////////
    const reco::PFTrajectoryPoint& tpatvtx = ptc.trajectoryPoint(0);
    TLorentzVector partMC;
    partMC.SetPxPyPzE(tpatvtx.momentum().Px(),
		      tpatvtx.momentum().Py(),
		      tpatvtx.momentum().Pz(),
		      tpatvtx.momentum().E());
    
    partTOTMC += partMC;
    if (jetsDebug_) {
      //pdgcode
      int pdgcode = ptc.pdgCode();
      cout << pdgcode << endl;
      cout << tpatvtx << endl;
      cout << partMC.Px() << " " << partMC.Py() << " " 
      << partMC.Pz() << " " << partMC.E() 
	   << " PT=" 
	   << sqrt(partMC.Px()*partMC.Px()+partMC.Py()*partMC.Py()) 
	   << endl;
    }//debug?
  }//loop true particles
  */
  if (jetsDebug_) {
    cout << " ET Vector=" << partTOTMC.Et() 
	 << " " << partTOTMC.Eta() 
	 << " " << partTOTMC.Phi() << endl; cout << endl;
  }//debug

  //////////////////////////////////////////////////////////////////////////
  //CALO TOWER JETS (ECAL+HCAL Towers)
  //cout << endl;  
  //cout << "THERE ARE " << caloTowers_.size() << " CALO TOWERS" << endl;
  vector<TLorentzVector> allcalotowers;
  for ( unsigned int i = 0; i < caloTowers_.size(); ++i)
    {
      TLorentzVector caloT;
      TVector3 pepr( caloTowers_[i].eta(),
		     caloTowers_[i].phi(),
		     caloTowers_[i].energy());
      TVector3 pxyz = Utils::VectorEPRtoXYZ( pepr );
      caloT.SetPxPyPzE(pxyz.X(),pxyz.Y(),pxyz.Z(),caloTowers_[i].energy());
      allcalotowers.push_back(caloT);
    }//loop calo towers
  if ( jetsDebug_)  
    cout << " RETRIEVED " << allcalotowers.size() 
	 << " CALOTOWER 4-VECTORS " << endl;
  
  //ECAL+HCAL tower jets computation
  jetAlgo_.Clear();
  const vector< PFJetAlgorithm::Jet >&  caloTjets 
    = jetAlgo_.FindJets( &allcalotowers );
  
  //cout << caloTjets.size() << " CaloTower Jets found" << endl;
  double JetEHTETmax = 0.0;
  for ( unsigned i = 0; i < caloTjets.size(); i++) {
    TLorentzVector jetmom = caloTjets[i].GetMomentum();
    double jetcalo_pt = sqrt(jetmom.Px()*jetmom.Px()+jetmom.Py()*jetmom.Py());
    double jetcalo_et = jetmom.Et();

    EventColin::Jet jet;
    jet.eta = jetmom.Eta();
    jet.phi = jetmom.Phi();
    jet.et  = jetmom.Et();
    jet.e   = jetmom.E();
    
    if(outEvent_) outEvent_->addJetEHT( jet );

    if ( jetsDebug_) {
      cout << " ECAL+HCAL jet : " << caloTjets[i] << endl;
      cout << jetmom.Px() << " " << jetmom.Py() << " " 
	   << jetmom.Pz() << " " << jetmom.E() 
	   << " PT=" << jetcalo_pt << endl;
    }//debug

    if (jetcalo_et >= JetEHTETmax) 
      JetEHTETmax = jetcalo_et;
  }//loop MCjets

  //////////////////////////////////////////////////////////////////
  //PARTICLE FLOW JETS
  vector<TLorentzVector> allrecparticles;
//   if ( jetsDebug_) {
//     cout << endl;
//     cout << " THERE ARE " << pfBlocks_.size() << " EFLOW BLOCKS" << endl;
//   }//debug

//   for ( unsigned iefb = 0; iefb < pfBlocks_.size(); iefb++) {
//       const std::vector< PFBlockParticle >& recparticles 
// 	= pfBlocks_[iefb].particles();

  
  for(unsigned i=0; i<pfCandidates_->size(); i++) {
  
//       if (jetsDebug_) 
// 	cout << " there are " << recparticles.size() 
// 	     << " particle in this block" << endl;
    
    const reco::PFCandidate& candidate = (*pfCandidates_)[i];

    if (jetsDebug_) {
      cout << i << " " << candidate << endl;
      int type = candidate.particleId();
      cout << " type= " << type << " " << candidate.charge() 
	   << endl;
    }//debug

    const math::XYZTLorentzVector& PFpart = candidate.p4();
    
    TLorentzVector partRec(PFpart.Px(), 
			   PFpart.Py(), 
			   PFpart.Pz(),
			   PFpart.E());
    
    //loading 4-vectors of Rec particles
    allrecparticles.push_back( partRec );

  }//loop on candidates
  

  if (jetsDebug_) 
    cout << " THERE ARE " << allrecparticles.size() 
	 << " RECONSTRUCTED 4-VECTORS" << endl;

  jetAlgo_.Clear();
  const vector< PFJetAlgorithm::Jet >&  PFjets 
    = jetAlgo_.FindJets( &allrecparticles );

  if (jetsDebug_) 
    cout << PFjets.size() << " PF Jets found" << endl;
  double JetPFETmax = 0.0;
  for ( unsigned i = 0; i < PFjets.size(); i++) {
    TLorentzVector jetmom = PFjets[i].GetMomentum();
    double jetpf_pt = sqrt(jetmom.Px()*jetmom.Px()+jetmom.Py()*jetmom.Py());
    double jetpf_et = jetmom.Et();

    EventColin::Jet jet;
    jet.eta = jetmom.Eta();
    jet.phi = jetmom.Phi();
    jet.et  = jetmom.Et();
    jet.e   = jetmom.E();

    if(outEvent_) outEvent_->addJetPF( jet );

    if (jetsDebug_) {
      cout <<" Rec jet : "<< PFjets[i] <<endl;
      cout << jetmom.Px() << " " << jetmom.Py() << " " 
	   << jetmom.Pz() << " " << jetmom.E() 
	   << " PT=" << jetpf_pt << " eta="<< jetmom.Eta() 
	   << " Phi=" << jetmom.Phi() << endl;
      cout << "------------------------------------------------" << endl;
    }//debug
    
    if (jetpf_et >= JetPFETmax)  
      JetPFETmax = jetpf_et;
  }//loop PF jets

  //fill histos
  h_deltaETvisible_MCEHT_->Fill(JetEHTETmax - partTOTMC.Et());
  h_deltaETvisible_MCPF_ ->Fill(JetPFETmax - partTOTMC.Et());
}//Makejets



void PFRootEventManager::display(int ientry) {
  processEntry(ientry);
  display();
}



void PFRootEventManager::displayNext(bool init) {

  static int ientry=0;
  if( init ) ientry=0; // restarting from 0

  bool ok = false;
  while(!ok && ientry<tree_->GetEntries() ) {
    ok = processEntry(ientry);
    // iCurrentEntry_=ientry;
    ientry++;
  }
  display();
}


void PFRootEventManager::display() {
  if(displayRZ_) displayView(RZ);
  if(displayXY_) displayView(XY);
  if(displayEtaPhi_) { 
    displayView(EPE);
    displayView(EPH);
  }  
}


void PFRootEventManager::displayView(unsigned viewType) {
  

  // display or clear canvas
  if(!displayView_[viewType] 
     || !gROOT->GetListOfCanvases()->FindObject(displayView_[viewType]) ) {
    assert(viewSize_.size() == 2);
    switch(viewType) {
    case XY:
      displayView_[viewType] = new TCanvas("displayXY_", "XY view",
					   viewSize_[0], viewSize_[1]);
      break;
    case RZ:
      displayView_[viewType] = new TCanvas("displayRZ_", "RZ view",
					   viewSize_[0], viewSize_[1]);
      break;
    case EPE:
      displayView_[viewType] = new TCanvas("displayEPE_", "eta/phi view, ECAL",
					   viewSize_[0], viewSize_[1]);
      break;
    case EPH:
      displayView_[viewType] = new TCanvas("displayEPH_", "eta/phi view, HCAL",
					   viewSize_[0], viewSize_[1]);
      break;
    }
    displayView_[viewType]->SetGrid(0, 0);
    displayView_[viewType]->SetLeftMargin(0.12);
    displayView_[viewType]->SetBottomMargin(0.12);
    displayView_[viewType]->Draw();  
    displayView_[viewType]->ToggleToolBar();
  } else 
    displayView_[viewType]->Clear();
  displayView_[viewType]->cd();
  
  // Draw support histogram
  double zLow = -500.;
  double zUp  = +500.;
  double rLow = -300.;
  double rUp  = +300.;
  if(!displayHist_[viewType]) {
    switch(viewType) {
    case XY:
      displayHist_[viewType] = new TH2F("hdisplayHist_XY", "", 
					500, rLow, rUp, 500, rLow, rUp);
      displayHist_[viewType]->SetXTitle("X");
      displayHist_[viewType]->SetYTitle("Y");
      break;
    case RZ:
      displayHist_[viewType] = new TH2F("hdisplayHist_RZ", "", 
					500, zLow, zUp, 500, rLow, rUp);
      displayHist_[viewType]->SetXTitle("Z");
      displayHist_[viewType]->SetYTitle("R");
      break;      
    case EPE:
    case EPH:
      if(! displayHist_[EPE] ) {
	displayHist_[EPE] = new TH2F("hdisplayHist_EP", "", 
				     500, -5, 5, 500, -3.5, 3.5);
	displayHist_[EPE]->SetXTitle("#eta");
	displayHist_[EPE]->SetYTitle("#phi");
      }
      displayHist_[EPH] = displayHist_[EPE];
      break;
    default:
      std::cerr << "This kind of view is not implemented" << std::endl;
      break;
    }
    displayHist_[viewType]->SetStats(kFALSE);
  }
  displayHist_[viewType]->Draw();

  switch(viewType) {
  case XY:
    { 
      // Draw ECAL front face
      frontFaceECALXY_.SetX1(0);
      frontFaceECALXY_.SetY1(0);
      frontFaceECALXY_.SetR1(PFGeometry::innerRadius(PFGeometry::ECALBarrel));
      frontFaceECALXY_.SetR2(PFGeometry::innerRadius(PFGeometry::ECALBarrel));
      frontFaceECALXY_.SetFillStyle(0);
      frontFaceECALXY_.Draw();
      
      // Draw HCAL front face
      frontFaceHCALXY_.SetX1(0);
      frontFaceHCALXY_.SetY1(0);
      frontFaceHCALXY_.SetR1(PFGeometry::innerRadius(PFGeometry::HCALBarrel));
      frontFaceHCALXY_.SetR2(PFGeometry::innerRadius(PFGeometry::HCALBarrel));
      frontFaceHCALXY_.SetFillStyle(0);
      frontFaceHCALXY_.Draw();
      break;
    }
  case RZ:
    {
      // Draw lines at different etas
      TLine l;
      l.SetLineColor(1);
      l.SetLineStyle(3);
      TLatex etaLeg;
      etaLeg.SetTextSize(0.02);
      float etaMin = -3.;
      float etaMax = +3.;
      float etaBin = 0.2;
      int nEtas = int((etaMax - etaMin)/0.2) + 1;
      for (int iEta = 0; iEta <= nEtas; iEta++) {
	float eta = etaMin + iEta*etaBin;
	float r = 0.9*rUp;
	TVector3 etaImpact;
	etaImpact.SetPtEtaPhi(r, eta, 0.);
	etaLeg.SetTextAlign(21);
	if (eta <= -1.39) {
	  etaImpact.SetXYZ(0.,0.85*zLow*tan(etaImpact.Theta()),0.85*zLow);
	  etaLeg.SetTextAlign(31);
	} else if (eta >= 1.39) {
	  etaImpact.SetXYZ(0.,0.85*zUp*tan(etaImpact.Theta()),0.85*zUp);
	  etaLeg.SetTextAlign(11);
	}
	l.DrawLine(0., 0., etaImpact.Z(), etaImpact.Perp());
	etaLeg.DrawLatex(etaImpact.Z(), etaImpact.Perp(), Form("%2.1f", eta));
      }
      
      frontFaceECALRZ_.SetX1(-1.*PFGeometry::innerZ(PFGeometry::ECALEndcap));
      frontFaceECALRZ_.SetY1(-1.*PFGeometry::innerRadius(PFGeometry::ECALBarrel));
      frontFaceECALRZ_.SetX2(PFGeometry::innerZ(PFGeometry::ECALEndcap));
      frontFaceECALRZ_.SetY2(PFGeometry::innerRadius(PFGeometry::ECALBarrel));
      frontFaceECALRZ_.SetFillStyle(0);
      frontFaceECALRZ_.Draw();
      break;
    }
  default:
    // do nothing for other views
    break;
  }

  double phi0 = 0.;

  // display reconstructed objects
  displayView_[viewType]->cd();
  displayRecHits(viewType, phi0);
  displayClusters(viewType, phi0);
  if(displayRecTracks_) displayRecTracks(viewType, phi0);
  if(displayTrueParticles_) displayTrueParticles(viewType, phi0);
}



void PFRootEventManager::displayRecHits(unsigned viewType, double phi0) {
  double maxee = getMaxEEcal();
  double maxeh = getMaxEHcal();
  double maxe = maxee>maxeh ? maxee : maxeh;
  
  int color = TColor::GetColor(220, 220, 255);
  int seedcolor = TColor::GetColor(160, 160, 255);
  
  for(unsigned i=0; i<rechitsECAL_.size(); i++) { 
    int rhcolor = color;
    if(clusterAlgoECAL_.isSeed(i) )
      rhcolor = seedcolor;
    displayRecHit(rechitsECAL_[i], viewType, maxe, phi0, rhcolor);
  }
  for(unsigned i=0; i<rechitsHCAL_.size(); i++) { 
    int rhcolor = color;
    if(clusterAlgoHCAL_.isSeed(i) )
      rhcolor = seedcolor;
    displayRecHit(rechitsHCAL_[i], viewType, maxe, phi0, rhcolor);
  }
  for(unsigned i=0; i<rechitsPS_.size(); i++) { 
    int rhcolor = color;
    if(clusterAlgoPS_.isSeed(i) )
      rhcolor = seedcolor;
    displayRecHit(rechitsPS_[i], viewType, maxe, phi0, rhcolor);
  }   
}


void PFRootEventManager::displayRecHit(reco::PFRecHit& rh, 
				       unsigned viewType,
				       double maxe, 
				       double phi0, 
				       int color) {
  double me = maxe;
  double thresh = 0;
  int layer = rh.layer();

  switch(layer) {
  case PFLayer::ECAL_BARREL:
    thresh = clusterAlgoECAL_.threshBarrel();
    break;     
  case PFLayer::ECAL_ENDCAP:
    thresh = clusterAlgoECAL_.threshEndcap();
    break;     
  case PFLayer::HCAL_BARREL1:
  case PFLayer::HCAL_BARREL2:
    thresh = clusterAlgoHCAL_.threshBarrel();
    break;           
  case PFLayer::HCAL_ENDCAP:
    thresh = clusterAlgoHCAL_.threshEndcap();
    break;           
  case PFLayer::PS1:
  case PFLayer::PS2:
    me = -1;
    thresh = clusterAlgoPS_.threshBarrel();
    break;
  default:
    cerr<<"PFRootEventManager::displayRecHit : manage other layers."
	<<" Rechit not drawn."<<endl;
    return;
  }
  
  if( rh.energy() < thresh ) return;


  // on EPH view, draw only HCAL
  if(  viewType == EPH && 
       ! (layer == PFLayer::HCAL_BARREL1 || 
	  layer == PFLayer::HCAL_BARREL2 || 
	  layer == PFLayer::HCAL_ENDCAP ) ) return;
  
  // on EPE view, draw only HCAL and preshower
  if(  viewType == EPE && 
       (layer == PFLayer::HCAL_BARREL1 || 
	layer == PFLayer::HCAL_BARREL2 || 
	layer == PFLayer::HCAL_ENDCAP ) ) return;
    

  double rheta = rh.positionREP().Eta();
  double rhphi = rh.positionREP().Phi();
  //  if( !insideGCut( rheta, rhphi ) ) return;



//   if( abs(rheta - 1.69) > 0.05 ||
//       abs(rhphi + 1.61) > 0.05 || 
//       layer<0) return;

  double sign = 1.;
  if (cos(phi0 - rhphi) < 0.) sign = -1.;


  double etaSize[4];
  double phiSize[4];
  double x[5];
  double y[5];
  double z[5];
  double r[5];
  double eta[5];
  double phi[5];
  double xprop[5];
  double yprop[5];
  double etaprop[5];
  double phiprop[5];

  
  const std::vector< math::XYZPoint >& corners = rh.getCornersXYZ();
  
  assert(corners.size() == 4);

  
  double propfact = 0.95; // so that the cells don't overlap ? 
  
  double ampl=0;
  if(me>0) ampl = (log(rh.energy() + 1.)/log(me + 1.));

  for ( unsigned jc=0; jc<4; ++jc ) { 

    // cout<<"corner "<<jc<<" "<<corners[jc].Eta()<<" "<<corners[jc].Phi()<<endl;

    phiSize[jc] = rhphi-corners[jc].Phi();
    etaSize[jc] = rheta-corners[jc].Eta();
    if ( phiSize[jc] > 1. ) phiSize[jc] -= 2.*TMath::Pi();  // this is strange...
    if ( phiSize[jc] < -1. ) phiSize[jc]+= 2.*TMath::Pi();
    phiSize[jc] *= propfact;
    etaSize[jc] *= propfact;

    math::XYZPoint cornerposxyz = corners[jc];

    x[jc] = cornerposxyz.X();
    y[jc] = cornerposxyz.Y();
    z[jc] = cornerposxyz.Z();
    r[jc] = sign*cornerposxyz.Rho();
    eta[jc] = rheta - etaSize[jc];
    phi[jc] = rhphi - phiSize[jc];
    

    // cell area is prop to log(E)
    // not drawn for preshower. 
    // otherwise, drawn for eta/phi view, and for endcaps in xy view
    if( layer != PFLayer::PS1 && 
	layer != PFLayer::PS2 && 
	( viewType == EPE || 
	  viewType == EPH || 
	  ( viewType == XY &&  
	    ( layer == PFLayer::ECAL_ENDCAP || 
	      layer == PFLayer::HCAL_ENDCAP ) ) ) ) {
      
      
      math::XYZPoint centreXYZrot = rh.positionXYZ();

      math::XYZPoint centertocorner(x[jc] - centreXYZrot.X(), 
				    y[jc] - centreXYZrot.Y(),
				    0 );

      math::XYZPoint centertocornerep(eta[jc] - centreXYZrot.Eta(), 
				      phi[jc] - centreXYZrot.Phi(),
				      0 );
      

      // centertocorner -= centreXYZrot;
      xprop[jc] = centreXYZrot.X() + centertocorner.X()*ampl;
      yprop[jc] = centreXYZrot.Y() + centertocorner.Y()*ampl;

      etaprop[jc] = centreXYZrot.Eta() + centertocornerep.X()*ampl;
      phiprop[jc] = centreXYZrot.Phi() + centertocornerep.Y()*ampl;
    }
  }

  if(layer == PFLayer::ECAL_BARREL  || 
     layer == PFLayer::HCAL_BARREL1 || 
     layer == PFLayer::HCAL_BARREL2 || viewType == RZ) {

    // we are in the barrel. Determining which corners to shift 
    // away from the center to represent the cell energy
    
    int i1 = -1;
    int i2 = -1;

    if(fabs(phiSize[1]-phiSize[0]) > 0.0001) {
      if (viewType == XY) {
	i1 = 2;
	i2 = 3;
      } else if (viewType == RZ) {
	i1 = 1;
	i2 = 2;
      }
    } else {
      if (viewType == XY) {
	i1 = 1;
	i2 = 2;
      } else if (viewType == RZ) {
	i1 = 2;
	i2 = 3;
      }
    }

    x[i1] *= 1+ampl/2.;
    x[i2] *= 1+ampl/2.;
    y[i1] *= 1+ampl/2.;
    y[i2] *= 1+ampl/2.;
    z[i1] *= 1+ampl/2.;
    z[i2] *= 1+ampl/2.;
    r[i1] *= 1+ampl/2.;
    r[i2] *= 1+ampl/2.;
  }

  
  x[4]=x[0];
  y[4]=y[0]; // closing the polycell
  z[4]=z[0];
  r[4]=r[0]; // closing the polycell
  eta[4]=eta[0];
  phi[4]=phi[0]; // closing the polycell
  



  
  switch( viewType ) {
  case  XY:
    {
      TPolyLine lineSizeXY;
      TPolyLine linePropXY;          
      if(layer == PFLayer::ECAL_BARREL || 
	 layer == PFLayer::HCAL_BARREL1 || 
	 layer == PFLayer::HCAL_BARREL2) {
	lineSizeXY.SetLineColor(color);
	//cout << "x,y " << x[0] << " " << y[0] << endl;
	lineSizeXY.SetFillColor(color);
	lineSizeXY.DrawPolyLine(5,x,y,"f");
      } else {
	//cout << "x,y " << x[0] << " " << y[0] << endl;
	lineSizeXY.SetLineColor(color);
	lineSizeXY.DrawPolyLine(5,x,y);
	
	if( ampl>0 ) { // not for preshower
	  xprop[4]=xprop[0];
	  yprop[4]=yprop[0]; // closing the polycell    
	  linePropXY.SetLineColor(color);
	  linePropXY.SetFillColor(color);
	  linePropXY.DrawPolyLine(5,xprop,yprop,"F");
	}
      }
      break;
    }
  case RZ:
    {
      TPolyLine lineSizeRZ;
      lineSizeRZ.SetLineColor(color);
      lineSizeRZ.SetFillColor(color);
      // cout << "z,r " << z[0] << " " << r[0] << endl;
      lineSizeRZ.DrawPolyLine(5,z,r,"f");
      break;
    }
  case EPE:
  case EPH:
    {
      TPolyLine lineSizeEP;
      TPolyLine linePropEP;          
      
      lineSizeEP.SetLineColor(color);
      lineSizeEP.SetFillColor(color);
      lineSizeEP.DrawPolyLine(5,eta,phi);
      
      if( ampl>0 ) { // not for preshower
	etaprop[4]=etaprop[0];
	phiprop[4]=phiprop[0]; // closing the polycell    
	linePropEP.SetLineColor(color);
	linePropEP.SetFillColor(color);
	linePropEP.DrawPolyLine(5,etaprop,phiprop,"F");
      }
      break;
    }
  default:
    break;
    
  }
}


void PFRootEventManager::displayClusters(unsigned viewType, double phi0) {
  
  // std::vector<reco::PFCluster>::iterator itCluster;
  // for(itCluster = clusters_->begin(); itCluster != clusters_->end(); 
  //     itCluster++) {
  for(unsigned i=0; i<clustersECAL_->size(); i++) 
    displayCluster( (*clustersECAL_)[i], viewType, phi0);
  for(unsigned i=0; i<clustersHCAL_->size(); i++) 
    displayCluster( (*clustersHCAL_)[i], viewType, phi0);
  for(unsigned i=0; i<clustersPS_->size(); i++) 
    displayCluster( (*clustersPS_)[i], viewType, phi0);

  for(unsigned i=0; i<clustersIslandBarrel_.size(); i++) {
    int id = i;
    int type = 4;
    int layer = PFLayer::ECAL_BARREL;
    reco::PFCluster cluster( id, type, layer, 
			     clustersIslandBarrel_[i].energy(), 
			     clustersIslandBarrel_[i].x(),
			     clustersIslandBarrel_[i].y(),
			     clustersIslandBarrel_[i].z() ); 
    displayCluster( cluster, viewType, phi0);
  }
    
}



void PFRootEventManager::displayCluster(const reco::PFCluster& cluster,
					unsigned viewType, double phi0) {
  

  double eta = cluster.positionXYZ().Eta();
  double phi = cluster.positionXYZ().Phi();
  

  int type = cluster.type();
  if(algosToDisplay_.find(type) == algosToDisplay_.end() )
    return;

  if( !insideGCut( eta, phi ) ) return;

  TMarker m;

  int color = 4;
  if( displayColorClusters_ ) 
    color = cluster.type();

  m.SetMarkerColor(color);
  m.SetMarkerStyle(20);  


  const math::XYZPoint& xyzPos = cluster.positionXYZ();

  switch(viewType) {
  case XY:
    m.DrawMarker(xyzPos.X(), xyzPos.Y());
    break;
  case RZ:
    {
      double sign = 1.;
      if (cos(phi0 - phi) < 0.)
	sign = -1.;
      m.DrawMarker(xyzPos.Z(), sign*xyzPos.Rho());
      break;
    }
  case EPE:
    if( cluster.layer()<0 ) {
      m.DrawMarker(eta, phi);
      if( displayClusterLines_ ) displayClusterLines(cluster);
    }
    break;
  case EPH:
    if( cluster.layer()>0 ) {
      m.DrawMarker(eta, phi);
      if( displayClusterLines_ ) displayClusterLines(cluster);
    }
    break;
  }      
}


void PFRootEventManager::displayClusterLines(const reco::PFCluster& cluster) {
  

  const math::XYZPoint& xyzPos = cluster.positionXYZ();
  double eta = xyzPos.Eta(); 
  double phi = xyzPos.Phi(); 
  
  const std::vector< reco::PFRecHitFraction >& rhfracs = 
    cluster.recHitFractions();

  TLine l;
  int color = cluster.type();
  l.SetLineColor( color );
  
//   PFClusterAlgo* algo=0;
//   switch( cluster.layer() ) {
//   case PFLayer::ECAL_BARREL:
//   case PFLayer::ECAL_ENDCAP:
//     algo = clusterAlgoECAL_;
//     break;       
//   case PFLayer::HCAL_BARREL1:
//   case PFLayer::HCAL_BARREL2:
//   case PFLayer::HCAL_ENDCAP:
//     algo = clusterAlgoHCAL_;
//     break;                     
//   case PFLayer::PS1:
//   case PFLayer::PS2:
//     algo = clusterAlgoPS_;
//     break;
//   default:
//     assert(0);
//     return;
//   }


  // draw a line from the cluster to each of the rechits
  for(unsigned i=0; i<rhfracs.size(); i++) {

    // rechit index 
    // unsigned rhi = rhfracs[i].recHitIndex();

    const reco::PFRecHit& rh = *(rhfracs[i].recHitRef());


    double rheta = rh.positionXYZ().Eta();
    double rhphi = rh.positionXYZ().Phi();
    l.DrawLine(eta,phi,rheta,rhphi);
  }
}



void PFRootEventManager::displayRecTracks(unsigned viewType, double phi0) {

  std::vector<reco::PFRecTrack>::iterator itRecTrack;
  for (itRecTrack = recTracks_.begin(); itRecTrack != recTracks_.end();
       itRecTrack++) {

    double sign = 1.;

    const reco::PFTrajectoryPoint& tpatecal 
      = itRecTrack->trajectoryPoint(itRecTrack->nTrajectoryMeasurements() +
				    reco::PFTrajectoryPoint::ECALEntrance );
    
    if ( cos(phi0 - tpatecal.momentum().Phi()) < 0.)
      sign = -1.;

    const std::vector<reco::PFTrajectoryPoint>& points = 
      itRecTrack->trajectoryPoints();

    int    linestyle = itRecTrack->algoType(); 
    int    markerstyle = 8;
    double markersize = 0.8;
    int    color = 103;
    
    displayTrack( points, viewType, phi0, sign, false,
		  linestyle, markerstyle, markersize, color );
  }
}



void PFRootEventManager::displayTrueParticles(unsigned viewType, double phi0) {

  for(unsigned i=0; i<trueParticles_.size(); i++) {
    
    const reco::PFSimParticle& ptc = trueParticles_[i];
    

    double sign = 1.;
    
    const reco::PFTrajectoryPoint& tpatecal 
      = ptc.trajectoryPoint(ptc.nTrajectoryMeasurements() +
			    reco::PFTrajectoryPoint::ECALEntrance );
    
    if ( cos(phi0 - tpatecal.momentum().Phi()) < 0.)
      sign = -1.;

    const std::vector<reco::PFTrajectoryPoint>& points = 
      ptc.trajectoryPoints();

    int markerstyle;
    switch( abs(ptc.pdgCode() ) ) {
    case 22:   markerstyle = 3 ;   break; // photons
    case 11:   markerstyle = 5 ;   break; // electrons 
    case 13:   markerstyle = 2 ;   break; // muons 
    case 130:  
    case 321:  markerstyle = 24;  break; // K
    case 211:  markerstyle = 25;  break; // pi+/pi-
    case 2212: markerstyle = 26;  break; // protons
    case 2112: markerstyle = 27;  break; // neutrons  
    default:   markerstyle = 30;  break; 
    }
   
    int    color = 4;
    int    linestyle = 2;
    double markersize = 0.8;
    
    bool displayInitial=true;
    if( ptc.motherId() < 0 ) displayInitial=false;

    displayTrack( points, viewType, phi0, 
		  sign, displayInitial, 
		  linestyle, markerstyle, markersize, 
		  color );
  }
}




void 
PFRootEventManager::displayTrack 
( const std::vector<reco::PFTrajectoryPoint>& points, 
  unsigned viewType, double phi0, double sign, bool displayInitial,
  int linestyle, 
  int markerstyle, double markersize, 
  int color) {
  
  // reserving space. nb not all trajectory points are valid

  vector<double> xPos;
  xPos.reserve( points.size() );
  vector<double> yPos;
  yPos.reserve( points.size() );

    
  bool inside = false; 

  for(unsigned i=0; i<points.size(); i++) {
    
    if( !points[i].isValid() ) continue;
	
    const math::XYZPoint& xyzPos = points[i].positionXYZ();      

    double eta = xyzPos.Eta();
    double phi = xyzPos.Phi();
    
    if( !displayInitial && 
	points[i].layer() == reco::PFTrajectoryPoint::ClosestApproach ) {
      const math::XYZTLorentzVector& mom = points[i].momentum();
      eta = mom.Eta();
      phi = mom.Phi();
    }
    
    if( insideGCut(eta, phi) ) 
      inside = true;
    

    switch(viewType) {
    case XY:
      xPos.push_back(xyzPos.X());
      yPos.push_back(xyzPos.Y());
      break;
    case RZ:
      xPos.push_back(xyzPos.Z());
      yPos.push_back(sign*xyzPos.Rho());
      break;
    case EPE:
    case EPH:
      xPos.push_back( eta );
      yPos.push_back( phi );
      
      // closest approach is meaningless in eta/phi     
//       if(!displayInitial && 
// 	 points[i].layer() == reco::PFTrajectoryPoint::ClosestApproach ) {
// 	const math::XYZTLorentzVector& mom = points[i].momentum();
// 	xPos.push_back(mom.Eta());
// 	yPos.push_back(mom.Phi());	  
//       }	  
//       else {
// 	xPos.push_back(xyzPos.Eta());
// 	yPos.push_back(xyzPos.Phi());
//       }
      break;
    }
  }  

  /// no point inside graphical cut.
  if( !inside ) return;

  TGraph graph;
  graph.SetLineStyle(linestyle);
  graph.SetMarkerStyle(markerstyle);
  graph.SetMarkerColor(color);
  graph.SetMarkerSize(markersize);
  graph.SetLineColor(color);
  graph.DrawGraph( xPos.size(), &xPos[0], &yPos[0], "pl" );
}


void PFRootEventManager::unZoom() {

  for( unsigned i=0; i<displayHist_.size(); i++) {

    // the corresponding view was not requested
    if( ! displayHist_[i] ) continue;

    displayHist_[i]->GetXaxis()->UnZoom();
    displayHist_[i]->GetYaxis()->UnZoom();
  }

  updateDisplay();
}



void PFRootEventManager::updateDisplay() {

  for( unsigned i=0; i<displayView_.size(); i++) {
    if( gROOT->GetListOfCanvases()->FindObject(displayView_[i]) )
    displayView_[i]->Modified();
  }
}


void PFRootEventManager::lookForMaxRecHit(bool ecal) {

  // look for the rechit with max e in ecal or hcal
  double maxe = -999;
  reco::PFRecHit* maxrh = 0;

  reco::PFRecHitCollection* rechits = 0;
  if(ecal) rechits = &rechitsECAL_;
  else rechits = &rechitsHCAL_;
  assert(rechits);

  for(unsigned i=0; i<(*rechits).size(); i++) {

    double energy = (*rechits)[i].energy();

    if(energy > maxe ) {
      maxe = energy;
      maxrh = &((*rechits)[i]);
    }      
  }
  
  if(!maxrh) return;

  // center view on this rechit


  // get the cell size to set the eta and phi width 
  // of the display window from one of the cells
  
  double phisize = -1;
  double etasize = -1;
  maxrh->size(phisize, etasize);
   
  double etagate = displayZoomFactor_ * etasize;
  double phigate = displayZoomFactor_ * phisize;
  
  double eta =  maxrh->positionREP().Eta();
  double phi =  maxrh->positionREP().Phi();
  

  if(displayHist_[EPE]) {
    displayHist_[EPE]->GetXaxis()->SetRangeUser(eta-etagate, eta+etagate);
    displayHist_[EPE]->GetYaxis()->SetRangeUser(phi-phigate, phi+phigate);
  }
  if(displayHist_[EPH]) {
    displayHist_[EPH]->GetXaxis()->SetRangeUser(eta-etagate, eta+etagate);
    displayHist_[EPH]->GetYaxis()->SetRangeUser(phi-phigate, phi+phigate);
  }
  
  updateDisplay();
}  



double PFRootEventManager::getMaxE(int layer) const {

  double maxe = -9999;

  // in which vector should we look for these rechits ?

  const reco::PFRecHitCollection* vec = 0;
  switch(layer) {
  case PFLayer::ECAL_ENDCAP:
  case PFLayer::ECAL_BARREL:
    vec = &rechitsECAL_;
    break;
  case PFLayer::HCAL_ENDCAP:
  case PFLayer::HCAL_BARREL1:
  case PFLayer::HCAL_BARREL2:
    vec = &rechitsHCAL_;
    break;
  case PFLayer::PS1:
  case PFLayer::PS2:
    vec = &rechitsPS_;
    break;
  default:
    cerr<<"PFRootEventManager::getMaxE : manage other layers"<<endl;
    return maxe;
  }

  for( unsigned i=0; i<vec->size(); i++) {
    if( (*vec)[i].layer() != layer ) continue;
    if( (*vec)[i].energy() > maxe)
      maxe = (*vec)[i].energy();
  }

  return maxe;
}



double PFRootEventManager::getMaxEEcal() {
  
  if( maxERecHitEcal_<0 ) {
    double maxeec = getMaxE( PFLayer::ECAL_ENDCAP );
    double maxeb =  getMaxE( PFLayer::ECAL_BARREL );
    maxERecHitEcal_ = maxeec > maxeb ? maxeec:maxeb; 
    // max of both barrel and endcap
  }
  return  maxERecHitEcal_;
}




double PFRootEventManager::getMaxEHcal() {

  if(maxERecHitHcal_ < 0) {
    double maxeec = getMaxE( PFLayer::HCAL_ENDCAP );
    double maxeb =  getMaxE( PFLayer::HCAL_BARREL1 );
    maxERecHitHcal_ =  maxeec>maxeb  ?  maxeec:maxeb;
  }
  return maxERecHitHcal_;
}

void PFRootEventManager::getMap(string& map) {

  string dollar = "$";
  string slash  = "/";
  
  // protection necessary or segv !!
  int dollarPos = map.find(dollar,0);
  if( dollarPos == -1 ) return;

  int    lengh  = map.find(slash,0) - map.find(dollar,0) + 1;
  string env_variable =
    map.substr( ( map.find(dollar,0) + 1 ), lengh -2);
  //  cout << "var=" << env_variable << endl;

  const char* name = env_variable.c_str();
  string directory;
  try{
    // cout<<"call getenv"<<endl;
    directory = getenv(name);
    directory += "/";
    // cout<<directory<<endl;
  }
  catch( const string& err ) {
    cout<<err<<endl;
    cout << "ERROR: YOU SHOULD SET THE VARIABLE "
         << env_variable << endl;
  }

  map.replace( 0, lengh , directory);

  if (verbosity_ == VERBOSE ) {
    cout << "Looking for resolution " << map << endl;
    cout << map << endl;
  }
}

void  PFRootEventManager::print() const {

  if( printRecHits_ ) {
    cout<<"ECAL RecHits =============================================="<<endl;
    for(unsigned i=0; i<rechitsECAL_.size(); i++) {
      string seedstatus = "    ";
      if(clusterAlgoECAL_.isSeed(i) ) 
	seedstatus = "SEED";
      printRecHit(rechitsECAL_[i], seedstatus.c_str() );
    }
    cout<<endl;
    cout<<"HCAL RecHits =============================================="<<endl;
    for(unsigned i=0; i<rechitsHCAL_.size(); i++) {
      string seedstatus = "    ";
      if(clusterAlgoHCAL_.isSeed(i) ) 
	seedstatus = "SEED";
      printRecHit(rechitsHCAL_[i]);
    }
    cout<<endl;
    cout<<"PS RecHits ================================================"<<endl;
    for(unsigned i=0; i<rechitsPS_.size(); i++) {
      string seedstatus = "    ";
      if(clusterAlgoPS_.isSeed(i) ) 
	seedstatus = "SEED";
      printRecHit(rechitsPS_[i]);
    }
    cout<<endl;
  }
  if( printClusters_ ) {
    cout<<"ECAL Clusters ============================================="<<endl;
    for(unsigned i=0; i<clustersECAL_->size(); i++) {
      printCluster((*clustersECAL_)[i]);
    }    
    cout<<endl;
    cout<<"HCAL Clusters ============================================="<<endl;
    for(unsigned i=0; i<clustersHCAL_->size(); i++) {
      printCluster((*clustersHCAL_)[i]);
    }    
    cout<<endl;
    cout<<"PS Clusters   ============================================="<<endl;
    for(unsigned i=0; i<clustersPS_->size(); i++) {
      printCluster((*clustersPS_)[i]);
    }    
    cout<<endl;
  }
  if( printPFBlocks_ ) {
    cout<<"Particle Flow Blocks ======================================"<<endl;
    for(unsigned i=0; i<pfBlocks_->size(); i++) {
      cout<<(*pfBlocks_)[i]<<endl;
    }    
    cout<<endl;
  }
  if(printPFCandidates_) {
    cout<<"Particle Flow Candidates =================================="<<endl;
    for(unsigned i=0; i<pfCandidates_->size(); i++) {
      cout<<(*pfCandidates_)[i]<<endl;
    }    
    cout<<endl;
  }
  if( printTrueParticles_ ) {
    cout<<"True Particles  ==========================================="<<endl;
    for(unsigned i=0; i<trueParticles_.size(); i++) {
      if( trackInsideGCut( &(trueParticles_[i]) ) )
	cout<<"\t"<<trueParticles_[i]<<endl;
    }    
  }
  

}

void  PFRootEventManager::printRecHit(const reco::PFRecHit& rh, 
				      const char* seedstatus) const {
  
  double eta = rh.positionREP().Eta();
  double phi = rh.positionREP().Phi();

  if(insideGCut(eta, phi)) 
    cout<<seedstatus<<" "<<rh<<endl;;
}

void  PFRootEventManager::printCluster(const reco::PFCluster& cluster ) const {
  
  double eta = cluster.positionREP().Eta();
  double phi = cluster.positionREP().Phi();

  if(insideGCut(eta, phi)) 
    cout<<cluster<<endl;
}



bool PFRootEventManager::insideGCut( double eta, double phi ) const {

 TCutG* cutg = (TCutG*) gROOT->FindObject("CUTG");
 if(cutg) { // true if the user has drawn a TCutG
   if( !cutg->IsInside(eta, phi) ) return false;
 }
 return true;
} 


bool PFRootEventManager::trackInsideGCut( const reco::PFTrack* track ) const {

  const vector< reco::PFTrajectoryPoint >& points = track->trajectoryPoints();
  
  for( unsigned i=0; i<points.size(); i++) {
    if( ! points[i].isValid() ) continue;
    
    const math::XYZPoint& pos = points[i].positionXYZ();
    if( insideGCut(pos.Eta(), pos.Phi() ) ) return true;
  }

  // no point inside cut
  return false;
}
