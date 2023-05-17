// -*- C++ -*-

#include "FiberHit.hh"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <limits>

#include <std_ostream.hh>

#include "ConfMan.hh"
#include "DCGeomMan.hh"
#include "DebugCounter.hh"
#include "DeleteUtility.hh"
#include "FLHit.hh"
#include "FuncName.hh"
#include "HodoParamMan.hh"
#include "HodoPHCMan.hh"
#include "RawData.hh"

namespace
{
const auto qnan = TMath::QuietNaN();
const auto& gGeom = DCGeomMan::GetInstance();
const auto& gHodo = HodoParamMan::GetInstance();
const auto& gPHC  = HodoPHCMan::GetInstance();
}

//_____________________________________________________________________________
FiberHit::FiberHit(HodoRawHit *object)
  : HodoHit(object),
    m_paired_plane(),
    m_paired_segment(),
    m_ud(0),
    m_position(qnan),
    m_offset(0),
    m_pair_id(0),
    m_adc_hg(),
    m_adc_lg(),
    m_pedcor_hg(),
    m_pedcor_lg(),
    m_mip_hg(),
    m_mip_lg(),
    m_dE_hg(0.),
    m_dE_lg(0.)
{
  debug::ObjectCounter::increase(ClassName());
}

//_____________________________________________________________________________
FiberHit::~FiberHit()
{
  del::ClearContainer(m_hit_container);
  debug::ObjectCounter::decrease(ClassName());
}

