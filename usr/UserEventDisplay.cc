/**
 *  file: UserEventDisplay.cc
 *  date: 2017.04.10
 *
 */

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "ConfMan.hh"
#include "DCRawHit.hh"
#include "DatabasePDG.hh"
#include "DetectorID.hh"
#include "EventDisplay.hh"
#include "RMAnalyzer.hh"
#include "FiberCluster.hh"
#include "FiberHit.hh"
#include "FLHit.hh"
#include "KuramaLib.hh"
#include "RawData.hh"
//#include "RootHelper.hh"
#include "UnpackerManager.hh"
#include "VEvent.hh"

namespace
{
  const std::string& class_name("EventDisplay");
  const DCGeomMan&     gGeom   = DCGeomMan::GetInstance();
  EventDisplay&        gEvDisp = EventDisplay::GetInstance();
  RMAnalyzer&          gRM     = RMAnalyzer::GetInstance();
  const UserParamMan&  gUser   = UserParamMan::GetInstance();
  const hddaq::unpacker::UnpackerManager& gUnpacker
  = hddaq::unpacker::GUnpacker::get_instance();
  const double KaonMass   = pdg::KaonMass();
  const double ProtonMass = pdg::ProtonMass();
}

//______________________________________________________________________________
VEvent::VEvent( void )
{
}

//______________________________________________________________________________
VEvent::~VEvent( void )
{
}

//______________________________________________________________________________
class UserEventDisplay : public VEvent
{
private:
  RawData      *rawData;
  DCAnalyzer   *DCAna;
  HodoAnalyzer *hodoAna;
public:
        UserEventDisplay( void );
       ~UserEventDisplay( void );
  bool  ProcessingBegin( void );
  bool  ProcessingEnd( void );
  bool  ProcessingNormal( void );
  bool  InitializeHistograms( void );
};

//______________________________________________________________________________
UserEventDisplay::UserEventDisplay( void )
  : VEvent(),
    rawData(0),
    DCAna( new DCAnalyzer ),
    hodoAna( new HodoAnalyzer )
{
}

//______________________________________________________________________________
UserEventDisplay::~UserEventDisplay( void )
{
  if (DCAna)   delete DCAna;
  if (hodoAna) delete hodoAna;
  if (rawData) delete rawData;
}

//______________________________________________________________________________
bool
UserEventDisplay::ProcessingBegin( void )
{
  return true;
}

