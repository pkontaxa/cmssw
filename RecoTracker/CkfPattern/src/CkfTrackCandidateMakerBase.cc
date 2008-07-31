#include <memory>
#include <string>

#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DataFormats/Common/interface/OwnVector.h"
#include "DataFormats/TrackCandidate/interface/TrackCandidateCollection.h"
#include "DataFormats/Common/interface/View.h"

#include "Geometry/TrackerGeometryBuilder/interface/TrackerGeometry.h"

#include "TrackingTools/PatternTools/interface/Trajectory.h"
#include "TrackingTools/TrajectoryCleaning/interface/TrajectoryCleanerBySharedHits.h"
#include "TrackingTools/Records/interface/TrackingComponentsRecord.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateTransform.h"

#include "RecoTracker/CkfPattern/interface/CkfTrackCandidateMakerBase.h"
#include "RecoTracker/CkfPattern/interface/TransientInitialStateEstimator.h"
#include "RecoTracker/Record/interface/TrackerRecoGeometryRecord.h"
#include "RecoTracker/Record/interface/CkfComponentsRecord.h"


#include "RecoTracker/CkfPattern/interface/SeedCleanerByHitPosition.h"
#include "RecoTracker/CkfPattern/interface/CachingSeedCleanerByHitPosition.h"
#include "RecoTracker/CkfPattern/interface/SeedCleanerBySharedInput.h"
#include "RecoTracker/CkfPattern/interface/CachingSeedCleanerBySharedInput.h"

#include "RecoTracker/Record/interface/NavigationSchoolRecord.h"
#include "TrackingTools/DetLayers/interface/NavigationSchool.h"

#include<algorithm>
#include<functional>

using namespace edm;
using namespace std;

namespace cms{
  CkfTrackCandidateMakerBase::CkfTrackCandidateMakerBase(edm::ParameterSet const& conf) : 

    conf_(conf),
    theTrackCandidateOutput(true),
    theTrajectoryOutput(false),
    useSplitting(conf.getParameter<bool>("useHitsSplitting")),
    doSeedingRegionRebuilding(conf.getParameter<bool>("doSeedingRegionRebuilding")),
    theTrajectoryBuilderName(conf.getParameter<std::string>("TrajectoryBuilder")), 
    theTrajectoryBuilder(0),
    theTrajectoryCleanerName(conf.getParameter<std::string>("TrajectoryCleaner")), 
    theTrajectoryCleaner(0),
    theInitialState(0),
    theNavigationSchoolName(conf.getParameter<std::string>("NavigationSchool")),
    theNavigationSchool(0),
    theSeedCleaner(0)
  {  
    //produces<TrackCandidateCollection>();  
  }

  
  // Virtual destructor needed.
  CkfTrackCandidateMakerBase::~CkfTrackCandidateMakerBase() {
    delete theInitialState;  
    if (theSeedCleaner) delete theSeedCleaner;
  }  

  void CkfTrackCandidateMakerBase::beginJobBase (EventSetup const & es)
  {
    // get nested parameter set for the TransientInitialStateEstimator
    ParameterSet tise_params = conf_.getParameter<ParameterSet>("TransientInitialStateEstimatorParameters") ;
    theInitialState          = new TransientInitialStateEstimator( es,tise_params);
   
    std::string cleaner = conf_.getParameter<std::string>("RedundantSeedCleaner");
    if (cleaner == "SeedCleanerByHitPosition") {
        theSeedCleaner = new SeedCleanerByHitPosition();
    } else if (cleaner == "SeedCleanerBySharedInput") {
        theSeedCleaner = new SeedCleanerBySharedInput();
    } else if (cleaner == "CachingSeedCleanerByHitPosition") {
        theSeedCleaner = new CachingSeedCleanerByHitPosition();
    } else if (cleaner == "CachingSeedCleanerBySharedInput") {
        theSeedCleaner = new CachingSeedCleanerBySharedInput();
    } else if (cleaner == "none") {
        theSeedCleaner = 0;
    } else {
        throw cms::Exception("RedundantSeedCleaner not found", cleaner);
    }
  }

