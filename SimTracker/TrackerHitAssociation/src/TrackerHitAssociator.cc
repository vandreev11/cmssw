// File: TrackerHitAssociator.cc

#include <memory>
#include <string>
#include <vector>

#include "SimTracker/TrackerHitAssociation/interface/TrackerHitAssociator.h"
//--- for Geometry:
#include "DataFormats/Common/interface/Ref.h"
#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/SiStripDetId/interface/StripSubdetector.h"
#include "DataFormats/SiPixelDetId/interface/PixelSubdetector.h"

#include "SimDataFormats/CrossingFrame/interface/CrossingFrame.h"

//for accumulate
#include <numeric>

using namespace std;
using namespace edm;

//
// Constructor 
//
TrackerHitAssociator::TrackerHitAssociator(const edm::Event& e)  : 
  myEvent_(e), 
  doPixel_( true ),
  doStrip_( true ) {
  
  trackerContainers.clear();
  //
  // Take by default all tracker SimHits
  //
  trackerContainers.push_back("TrackerHitsTIBLowTof");
  trackerContainers.push_back("TrackerHitsTIBHighTof");
  trackerContainers.push_back("TrackerHitsTIDLowTof");
  trackerContainers.push_back("TrackerHitsTIDHighTof");
  trackerContainers.push_back("TrackerHitsTOBLowTof");
  trackerContainers.push_back("TrackerHitsTOBHighTof");
  trackerContainers.push_back("TrackerHitsTECLowTof");
  trackerContainers.push_back("TrackerHitsTECHighTof");
  trackerContainers.push_back("TrackerHitsPixelBarrelLowTof");
  trackerContainers.push_back("TrackerHitsPixelBarrelHighTof");
  trackerContainers.push_back("TrackerHitsPixelEndcapLowTof");
  trackerContainers.push_back("TrackerHitsPixelEndcapHighTof");

  // Step A: Get Inputs
  edm::Handle<CrossingFrame> cf;
  e.getByType(cf);
  
  std::auto_ptr<MixCollection<PSimHit> > allTrackerHits(new MixCollection<PSimHit>(cf.product(),trackerContainers));
  
  //Loop on PSimHit
  SimHitMap.clear();
  
  MixCollection<PSimHit>::iterator isim;
  for (isim=allTrackerHits->begin(); isim!= allTrackerHits->end();isim++) {
    SimHitMap[(*isim).detUnitId()].push_back((*isim));
  }
  
  if(doStrip_) e.getByLabel("siStripDigis", stripdigisimlink);
  if(doPixel_) e.getByLabel("siPixelDigis", pixeldigisimlink);
  
}

//
// Constructor with configurables
//
TrackerHitAssociator::TrackerHitAssociator(const edm::Event& e, const edm::ParameterSet& conf)  : 
  myEvent_(e), 
  doPixel_( conf.getParameter<bool>("associatePixel") ),
  doStrip_( conf.getParameter<bool>("associateStrip") ) {

  
  trackerContainers.clear();
  trackerContainers = conf.getParameter<std::vector<std::string> >("ROUList");

  // Step A: Get Inputs
  edm::Handle<CrossingFrame> cf;
  e.getByType(cf);
  
  std::auto_ptr<MixCollection<PSimHit> > allTrackerHits(new MixCollection<PSimHit>(cf.product(),trackerContainers));
  
  //Loop on PSimHit
  SimHitMap.clear();
  
  MixCollection<PSimHit>::iterator isim;
  for (isim=allTrackerHits->begin(); isim!= allTrackerHits->end();isim++) {
    SimHitMap[(*isim).detUnitId()].push_back((*isim));
  }
  
  if(doStrip_) e.getByLabel("siStripDigis", stripdigisimlink);
  if(doPixel_) e.getByLabel("siPixelDigis", pixeldigisimlink);
  
}