//______________________________________________________________________________
bool
UserEventDisplay::ProcessingNormal( void )
{
  static const std::string func_name("["+class_name+"::"+__func__+"]");

  static const double MaxMultiHitBcOut  = gUser.GetParameter("MaxMultiHitBcOut");
  static const double MaxMultiHitSdcIn  = gUser.GetParameter("MaxMultiHitSdcIn");
  static const double MaxMultiHitSdcOut = gUser.GetParameter("MaxMultiHitSdcOut");

  static const double MinTimeBFT = gUser.GetParameter("TimeBFT", 0);
  static const double MaxTimeBFT = gUser.GetParameter("TimeBFT", 1);
  static const double MinTdcSCH  = gUser.GetParameter("TdcSCH", 0);
  static const double MaxTdcSCH  = gUser.GetParameter("TdcSCH", 1);
  static const double MinTdcSFT  = gUser.GetParameter("TdcSFT", 0);
  static const double MaxTdcSFT  = gUser.GetParameter("TdcSFT", 1);
  static const double MinTdcFBT1  = gUser.GetParameter("TdcFBT1", 0);
  static const double MaxTdcFBT1  = gUser.GetParameter("TdcFBT1", 1);
  static const double MinTdcFBT2  = gUser.GetParameter("TdcFBT2", 0);
  static const double MaxTdcFBT2  = gUser.GetParameter("TdcFBT2", 1);

  static const double OffsetToF  = gUser.GetParameter("OffsetToF");
  static const double dTOfs      = gUser.GetParameter("dTOfs",   0);
  static const double MinTimeL1  = gUser.GetParameter("TimeL1",  0);
  static const double MaxTimeL1  = gUser.GetParameter("TimeL1",  1);
  static const double MinTotSDC2 = gUser.GetParameter("MinTotSDC2", 0);
  static const double MinTotSDC3 = gUser.GetParameter("MinTotSDC3", 0);

  // static const int IdBH2 = gGeom.GetDetectorId("BH2");
  static const int IdSFT_U = gGeom.GetDetectorId("SFT-U");
  static const int IdSFT_V = gGeom.GetDetectorId("SFT-V");
  static const int IdSFT_X = gGeom.GetDetectorId("SFT-X");
  static const int IdSCH = gGeom.GetDetectorId("SCH");
  static const int IdTOF = gGeom.GetDetectorId("TOF");
  static const int IdFBT1_D1 = gGeom.DetectorId("FBT1-DX1");
  static const int IdFBT1_U1 = gGeom.DetectorId("FBT1-UX1");
  static const int IdFBT1_D2 = gGeom.DetectorId("FBT1-DX2");
  static const int IdFBT1_U2 = gGeom.DetectorId("FBT1-UX2");
  static const int IdFBT2_D1 = gGeom.DetectorId("FBT2-DX1");
  static const int IdFBT2_U1 = gGeom.DetectorId("FBT2-UX1");
  static const int IdFBT2_D2 = gGeom.DetectorId("FBT2-DX2");
  static const int IdFBT2_U2 = gGeom.DetectorId("FBT2-UX2");


  rawData = new RawData;
  rawData->DecodeHits();

  gRM.Decode();

  gEvDisp.DrawText( 0.1, 0.3, Form("Run# %5d%4sEvent# %6d",
				    gRM.RunNumber(), "",
				    gRM.EventNumber() ) );

  // Trigger Flag
  {
    const HodoRHitContainer &cont=rawData->GetTrigRawHC();
    int trigflag[NumOfSegTrig] = {};
    int nh = cont.size();
    for( int i=0; i<nh; ++i ){
      HodoRawHit *hit = cont[i];
      int seg = hit->SegmentId()+1;
      int tdc = hit->GetTdc1();
      trigflag[seg-1] = tdc;
    }
    if( trigflag[SpillEndFlag]>0 ) return true;
  }

  // Trigger flag
  bool flag_tof_stop = false;
  {
    static const int device_id    = gUnpacker.get_device_id("TFlag");
    static const int data_type_id = gUnpacker.get_data_id("TFlag", "tdc");

    int mhit = gUnpacker.get_entries(device_id, 0, kTofTiming, 0, data_type_id);
    for(int m = 0; m<mhit; ++m){
      int tof_timing = gUnpacker.get(device_id, 0, kTofTiming, 0, data_type_id, m);
      if(!(MinTimeL1 < tof_timing && tof_timing < MaxTimeL1)) flag_tof_stop = true;
    }// for(m)
  }


  // BH2
  // {
  //   const HodoRHitContainer &cont = rawData->GetBH2RawHC();
  //   int nh=cont.size();
  //   for( int i=0; i<nh; ++i ){
  //     HodoRawHit *hit = cont[i];
  //     if( !hit ) continue;
  //     int seg=hit->SegmentId();
  //     int Tu=hit->GetTdcUp(), Td=hit->GetTdcDown();
  //     // if( Tu>0 || Td>0 )
  //     // 	gEvDisp.DrawHitHodoscope( DCGeomIdBH2, seg, Tu, Td );
  //   }
  // }
  hodoAna->DecodeBH2Hits(rawData);
  int ncBh2 = hodoAna->GetNClustersBH2();
  if( ncBh2==0 ) {
    gEvDisp.GetCommand();
    return true;
  }
  BH2Cluster *clBH2 = hodoAna->GetClusterBH2(0);
  double time0 = clBH2->CTime0();

  // TOF
  {
    const HodoRHitContainer &cont = rawData->GetTOFRawHC();
    int nh = cont.size();
    for( int i=0; i<nh; ++i ){
      HodoRawHit *hit = cont[i];
      if( !hit ) continue;
      int seg = hit->SegmentId();
      int Tu = hit->GetTdcUp(), Td = hit->GetTdcDown();
      if( Tu>0 || Td>0 )
	gEvDisp.DrawHitHodoscope( IdTOF, seg, Tu, Td );
    }
  }
  hodoAna->DecodeTOFHits(rawData);
  Hodo2HitContainer TOFCont;
  int nhTof = hodoAna->GetNHitsTOF();
  for( int i=0; i<nhTof; ++i ){
    Hodo2Hit *hit = hodoAna->GetHitTOF(i);
    if( !hit ) continue;
    TOFCont.push_back( hit );
  }
  if( nhTof==0 ) {
    gEvDisp.GetCommand();
    return true;
  }

  // SCH
  {
    hodoAna->DecodeSCHHits(rawData);
    int nhSch = hodoAna->GetNHitsSCH();
    for( int i=0; i<nhSch; ++i ){
      FiberHit *hit = hodoAna->GetHitSCH(i);
      if( !hit ) continue;
      int mh  = hit->GetNumOfHit();
      int seg = hit->SegmentId();
      bool hit_flag = false;
      bool hit_flag2 = false; // later coincidensed event
      for( int m=0; m<mh; ++m ){
  	double leading = hit->GetLeading(m);
  	if( MinTdcSCH<leading && leading<MaxTdcSCH ){
  	  hit_flag = true;
  	} else if  (leading > 400 && leading<480) {
  	  hit_flag2 = true;
	}
      }
      if( hit_flag ){
  	gEvDisp.DrawHitHodoscope( IdSCH, seg );
      } else if (hit_flag2){
  	gEvDisp.DrawHitHodoscope( IdSCH, seg , 1, -1);
      }
    }
  }

  hodoAna->DecodeSFTHits(rawData);
  //hodoAna->WidthCutSFT( 0, 40. , 100.);
  //hodoAna->WidthCutSFT( 1, 40. , 100.);
  //hodoAna->WidthCutSFT( 2, 40. , 100.);
  ////////// SFT-U
  {
    // Fiber Hit
    int nh = hodoAna->GetNHitsSFT(SFT_U);
    for( int i=0; i<nh; ++i ){
      const FiberHit* hit = hodoAna->GetHitSFT(SFT_U, i);
      if(!hit) continue;
      int mh = hit->GetNLeading();
      int seg    = hit->SegmentId();

      bool hit_flag = false;
      bool hit_flag2 = false;
      for( int m=0; m<mh; ++m ){
  	double leading = hit->GetLeading(m);
  	if( 530<leading && leading<MaxTdcSFT ){
  	  hit_flag = true;
	  std::cout << "SFT_U : " << seg << ", " << leading<< std::endl;
  	} else if( MinTdcSFT <leading && leading<MaxTdcSFT ){
  	  hit_flag2 = true;
	  std::cout << "SFT_U : " << seg << ", " << leading<< std::endl;
  	}

      }
      if( hit_flag){
  	gEvDisp.DrawHitHodoscope( IdSFT_U, seg );
      } else if (hit_flag2){
  	gEvDisp.DrawHitHodoscope( IdSFT_U, seg , 1, -1);
      }

    }
  }

  ////////// SFT-V
  {
    // Fiber Hit
    int nh = hodoAna->GetNHitsSFT(SFT_V);
    for( int i=0; i<nh; ++i ){
      const FiberHit* hit = hodoAna->GetHitSFT(SFT_V, i);
      if(!hit) continue;
      int mh = hit->GetNLeading();
      int seg    = hit->SegmentId();

      bool hit_flag = false;
      bool hit_flag2 = false;

      for( int m=0; m<mh; ++m ){
  	double leading = hit->GetLeading(m);
  	if( 530<leading && leading<MaxTdcSFT ){
  	  hit_flag = true;
	  std::cout << "SFT_V : " << seg << ", " << leading<< std::endl;
  	} else if( MinTdcSFT <leading && leading<MaxTdcSFT ){
  	  hit_flag2 = true;
	  std::cout << "SFT_V : " << seg << ", " << leading<< std::endl;
  	}
      }
      if( hit_flag ){
  	gEvDisp.DrawHitHodoscope( IdSFT_V, seg );
      } else if ( hit_flag2 ){
  	gEvDisp.DrawHitHodoscope( IdSFT_V, seg , 1, -1);
      }
    }
  }

  ////////// SFT-X
  {
    // Fiber Hit
    for(int p = SFT_X1; p<NumOfPlaneSFT; ++p){
      int nh = hodoAna->GetNHitsSFT(p);
      enum { U, D };
      for( int i=0; i<nh; ++i ){
	const FiberHit* hit = hodoAna->GetHitSFT(p, i);
	if(!hit) continue;
	int mh = hit->GetNLeading();
	int seg    = hit->SegmentId();

	bool hit_flag = false;
	bool hit_flag2 = false;
	// raw leading data
	for( int m=0; m<mh; ++m ){
	  double leading  = hit->GetLeading(m);

	  if( 530<leading && leading<MaxTdcSFT ){
	    hit_flag = true;
	    std::cout << "SFT_X : " << seg << ", " << leading<< std::endl;
	  } else if( MinTdcSFT <leading && leading<MaxTdcSFT ){
	    hit_flag2 = true;
	    std::cout << "SFT_X : " << seg << ", " << leading<< std::endl;
	  }
	}// for(m)

	if( hit_flag ){
	  gEvDisp.DrawHitHodoscope( IdSFT_X, seg );
	} else if ( hit_flag2 ){
	  gEvDisp.DrawHitHodoscope( IdSFT_X, seg , 1, -1);
	}

      }
    }
  }
  
  bool flagFBT=false;
  {
    ////////// FBT1
    hodoAna->DecodeFBT1Hits( rawData );
    // Fiber Hit
    enum { U, D };
    for(int layer = 0; layer<NumOfLayersFBT1; ++layer){
      for(int UorD = 0; UorD<2; ++UorD){
	int nh = hodoAna->GetNHitsFBT1(layer, UorD);
	for( int i=0; i<nh; ++i ){
	  const FiberHit* hit = hodoAna->GetHitFBT1(layer, UorD, i);
	  if(!hit) continue;
	  int mh  = hit->GetNLeading();
	  int seg   = hit->SegmentId();

	  bool hit_flag = false;
	  bool hit_flag2 = false;
	  // raw leading data
	  for( int m=0; m<mh; ++m ){
	    double leading  = hit->GetLeading(m);
	    if( 505<leading && leading<MaxTdcFBT1 ){
	      hit_flag = true;
	      flagFBT = true;
	      std::cout << "FBT1 : " << seg << ", " << leading<< std::endl;
	    } else if( MinTdcFBT1 <leading && leading<MaxTdcFBT1 ){
	      flagFBT = true;
	      hit_flag2 = true;
	      std::cout << "FBT1 : " << seg << ", " << leading<< std::endl;
	    } 
	  }// for(m)
	  
	  int IdFBT = 0;
	  if (layer == 0 && UorD ==D)
	    IdFBT = IdFBT1_D1;
	  else if (layer == 0 && UorD ==U)
	    IdFBT = IdFBT1_U1;
	  else if (layer == 1 && UorD ==D)
	    IdFBT = IdFBT1_D2;
	  else if (layer == 1 && UorD ==U)
	    IdFBT = IdFBT1_U2;

	  if( hit_flag ){
	    gEvDisp.DrawHitHodoscope( IdFBT, seg );
	  } else if ( hit_flag2 ){
	    gEvDisp.DrawHitHodoscope( IdFBT, seg , 1, -1);
	  }

	}
      }
    }

    ////////// FBT2
    hodoAna->DecodeFBT2Hits( rawData );
    for(int layer = 0; layer<NumOfLayersFBT2; ++layer){
      for(int UorD = 0; UorD<2; ++UorD){
	int nh = hodoAna->GetNHitsFBT2(layer, UorD);
	for( int i=0; i<nh; ++i ){
	  const FiberHit* hit = hodoAna->GetHitFBT2(layer, UorD, i);
	  if(!hit) continue;
	  int mh  = hit->GetNLeading();
	  int seg   = hit->SegmentId();

	  bool hit_flag = false;
	  bool hit_flag2 = false;
	  // raw leading data
	  for( int m=0; m<mh; ++m ){
	    double leading  = hit->GetLeading(m);
	    if( 505<leading && leading<MaxTdcFBT2 ){
	      flagFBT = true;
	      hit_flag = true;
	      std::cout << "FBT2 : " << seg << ", " << leading<< std::endl;
	    } else if( MinTdcFBT2 <leading && leading<MaxTdcFBT2 ){
	      flagFBT = true;
	      hit_flag2 = true;
	      std::cout << "FBT2 : " << seg << ", " << leading<< std::endl;
	    }
	  }// for(m)
	  
	  int IdFBT = 0;
	  if (layer == 0 && UorD ==D)
	    IdFBT = IdFBT2_D1;
	  else if (layer == 0 && UorD ==U)
	    IdFBT = IdFBT2_U1;
	  else if (layer == 1 && UorD ==D)
	    IdFBT = IdFBT2_D2;
	  else if (layer == 1 && UorD ==U)
	    IdFBT = IdFBT2_U2;

	  if( hit_flag ){
	    gEvDisp.DrawHitHodoscope( IdFBT, seg );
	  } else if ( hit_flag2 ){
	    gEvDisp.DrawHitHodoscope( IdFBT, seg , 1, -1);
	  }

	}
      }
    }
  }


  DCAna->DecodeRawHits( rawData );

  // BcOut
  double multi_BcOut = 0.;
  {
    for( int layer=1; layer<=NumOfLayersBcOut; ++layer ){
      const DCHitContainer &contIn =DCAna->GetBcOutHC(layer);
      int nhIn=contIn.size();
      for( int i=0; i<nhIn; ++i ){
	// DCHit  *hit  = contIn[i];
	// double  wire = hit->GetWire();
	// int     mhit = hit->GetTdcSize();
	// bool    goodFlag = false;
	++multi_BcOut;
	// for (int j=0; j<mhit; j++) {
	//   if (hit->IsWithinRange(j)) {
	//     goodFlag = true;
	//     break;
	//   }
	// }
	// if( goodFlag )
	//   gEvDisp.DrawHitWire( layer+112, int(wire) );
      }
    }
  }
  multi_BcOut /= (double)NumOfLayersBcOut;
  /*
  if( multi_BcOut > MaxMultiHitBcOut ) {
    gEvDisp.GetCommand();
    return true;
  }
  */
  // SdcIn
  double multi_SdcIn = 0.;
  {
    //for( int layer=1; layer<=NumOfLayersSdcIn; ++layer ){
    for( int layer=1; layer<=NumOfLayersSDC1; ++layer ){
      const DCHitContainer &contIn =DCAna->GetSdcInHC(layer);
      int nhIn=contIn.size();
      if ( nhIn > MaxMultiHitSdcIn )
	continue;
      for( int i=0; i<nhIn; ++i ){
	DCHit  *hit  = contIn[i];
	double  wire = hit->GetWire();
	int     mhit = hit->GetTdcSize();
	bool    goodFlag = false;
	++multi_SdcIn;
	for (int j=0; j<mhit; j++) {
	  if (hit->IsWithinRange(j)) {
	    goodFlag = true;
	    break;
	  }
	}
	if( goodFlag )
	  gEvDisp.DrawHitWire( layer, int(wire) );
	else
	  gEvDisp.DrawHitWire( layer, int(wire), false, false );
      }
    }
  }
  multi_SdcIn /= (double)NumOfLayersSdcIn;
  if( multi_SdcIn > MaxMultiHitSdcIn ) {
    std::cout << "multi_SdcIn > " << MaxMultiHitSdcIn << std::endl;
    //return true;
  }


  // SdcOut
  double offset = flag_tof_stop ? 0 : dTOfs;
  DCAna->DecodeSdcOutHits( rawData, offset );
  DCAna->TotCutSDC2( MinTotSDC2 );
  DCAna->TotCutSDC3( MinTotSDC3 );

  double multi_SdcOut = 0.;
  {
    const int NumOfLayersSdcOut_wo_FBT = PlMaxSdcOut - PlMinSdcOut + 1;
    for( int layer=1; layer<=NumOfLayersSdcOut_wo_FBT; ++layer ){
      const DCHitContainer &contOut =DCAna->GetSdcOutHC(layer);
      int nhOut = contOut.size();
      if ( nhOut > MaxMultiHitSdcOut )
	continue;
      for( int i=0; i<nhOut; ++i ){
	DCHit  *hit  = contOut[i];
	double  wire = hit->GetWire();
	++multi_SdcOut;
	gEvDisp.DrawHitWire( layer+30, int(wire) );
      }
    }
  }
  multi_SdcOut /= (double)NumOfLayersSdcOut;

  if( multi_SdcOut > MaxMultiHitSdcOut ) {
    std::cout << "multi_SdcOut > " << MaxMultiHitSdcOut << std::endl;
    //gEvDisp.GetCommand();
    //return true;
  }

  int ntBcOut = 0;
  if( multi_BcOut<MaxMultiHitBcOut ){
    DCAna->TrackSearchBcOut(-1);
    ntBcOut = DCAna->GetNtracksBcOut();
    for( int it=0; it<ntBcOut; ++it ){
      DCLocalTrack *tp = DCAna->GetTrackBcOut( it );
      if( tp ) gEvDisp.DrawBcOutLocalTrack( tp );
    }
  }
  if( ntBcOut==0 ) {
    gEvDisp.GetCommand();
    return true;
  }

  int ntSdcIn = 0;
  if( multi_SdcIn<MaxMultiHitSdcIn ){
    std::cout << "TrackSearchSdcIn()" << std::endl;
    DCAna->TrackSearchSdcIn();
    ntSdcIn = DCAna->GetNtracksSdcIn();
    for( int it=0; it<ntSdcIn; ++it ){
      DCLocalTrack *tp = DCAna->GetTrackSdcIn( it );
      if( tp ) gEvDisp.DrawSdcInLocalTrack( tp );
    }
  }
  /*
  if( ntSdcIn==0 ) {
    gEvDisp.GetCommand();
    return true;
  }
  */
  int ntSdcOut = 0;
  if( multi_SdcOut<MaxMultiHitSdcOut ){
    std::cout << "TrackSearchSdcOut()" << std::endl;
    DCAna->TrackSearchSdcOut( TOFCont );
    ntSdcOut = DCAna->GetNtracksSdcOut();
    for( int it=0; it<ntSdcOut; ++it ){
      DCLocalTrack *tp = DCAna->GetTrackSdcOut( it );
      if( tp ) gEvDisp.DrawSdcOutLocalTrack( tp );
    }
  }
  /*
  if( ntSdcOut==0 ) {
    gEvDisp.GetCommand();
    return true;
  }
  */
  if ( flagFBT )
    gEvDisp.GetCommand();
  return true;

  std::vector<ThreeVector> KnPCont, KnXCont;
  std::vector<ThreeVector> KpPCont, KpXCont;

  int ntKurama = 0;
  static int ntKurama_all = 0;
  if( ntSdcIn>0 && ntSdcOut>0 ){
    //if( ntSdcIn==1 && ntSdcOut==1 ){
    bool through_target = false;
    DCAna->TrackSearchKurama();
    ntKurama = DCAna->GetNTracksKurama();
    ntKurama_all++;
    for( int it=0; it<ntKurama; ++it ){
      KuramaTrack *tp = DCAna->GetKuramaTrack( it );
      if( !tp ) continue;
      tp->Print( "in "+func_name );
      const ThreeVector& postgt = tp->PrimaryPosition();
      const ThreeVector& momtgt = tp->PrimaryMomentum();
      double path = tp->PathLengthToTOF();
      double p    = momtgt.Mag();
      gEvDisp.DrawMomentum( p );
      if( std::abs( postgt.x() )<50. &&
	  std::abs( postgt.y() )<30. ){
	through_target = true;
      }
      // MassSquare
      double tofseg = tp->TofSeg();
      for( int j=0, n=TOFCont.size(); j<n; ++j ){
      	Hodo2Hit *hit = TOFCont[j];
      	if( !hit ) continue;
      	double seg = hit->SegmentId()+1;
	if( tofseg != seg ) continue;
      	double stof = hit->CMeanTime()-time0+OffsetToF;
      	if( stof<=0 ) continue;
      	double m2 = Kinematics::MassSquare( p, path, stof );
      	gEvDisp.DrawMassSquare( m2 );
	KpPCont.push_back( momtgt );
	KpXCont.push_back( postgt );
      }
    }
    if( through_target ) gEvDisp.DrawTarget();
    static double KuramaOk = 0.;
    KuramaOk += ( ntKurama>0 );
  }

  std::vector<double> BftXCont;
  ////////// BFT
  {
    hodoAna->DecodeBFTHits(rawData);
    // Fiber Cluster
    hodoAna->TimeCutBFT(MinTimeBFT, MaxTimeBFT);
    int ncl = hodoAna->GetNClustersBFT();
    for( int i=0; i<ncl; ++i ){
      FiberCluster *cl = hodoAna->GetClusterBFT(i);
      if( !cl ) continue;
      double pos    = cl->MeanPosition();
      BftXCont.push_back( pos );
    }
  }

  // K18TrackingD2U
  DCAna->TrackSearchK18D2U( BftXCont );
  int ntK18=DCAna->GetNTracksK18D2U();
  if( ntK18==0 ) return true;
  for( int i=0; i<ntK18; ++i ){
    K18TrackD2U *tp=DCAna->GetK18TrackD2U(i);
    if(!tp) continue;
    double x = tp->Xtgt(), y = tp->Ytgt();
    double u = tp->Utgt(), v = tp->Vtgt();
    double p = tp->P3rd();
    double pt = p/std::sqrt(1.+u*u+v*v);
    ThreeVector Pos( x, y, 0. );
    ThreeVector Mom( pt*u, pt*v, pt );
    KnPCont.push_back( Mom );
    KnXCont.push_back( Pos );
  }

#if 1
  if( KnPCont.size()==1 && KpPCont.size()==1 ){
    ThreeVector pkp = KpPCont[0];
    ThreeVector pkn = KnPCont[0];
    ThreeVector xkp = KpXCont[0];
    ThreeVector xkn = KnXCont[0];
    ThreeVector vertex = Kinematics::VertexPoint( xkn, xkp, pkn, pkp );
    LorentzVector LvKn( KnPCont[0], std::sqrt( KaonMass*KaonMass+pkn.Mag2()) );
    LorentzVector LvKp( KpPCont[0], std::sqrt( KaonMass*KaonMass+pkp.Mag2() ) );
    LorentzVector LvP( 0., 0., 0., ProtonMass );
    LorentzVector LvRp = LvKn+LvP-LvKp;
    ThreeVector MissMom = LvRp.Vect();
    gEvDisp.DrawVertex( vertex );
    gEvDisp.DrawMissingMomentum( MissMom, vertex );
  }
#endif

  return true;
}