  void CkfTrackCandidateMakerBase::setEventSetup( const edm::EventSetup& es ) {
    //services
    es.get<TrackerRecoGeometryRecord>().get( theGeomSearchTracker );
    es.get<IdealMagneticFieldRecord>().get( theMagField );

    theInitialState->setEventSetup( es );

    edm::ESHandle<TrajectoryCleaner> trajectoryCleanerH;
    es.get<TrajectoryCleaner::Record>().get(theTrajectoryCleanerName, trajectoryCleanerH);
    theTrajectoryCleaner= trajectoryCleanerH.product();

    edm::ESHandle<NavigationSchool> navigationSchoolH;
    es.get<NavigationSchoolRecord>().get(theNavigationSchoolName, navigationSchoolH);
    theNavigationSchool = navigationSchoolH.product();

    // set the TrajectoryBuilder
    edm::ESHandle<TrajectoryBuilder> theTrajectoryBuilderHandle;
    es.get<CkfComponentsRecord>().get(theTrajectoryBuilderName,theTrajectoryBuilderHandle);
    theTrajectoryBuilder = theTrajectoryBuilderHandle.product();    
       
  }

  // Functions that gets called by framework every event
  void CkfTrackCandidateMakerBase::produceBase(edm::Event& e, const edm::EventSetup& es)
  { 
    // getting objects from the EventSetup
    setEventSetup( es ); 

    // set the correct navigation
    NavigationSetter setter( *theNavigationSchool);
    
    // propagator
    edm::ESHandle<Propagator> thePropagator;
    es.get<TrackingComponentsRecord>().get("AnyDirectionAnalyticalPropagator",
					   thePropagator);

    // method for Debugging
    printHitsDebugger(e);

    // Step A: set Event for the TrajectoryBuilder
    theTrajectoryBuilder->setEvent(e);        
    
    // Step B: Retrieve seeds
    
    std::string seedProducer = conf_.getParameter<std::string>("SeedProducer");
    std::string seedLabel = conf_.getParameter<std::string>("SeedLabel");

    edm::Handle<View<TrajectorySeed> > collseed;
    e.getByLabel(seedProducer, seedLabel, collseed);
    
    // Step C: Create empty output collection
    std::auto_ptr<TrackCandidateCollection> output(new TrackCandidateCollection);    
    std::auto_ptr<std::vector<Trajectory> > outputT (new std::vector<Trajectory>());
    
    // Step D: Invoke the building algorithm
    if ((*collseed).size()>0){

       vector<Trajectory> rawResult;
       rawResult.reserve(collseed->size() * 4);

      if (theSeedCleaner) theSeedCleaner->init( &rawResult );
      
      // method for debugging
      countSeedsDebugger();

      vector<Trajectory> theTmpTrajectories;

      size_t collseed_size = collseed->size(); 
      for (size_t j = 0; j < collseed_size; j++){
       
	if (theSeedCleaner && !theSeedCleaner->good( &((*collseed)[j])) ) continue;

        theTmpTrajectories.clear();
	theTrajectoryBuilder->trajectories( (*collseed)[j], theTmpTrajectories );
	
       
	LogDebug("CkfPattern") << "======== CkfTrajectoryBuilder returned " << theTmpTrajectories.size()
			       << " trajectories for this seed ========";

	theTrajectoryCleaner->clean(theTmpTrajectories);

	if(doSeedingRegionRebuilding)
	  theTrajectoryBuilder->rebuildSeedingRegion((*collseed)[j],theTmpTrajectories);      

	for(vector<Trajectory>::iterator it=theTmpTrajectories.begin();
	    it!=theTmpTrajectories.end(); it++){
	  if( it->isValid() ) {
	    it->setSeedRef(collseed->refAt(j));
	    rawResult.push_back(*it);
            //TO BE FIXED: this cut should be configurable via cfi file
            if (theSeedCleaner && it->foundHits()>3) theSeedCleaner->add( & (*it) );
            //if (theSeedCleaner ) theSeedCleaner->add( & (*it) );
	  }
	}

        theTmpTrajectories.clear();
        
	LogDebug("CkfPattern") << "rawResult size after cleaning " << rawResult.size();
      }
      
      if (theSeedCleaner) theSeedCleaner->done();
      
      // Step E: Clean the result
      theTrajectoryCleaner->clean(rawResult);

      vector<Trajectory> & unsmoothedResult(rawResult);
      unsmoothedResult.erase(std::remove_if(unsmoothedResult.begin(),unsmoothedResult.end(),
					    std::not1(std::mem_fun_ref(&Trajectory::isValid))),
			     unsmoothedResult.end());
      

      //      for (vector<Trajectory>::const_iterator itraw = rawResult.begin();
      //	   itraw != rawResult.end(); itraw++) {
      //if((*itraw).isValid()) unsmoothedResult.push_back( *itraw);
      //}

      //analyseCleanedTrajectories(unsmoothedResult);
      
      if (theTrackCandidateOutput){
	// Step F: Convert to TrackCandidates
       output->reserve(unsmoothedResult.size());
       for (vector<Trajectory>::const_iterator it = unsmoothedResult.begin();
	    it != unsmoothedResult.end(); it++) {
	
	 Trajectory::RecHitContainer thits;
	 //it->recHitsV(thits);
	 it->recHitsV(thits,useSplitting);
	 OwnVector<TrackingRecHit> recHits;
	 recHits.reserve(thits.size());
	 for (Trajectory::RecHitContainer::const_iterator hitIt = thits.begin();
	      hitIt != thits.end(); hitIt++) {
	   recHits.push_back( (**hitIt).hit()->clone());
	 }
	
	 //PTrajectoryStateOnDet state = *(it->seed().startingState().clone());
	 std::pair<TrajectoryStateOnSurface, const GeomDet*> initState = 
	   theInitialState->innerState( *it);

	 // temporary protection againt invalid initial states
	 if (! initState.first.isValid() || initState.second == 0) {
	   //cout << "invalid innerState, will not make TrackCandidate" << endl;
	   continue;
	 }
	 
	 PTrajectoryStateOnDet* state =0 ;
	 if(useSplitting && (initState.second != thits.front()->det()) ){	 
	   state = TrajectoryStateTransform().persistentState(
							      thePropagator->propagate(initState.first,
										       thits.front()->det()->surface()),
							      thits.front()->det()->geographicalId().rawId());
	 }
	 
	 if(!state) state = TrajectoryStateTransform().persistentState( initState.first,
									initState.second->geographicalId().rawId());
	 
	 
	 output->push_back(TrackCandidate(recHits,it->seed(),*state,it->seedRef() ) );
	 
	 delete state;
       }
      }//output trackcandidates
      
      
      
      LogTrace("TrackingRegressionTest") << "========== CkfTrackCandidateMaker Info ==========";
      edm::ESHandle<TrackerGeometry> tracker;
      es.get<TrackerDigiGeometryRecord>().get(tracker);
      LogTrace("TrackingRegressionTest") << "number of Seed: " << collseed->size();
      
      /*
      for(iseed=theSeedColl.begin();iseed!=theSeedColl.end();iseed++){
	DetId tmpId = DetId( iseed->startingState().detId());
	const GeomDet* tmpDet  = tracker->idToDet( tmpId );
	GlobalVector gv = tmpDet->surface().toGlobal( iseed->startingState().parameters().momentum() );
	
	LogTrace("TrackingRegressionTest") << "seed perp,phi,eta : " 
	                                   << gv.perp() << " , " 
				           << gv.phi() << " , " 
				           << gv.eta() ;
      }
      */
      
      LogTrace("TrackingRegressionTest") << "number of finalTrajectories: " << unsmoothedResult.size();
      for (vector<Trajectory>::const_iterator it = unsmoothedResult.begin();
	   it != unsmoothedResult.end(); it++) {
	if (it->lastMeasurement().updatedState().isValid()) {
	  LogTrace("TrackingRegressionTest") << "candidate's n valid and invalid hit, chi2, pt : " 
					     << it->foundHits() << " , " 
					     << it->lostHits() <<" , " 
					     << it->chiSquared() << " , "
					     << it->lastMeasurement().updatedState().globalMomentum().perp();
	} else if (it->lastMeasurement().predictedState().isValid()) {
	  LogTrace("TrackingRegressionTest") << "candidate's n valid and invalid hit, chi2, pt : " 
					     << it->foundHits() << " , " 
					     << it->lostHits() <<" , " 
					     << it->chiSquared() << " , "
					     << it->lastMeasurement().predictedState().globalMomentum().perp();
	} else LogTrace("TrackingRegressionTest") << "candidate with invalid last measurement state!";
      }
      LogTrace("TrackingRegressionTest") << "=================================================";
     
      if (theTrajectoryOutput){ outputT->swap(unsmoothedResult);}

    }// end of ((*collseed).size()>0)
    
    // method for debugging
    deleteAssocDebugger();

    // Step G: write output to file
    if (theTrackCandidateOutput){ e.put(output);}
    if (theTrajectoryOutput){e.put(outputT);}

  }
}