//_____________________________________________________________________________
bool
FiberHit::Calculate()
{
  if(m_is_calculated){
    hddaq::cerr << FUNC_NAME << " already calculated" << std::endl;
    return false;
  }

  if(!HodoHit::Calculate())
    return false;

  HodoHit::Print();
  return true;
  // Detector information
  Int_t id    = m_raw->DetectorId();
  Int_t plane = m_raw->PlaneId();
  Int_t seg   = m_raw->SegmentId();

  // Geometry calibration
  m_ud = 0; // BFT is allways U
  const auto& detector_name = m_raw->DetectorName();
  if("BFT" == detector_name || "SFT-X" == detector_name){
    // case of BFT and SFT X layers
    // They have up and down planes in 1 layer.
    // We treat these 2 planes as 1 dimentional plane.
    // if(1 == m_raw->PlaneId()){
    if (1 == m_raw->PlaneId()%2) {
      // case of down plane
      m_offset = 0.5;
      m_pair_id  = 1;
    }
    m_pair_id += 2*m_raw->SegmentId();
  }else{
    // case of SFT UV layers & CFT
    // They have only 1 plane in 1 layer.
    m_pair_id = m_raw->SegmentId();
  }

  Int_t DetectorId = gGeom.GetDetectorId(detector_name);
  m_position       = gGeom.CalcWirePosition(DetectorId, seg);

  // hit information
  Int_t multi_hit_l = m_ud==0 ?
    m_raw->GetSizeTdcLeading(0) :
    m_raw->GetSizeTdcLeading(1);
  Int_t multi_hit_t = m_ud==0 ?
    m_raw->GetSizeTdcTrailing(0) :
    m_raw->GetSizeTdcTrailing(1);

  std::vector<Int_t> leading_cont, trailing_cont;
  {
    for(Int_t m=0; m<multi_hit_l; ++m) {
      leading_cont.push_back(m_ud==0? m_raw->GetTdcLeading(0, m) : m_raw->GetTdcLeading(1, m));
    }
    for(Int_t m=0; m<multi_hit_t; ++m) {
      trailing_cont.push_back(m_ud==0? m_raw->GetTdcTrailing(0, m): m_raw->GetTdcTrailing(1, m));
    }

    std::sort(leading_cont.begin(),  leading_cont.end(),  std::greater<Int_t>());
    std::sort(trailing_cont.begin(), trailing_cont.end(), std::greater<Int_t>());

    Int_t i_t = 0;
    for(Int_t i=0; i<multi_hit_l; ++i){
      data_pair a_pair;

      Int_t leading  = leading_cont.at(i);
      while(i_t < multi_hit_t){
	Int_t trailing = trailing_cont.at(i_t);

	if(leading > trailing){
	  a_pair.index_t = i_t;
	  m_pair_cont.push_back(a_pair);
	  break;
	}else{
	  ++i_t;
	}// Goto next trailing
      }

      if(i_t == multi_hit_t){
	a_pair.index_t = -1;
	m_pair_cont.push_back(a_pair);
	continue;
      }// no more trailing data
    }// for(i)
  }

  // Delete duplication index_t
  for(Int_t i = 0; i<multi_hit_l-1; ++i){
    if(true
       && m_pair_cont.at(i).index_t != -1
       && m_pair_cont.at(i).index_t == m_pair_cont.at(i+1).index_t)
    {
      m_pair_cont.at(i).index_t = -1;
    }
  }// for(i)

  for(Int_t i = 0; i<multi_hit_l; ++i){
    // leading
    Int_t leading = leading_cont.at(i);
    Double_t time_leading = qnan;
    if(!gHodo.GetTime(id, plane, seg, m_ud, leading, time_leading)){
      hddaq::cerr << FUNC_NAME
		  << " something is wrong at GetTime("
		  << id  << ", " << plane          << ", " << seg  << ", "
		  << m_ud << ", " << leading  << ", " << time_leading << ")" << std::endl;
      return false;
    }
    // m_t.push_back(time_leading);
    m_flag_join.push_back(false);

    // Is there paired trailing ?
    if(m_pair_cont.at(i).index_t == -1){
      m_pair_cont.at(i).time_l  = time_leading;
      m_pair_cont.at(i).time_t  = std::numeric_limits<Double_t>::quiet_NaN();
      m_pair_cont.at(i).ctime_l = time_leading;
      m_pair_cont.at(i).tot     = std::numeric_limits<Double_t>::quiet_NaN();
      continue;
    }

    // trailing
    Int_t trailing = trailing_cont.at(m_pair_cont.at(i).index_t);
    Double_t time_trailing = qnan;
    if(!gHodo.GetTime(id, plane, seg, m_ud, trailing, time_trailing)){
      hddaq::cerr << FUNC_NAME
		  << " something is wrong at GetTime("
		  << id  << ", " << plane          << ", " << seg  << ", "
		  << m_ud << ", " << trailing  << ", " << time_trailing << ")" << std::endl;
      return false;
    }

    Double_t tot           = time_trailing - time_leading;
    Double_t ctime_leading = time_leading;
    gPHC.DoCorrection(id, plane, seg, m_ud, time_leading, tot, ctime_leading);

    // m_de.at(HodoRawHit::kUp).push_back(tot);
    // m_ct.push_back(ctime_leading);

    m_pair_cont.at(i).time_l  = time_leading;
    m_pair_cont.at(i).time_t  = time_trailing;
    m_pair_cont.at(i).ctime_l = ctime_leading;
    m_pair_cont.at(i).tot     = tot;
  }// for(i)

#if 0
  if(id==113){// CFT ADC
    Double_t nhit_adc = m_raw->GetSizeAdc1();
    if(nhit_adc>0){
      Double_t hi  =  m_raw->GetAdc1();
      Double_t low =  m_raw->GetAdc2();
      Double_t pedeHi  = gHodo.GetP0(id, plane, seg, 0);
      Double_t pedeLow = gHodo.GetP0(id, plane, seg, 1);
      Double_t gainHi  = gHodo.GetP1(id, plane, seg, 0);// pedestal+mip(or peak)
      Double_t gainLow = gHodo.GetP1(id, plane, seg, 1);// pedestal+mip(or peak)
      Double_t Alow = gHodo.GetP0(id,plane,0/*seg*/,3);// same value for the same layer
      Double_t Blow = gHodo.GetP1(id,plane,0/*seg*/,3);// same value for the same layer

      if (hi>0)
	m_adc_hg  = hi  - pedeHi;
      if (low>0)
	m_adc_lg = low - pedeLow;
      //m_mip_hg  = (hi  - pedeHi)/(gainHi  - pedeHi);
      //m_mip_lg = (low - pedeLow)/(gainLow - pedeLow);
      if (m_pedcor_hg>-2000 && hi >0)
	m_adc_hg  = hi  + m_pedcor_hg;
      if (m_pedcor_lg>-2000 && low >0)
	m_adc_lg  = low  + m_pedcor_lg;

      if (m_adc_hg>0/* && gainHi > 0*/)
	m_mip_hg  = m_adc_hg/gainHi ;
      if (m_adc_lg>0/* && gainLow >0*/)
	m_mip_lg = m_adc_lg/gainLow;

      if(m_mip_lg>0){
	m_dE_lg = -(Alow/Blow) * log(1. - m_mip_lg/Alow);// [MeV]
	if(1-m_mip_lg/Alow<0){ // when pe is too big
	  m_dE_lg = -(Alow/Blow) * log(1. - (Alow-0.001)/Alow);// [MeV] almost max
	}
      }else{
	m_dE_lg = 0;// [MeV]
      }
#if 0
      if(m_dE_lg > 10){
        std::cout << "layer = " << plane << ", seg = " << seg << ", adcLow = "
                  << m_adc_lg << ", dE = " << m_dE_lg << ", mip_lg = "
                  << m_mip_lg << ", gainLow = " << gainLow << ", gainHi = "
                  << gainHi << std::endl;
      }
#endif
      for(auto& pair: m_pair_cont){
	Double_t time= pair.time_l;
	Double_t ctime = -100;
	if (m_adc_hg>20) {
	  gPHC.DoCorrection(id, plane, seg, m_ud, time, m_adc_hg, ctime);
	  pair.ctime_l = ctime;
	} else
	  pair.ctime_l = time;
      }
    }
  }
#endif
  m_is_calculated = true;
  return true;
}

//_____________________________________________________________________________
void
FiberHit::Print(const TString& arg, std::ostream& ost) const
{
  ost << FUNC_NAME << " " << arg << '\n'
      << std::setw(10) << "DetectorName" << std::setw(8)
      << m_raw->DetectorName() << '\n'
      << std::setw(10) << "Segment"      << std::setw(8)
      << m_raw->SegmentId()    << '\n'
      << std::setw(10) << "Position"     << std::setw(8) << m_position      << '\n'
      << std::setw(10) << "Offset"       << std::setw(8) << m_offset        << '\n'
      << std::setw(10) << "PairID"       << std::setw(8) << m_pair_id       << '\n';
}