std::vector<PSimHit> TrackerHitAssociator::associateHit(const TrackingRecHit & thit) 
{
  
  //vector with the matched SimHit
  std::vector<PSimHit> result; 
  simtrackid.clear();
  
  //get the Detector type of the rechit
  DetId detid=  thit.geographicalId();
  uint32_t detID = detid.rawId();
  //cout << "Associator ---> get Detid " << detID << endl;
  //check we are in the strip tracker
  if(detid.subdetId() == StripSubdetector::TIB ||
     detid.subdetId() == StripSubdetector::TOB || 
     detid.subdetId() == StripSubdetector::TID ||
     detid.subdetId() == StripSubdetector::TEC) 
    {
      //check if it is a simple SiStripRecHit2D
      if(const SiStripRecHit2D * rechit = 
	 dynamic_cast<const SiStripRecHit2D *>(&thit))
	{	  
	  simtrackid = associateSimpleRecHit(rechit);
	}
      //check if it is a matched SiStripMatchedRecHit2D
      if(const SiStripMatchedRecHit2D * rechit = 
	 dynamic_cast<const SiStripMatchedRecHit2D *>(&thit))
	{	  
	  simtrackid = associateMatchedRecHit(rechit);
	}
    }
  //check we are in the pixel tracker
  if( detid.subdetId() == PixelSubdetector::PixelBarrel || 
      detid.subdetId() == PixelSubdetector::PixelEndcap) 
    {
      if(const SiPixelRecHit * rechit = dynamic_cast<const SiPixelRecHit *>(&thit))
	{	  
	  simtrackid = associatePixelRecHit(rechit);
	}
    }
  
  
  //now get the SimHit from the trackid
  vector<PSimHit> simHit; 
  std::map<unsigned int, std::vector<PSimHit> >::const_iterator it = SimHitMap.find(detID);
  simHit.clear();
  if (it!= SimHitMap.end()){
    simHit = it->second;
    vector<PSimHit>::const_iterator simHitIter = simHit.begin();
    vector<PSimHit>::const_iterator simHitIterEnd = simHit.end();
    for (;simHitIter != simHitIterEnd; ++simHitIter) {
      const PSimHit ihit = *simHitIter;
      unsigned int simHitid = ihit.trackId();
      for(size_t i=0; i<simtrackid.size();i++){
	if(simHitid == simtrackid[i]){ //exclude the geant particles. they all have the same id
	  //	  cout << "Associator ---> ID" << ihit.trackId() << " Simhit x= " << ihit.localPosition().x() 
	  //	       << " y= " <<  ihit.localPosition().y() << " z= " <<  ihit.localPosition().x() << endl; 
	  result.push_back(ihit);
	}
      }
    }
  }else{
    /// Check if it's the gluedDet   
    std::map<unsigned int, std::vector<PSimHit> >::const_iterator itrphi = 
      SimHitMap.find(detID+2);//iterator to the simhit in the rphi module
    std::map<unsigned int, std::vector<PSimHit> >::const_iterator itster = 
      SimHitMap.find(detID+1);//iterator to the simhit in the stereo module
    if (itrphi!= SimHitMap.end()&&itster!=SimHitMap.end()){
      simHit = itrphi->second;
      simHit.insert(simHit.end(),(itster->second).begin(),(itster->second).end());
      vector<PSimHit>::const_iterator simHitIter = simHit.begin();
      vector<PSimHit>::const_iterator simHitIterEnd = simHit.end();
      for (;simHitIter != simHitIterEnd; ++simHitIter) {
	const PSimHit ihit = *simHitIter;
	unsigned int simHitid = ihit.trackId();
	for(size_t i=0; i<simtrackid.size();i++){
	  //  cout << " GluedDet Associator -->  check sihit id's = " << simHitid <<"; compared id's = "<< simtrackid[i] <<endl;
	  if(simHitid == simtrackid[i]){ //exclude the geant particles. they all have the same id
	    //	  cout << "GluedDet Associator ---> ID" << ihit.trackId() << " Simhit x= " << ihit.localPosition().x() 
	    //	       << " y= " <<  ihit.localPosition().y() << " z= " <<  ihit.localPosition().x() << endl; 
	    result.push_back(ihit);
	  }
	}
      }
    }
  }
  return result;  
}