//______________________________________________________________________________
bool
UserEventDisplay::ProcessingEnd( void )
{
  // gEvDisp.GetCommand();
  gEvDisp.EndOfEvent();
  // if( utility::UserStop() ) gEvDisp.Run();
  return true;
}

//______________________________________________________________________________
VEvent*
ConfMan::EventAllocator( void )
{
  return new UserEventDisplay;
}

//______________________________________________________________________________
bool
ConfMan:: InitializeHistograms( void )
{
  return true;
}

//______________________________________________________________________________
bool
ConfMan::InitializeParameterFiles( void )
{
  return
    ( InitializeParameter<DCGeomMan>("DCGEO")        &&
      InitializeParameter<DCDriftParamMan>("DCDRFT") &&
      InitializeParameter<DCTdcCalibMan>("DCTDC")    &&
      InitializeParameter<HodoParamMan>("HDPRM")     &&
      InitializeParameter<HodoPHCMan>("HDPHC")       &&
      InitializeParameter<FieldMan>("FLDMAP")        &&
      InitializeParameter<K18TransMatrix>("K18TM")   &&
      InitializeParameter<UserParamMan>("USER")      &&
      InitializeParameter<EventDisplay>()            );
}

//______________________________________________________________________________
bool
ConfMan::FinalizeProcess( void )
{
  return true;
}