std::vector<unsigned int> TrackerHitAssociator::associateHitId(const TrackingRecHit & thit) 
{
  
  //vector with the matched SimTrackID 
  simtrackid.clear();
  
  //get the Detector type of the rechit
  DetId detid=  thit.geographicalId();
  uint32_t detID = detid.rawId();
  //cout << "Associator ---> get Detid " << detID << endl;
  //check we are in the strip tracker
  if(detid.subdetId() == StripSubdetector::TIB ||
     detid.subdetId() == StripSubdetector::TOB || 
     detid.subdetId() == StripSubdetector::TID ||
     detid.subdetId() == StripSubdetector::TEC) 
    {
      //check if it is a simple SiStripRecHit2D
      if(const SiStripRecHit2D * rechit = 
	 dynamic_cast<const SiStripRecHit2D *>(&thit))
	{	  
	  simtrackid = associateSimpleRecHit(rechit);
	}
      //check if it is a matched SiStripMatchedRecHit2D
      if(const SiStripMatchedRecHit2D * rechit = 
	 dynamic_cast<const SiStripMatchedRecHit2D *>(&thit))
	{	  
	  simtrackid = associateMatchedRecHit(rechit);
	}
    }
  //check we are in the pixel tracker
  if( detid.subdetId() == PixelSubdetector::PixelBarrel || 
      detid.subdetId() == PixelSubdetector::PixelEndcap) 
    {
      if(const SiPixelRecHit * rechit = dynamic_cast<const SiPixelRecHit *>(&thit))
	{	  
	  simtrackid = associatePixelRecHit(rechit);
	}
    }
  return simtrackid;  
}


std::vector<unsigned int>  TrackerHitAssociator::associateSimpleRecHit(const SiStripRecHit2D * simplerechit)
{
  DetId detid=  simplerechit->geographicalId();
  uint32_t detID = detid.rawId();

  //to store temporary charge information
  std::vector<unsigned int> cache_simtrackid; 
  cache_simtrackid.clear();
  std::map<unsigned int, vector<float> > temp_simtrackid;
  float chg;
  temp_simtrackid.clear();

  edm::DetSetVector<StripDigiSimLink>::const_iterator isearch = stripdigisimlink->find(detID); 
  if(isearch != stripdigisimlink->end()) {  //if it is not empty
    //link_detset is a structure, link_detset.data is a std::vector<StripDigiSimLink>
    edm::DetSet<StripDigiSimLink> link_detset = (*stripdigisimlink)[detID];
    
    const edm::Ref<edm::DetSetVector<SiStripCluster>, SiStripCluster, edm::refhelper::FindForDetSetVector<SiStripCluster> > clust=simplerechit->cluster();

    int clusiz = clust->amplitudes().size();
    int first  = clust->firstStrip();     
    int last   = first + clusiz;
    float cluchg = std::accumulate(clust->amplitudes().begin(), clust->amplitudes().end(),0);
    // cout << "Associator ---> Clus size = " << clusiz << " first = " << first << "  last = " << last << "  tot charge = " << cluchg << endl;

    //use a vector
    std::vector<unsigned int> idcachev;
    for(edm::DetSet<StripDigiSimLink>::const_iterator linkiter = link_detset.data.begin(); linkiter != link_detset.data.end(); linkiter++){
      StripDigiSimLink link = *linkiter;
      if( link.channel() >= first  && link.channel() < last ){
	//write only once the id
	//	if(link.SimTrackId() != simtrackid_cache){
	if(find(idcachev.begin(),idcachev.end(),link.SimTrackId()) == idcachev.end()){
	  //std::cout << " Adding track id  = " << link.SimTrackId() << std::endl;
	  cache_simtrackid.push_back(link.SimTrackId());
	  //simtrackid_cache = link.SimTrackId();
	  idcachev.push_back(link.SimTrackId());
	}
	//get the charge released in the cluster by the simtrack strip by strip
	chg = 0;
	int mychan = link.channel()-first;
	chg = (clust->amplitudes()[mychan])*link.fraction();
	temp_simtrackid[link.SimTrackId()].push_back(chg);
	//std::cout << " Track ID = " <<  link.SimTrackId() << " charge by ch = " << chg << std::endl;
      }
    }
    
    vector<float> tmpchg;
    float simchg;
    float simfraction;
    std::map<float, unsigned int, greater<float> > temp_map;
    simchg=0;
    //loop over the unique ID's
    vector<unsigned int>::iterator new_end = unique(cache_simtrackid.begin(),cache_simtrackid.end());
    for(vector<unsigned int>::iterator i=cache_simtrackid.begin(); i != new_end; i++){
      std::map<unsigned int, vector<float> >::const_iterator it = temp_simtrackid.find(*i);
      if(it != temp_simtrackid.end()){
	tmpchg = it->second;
	for(size_t ii=0; ii<tmpchg.size(); ii++){
	  simchg +=tmpchg[ii];
	}
	simfraction = simchg/cluchg;
	//cut at >50%
	if(simfraction >0.5){
	  temp_map.insert(std::pair<float, unsigned int> (simfraction,*i));
	}
      }
    }	
    //sort the map ordered on the charge fraction 
    
    //copy the list of ID's ordered in the charge fraction 
    for(std::map<float , unsigned int>::const_iterator it = temp_map.begin(); it!=temp_map.end(); it++){
      //std::cout << "Track id = " << it->second << " fraction = " << it->first << std::endl;
      simtrackid.push_back(it->second);
    }
  }    

  return simtrackid;
  
}


std::vector<unsigned int>  TrackerHitAssociator::associateMatchedRecHit(const SiStripMatchedRecHit2D * matchedrechit)
{
  //to be written
  vector<unsigned int> matched_mono;
  vector<unsigned int> matched_st;
  matched_mono.clear();
  matched_st.clear();
  
  const SiStripRecHit2D *mono = matchedrechit->monoHit();
  const SiStripRecHit2D *st = matchedrechit->stereoHit();
  //associate the two simple hits separately
  matched_mono = associateSimpleRecHit(mono);
  matched_st   = associateSimpleRecHit(st);
  
  //save in a vector all the simtrack-id's that are common to mono and stereo hits
  if(!matched_mono.empty() && !matched_st.empty()){
    simtrackid.clear(); //final result vector
    //    unsigned int simtrackid_cache = 9999999;
    std::vector<unsigned int> idcachev;
    for(vector<unsigned int>::iterator mhit=matched_mono.begin(); mhit != matched_mono.end(); mhit++){
      //save only once the ID
      if(find(idcachev.begin(), idcachev.end(),(*mhit)) == idcachev.end()) {
	idcachev.push_back(*mhit);
	//save if the stereoID matched the monoID
	if(find(matched_st.begin(), matched_st.end(),(*mhit))!=matched_st.end()){
	  simtrackid.push_back(*mhit);
	  //std::cout << "matched case: saved ID " << (*mhit) << std::endl; 
	}
      }
    }
  }
  return simtrackid;
}

std::vector<unsigned int>  TrackerHitAssociator::associatePixelRecHit(const SiPixelRecHit * pixelrechit)
{
  //
  // Pixel associator
  //
  DetId detid=  pixelrechit->geographicalId();
  uint32_t detID = detid.rawId();
  edm::DetSetVector<PixelDigiSimLink>::const_iterator isearch = pixeldigisimlink->find(detID); 
  if(isearch != pixeldigisimlink->end()) {  //if it is not empty
    edm::DetSet<PixelDigiSimLink> link_detset = (*pixeldigisimlink)[detID];
    edm::Ref< edm::DetSetVector<SiPixelCluster>, SiPixelCluster> const& cluster = pixelrechit->cluster();
    int minPixelRow = (*cluster).minPixelRow();
    int maxPixelRow = (*cluster).maxPixelRow();
    int minPixelCol = (*cluster).minPixelCol();
    int maxPixelCol = (*cluster).maxPixelCol();    
    //std::cout << "    Cluster minRow " << minPixelRow << " maxRow " << maxPixelRow << std::endl;
    //std::cout << "    Cluster minCol " << minPixelCol << " maxCol " << maxPixelCol << std::endl;
    edm::DetSet<PixelDigiSimLink>::const_iterator linkiter = link_detset.data.begin();
    int dsl = 0;
    //    unsigned int simtrackid_cache = 9999999;
    std::vector<unsigned int> idcachev;
    for( ; linkiter != link_detset.data.end(); linkiter++) {
      dsl++;
      std::pair<int,int> pixel_coord = PixelDigi::channelToPixel(linkiter->channel());
      //std::cout << "    " << dsl << ") Digi link: row " << pixel_coord.first << " col " << pixel_coord.second << std::endl;      
      if(  pixel_coord.first  <= maxPixelRow && 
	   pixel_coord.first  >= minPixelRow &&
	   pixel_coord.second <= maxPixelCol &&
	   pixel_coord.second >= minPixelCol ) {
	//std::cout << "      !-> trackid   " << linkiter->SimTrackId() << endl;
	//std::cout << "          fraction  " << linkiter->fraction()   << endl;
	if(find(idcachev.begin(),idcachev.end(),linkiter->SimTrackId()) == idcachev.end()){
	  //	if(linkiter->SimTrackId() != simtrackid_cache) {  // Add each trackid only once
	  simtrackid.push_back(linkiter->SimTrackId());
	  //cout    << "          Adding TrackId " << linkiter->SimTrackId() << endl;
	  idcachev.push_back(linkiter->SimTrackId());
	}
      } 
    }
  }
  
  return simtrackid;
}


