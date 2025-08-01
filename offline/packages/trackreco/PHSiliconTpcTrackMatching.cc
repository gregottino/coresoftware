#include "PHSiliconTpcTrackMatching.h"

/// Tracking includes
#include <trackbase/MvtxDefs.h>
#include <trackbase/TrackFitUtils.h>
#include <trackbase/TpcDefs.h>
#include <trackbase/TrkrClusterContainer.h>
#include <trackbase/TrkrClusterCrossingAssoc.h>
#include <trackbase/TrkrClusterv3.h>
#include <trackbase/TrkrDefs.h>  // for cluskey, getTrkrId, tpcId

#include <trackbase_historic/SvtxTrackSeed_v2.h>
#include <trackbase_historic/TrackSeedContainer_v1.h>
#include <trackbase_historic/TrackSeed_v2.h>
#include <trackbase_historic/TrackSeedHelper.h>

#include <globalvertex/SvtxVertex.h>  // for SvtxVertex
#include <globalvertex/SvtxVertexMap.h>

#include <g4main/PHG4Hit.h>       // for PHG4Hit
#include <g4main/PHG4HitDefs.h>   // for keytype
#include <g4main/PHG4Particle.h>  // for PHG4Particle

#include <fun4all/Fun4AllReturnCodes.h>

#include <phool/PHCompositeNode.h>
#include <phool/getClass.h>
#include <phool/phool.h>
#include <phool/sphenix_constants.h>

#include <TF1.h>
#include <TFile.h>
#include <TNtuple.h>

#include <climits>   // for UINT_MAX
#include <cmath>     // for fabs, sqrt
#include <iostream>  // for operator<<, basic_ostream
#include <memory>
#include <set>      // for _Rb_tree_const_iterator
#include <utility>  // for pair

using namespace std;

//____________________________________________________________________________..
PHSiliconTpcTrackMatching::PHSiliconTpcTrackMatching(const std::string &name)
  : SubsysReco(name)
  , PHParameterInterface(name)
{
  InitializeParameters();
}

//____________________________________________________________________________..
PHSiliconTpcTrackMatching::~PHSiliconTpcTrackMatching() = default;

//____________________________________________________________________________..
int PHSiliconTpcTrackMatching::InitRun(PHCompositeNode *topNode)
{
  UpdateParametersWithMacro();
  if(_test_windows)
  {
  _file = new TFile(_file_name.c_str(), "RECREATE");
  _tree = new TNtuple("track_match", "track_match",
                      "event:sicrossing:siq:siphi:sieta:six:siy:siz:sipx:sipy:sipz:tpcq:tpcphi:tpceta:tpcx:tpcy:tpcz:tpcpx:tpcpy:tpcpz:tpcid:siid");
  }
  // put these in the output file
  cout << PHWHERE << " Search windows: phi " << _phi_search_win << " eta "
       << _eta_search_win << " _pp_mode " << _pp_mode << " _use_intt_crossing " << _use_intt_crossing << endl;

  int ret = GetNodes(topNode);
  if (ret != Fun4AllReturnCodes::EVENT_OK)
  {
    return ret;
  }
  std::istringstream stringline(m_fieldMap);
  stringline >> fieldstrength;

  // initialize the WindowMatchers
  window_dx.init_bools("dx", _print_windows || Verbosity()   >0);
  window_dy.init_bools("dy", _print_windows || Verbosity()   >0);
  window_dz.init_bools("dz", _print_windows || Verbosity()   >0);
  window_dphi.init_bools("dphi", _print_windows || Verbosity() >0);
  window_deta.init_bools("deta", _print_windows || Verbosity() >0);


  return ret;
}

//_____________________________________________________________________
void PHSiliconTpcTrackMatching::SetDefaultParameters()
{
  // Data on gasses @20 C and 760 Torr from the following source:
  // http://www.slac.stanford.edu/pubs/icfa/summer98/paper3/paper3.pdf
  // diffusion and drift velocity for 400kV for NeCF4 50/50 from calculations:
  // http://skipper.physics.sunysb.edu/~prakhar/tpc/HTML_Gases/split.html

  return;
}

std::string PHSiliconTpcTrackMatching::WindowMatcher::print_fn(const Arr3D& dat) {
  std::ostringstream os;
  if (dat[1]==0.) {
    os << dat[0];
  } else {
    os << dat[0] << (dat[1]>0 ? "+" : "") << dat[1] <<"*exp("<<dat[2]<<"/pT)";
  }
  return os.str();
}

void PHSiliconTpcTrackMatching::WindowMatcher::init_bools(const std::string& tag, const bool print) {
  // set values for positive tracks
  fabs_max_posQ = (posLo[0]==100.);
  posLo_b0 = (posLo[1]==0.);
  posHi_b0 = (posHi[1]==0.);
  // if no values for negative tracks, copy over from positive tracks
  if (negHi[0]==100.) {
    negLo = posLo;
    negHi = posHi;
    fabs_max_negQ = fabs_max_posQ;
    negLo_b0 = posLo_b0;
    negHi_b0 = posHi_b0;
    min_pt_negQ = min_pt_posQ;
  } else {
    fabs_max_negQ = (negLo[0]==100.);
    negLo_b0 = (negLo[1]==0.);
    negHi_b0 = (negHi[1]==0.);
  }
  if (print) {
    std::cout << " Track matching window, " << tag << ":" << std::endl;

    if (posHi==negHi && posLo == negLo) {
      std::cout << "  all tracks: ";
    } else {
      std::cout << "   +Q tracks: ";
    }
    if (posLo[0]==100) {
      std::cout << "  |" << tag <<"| < " << print_fn(posHi) << std::endl;
    } else {
      std::cout << print_fn(posLo) <<" < " << tag << " < " << print_fn(posHi) << std::endl;
    }

    if (posHi != negHi || posLo != negLo) {
      std::cout << "   -Q tracks: ";
      if (negLo[0]==100) {
        std::cout << "  |" << tag <<"| < " << print_fn(negHi) << std::endl;
      } else {
        std::cout << print_fn(negLo) <<" < " << tag << " < " << print_fn(negHi) << std::endl;
      }
    }
  }

}

bool PHSiliconTpcTrackMatching::WindowMatcher::in_window
(const bool posQ, const double tpc_pt, const double tpc_X, const double si_X)
{
  const auto delta = tpc_X-si_X;
  
  if (posQ) {
    double pt = (tpc_pt<min_pt_posQ) ? min_pt_posQ : tpc_pt;
    if (fabs_max_posQ) {
      return fabs(delta) < fn_exp(posHi, posHi_b0, pt);
    } else {
      return (delta > fn_exp(posLo, posLo_b0, pt)
           && delta < fn_exp(posHi, posHi_b0, pt));
    }
  } else {
    double pt = (tpc_pt<min_pt_negQ) ? min_pt_negQ : tpc_pt;
    if (fabs_max_negQ) {
      return fabs(delta) < fn_exp(negHi, negHi_b0, pt);
    } else {
      return (delta > fn_exp(negLo, negLo_b0, pt)
           && delta < fn_exp(negHi, negHi_b0, pt));
    }
  }
}

//____________________________________________________________________________..
int PHSiliconTpcTrackMatching::process_event(PHCompositeNode * /*unused*/)
{
  if(Verbosity() > 2)
  {
    std::cout << " Warning: PHSiliconTpcTrackMatching "
      << ( _zero_field ? "zero field is ON" : " zero field is OFF") << std::endl;
  }
  // _track_map contains the TPC seed track stubs
  // _track_map_silicon contains the silicon seed track stubs
  // _svtx_seed_map contains the combined silicon and tpc track seeds

    // in case these objects are in the input file, we clear the nodes and replace them
  _svtx_seed_map->Reset(); 
  
  if (Verbosity() > 0)
  {
    cout << PHWHERE << " TPC track map size " << _track_map->size() << " Silicon track map size " << _track_map_silicon->size() << endl;
  }

  if (_track_map->size() == 0)
  {
    return Fun4AllReturnCodes::EVENT_OK;
  }

  // loop over the silicon seeds and add the crossing to them
  for (unsigned int trackid = 0; trackid != _track_map_silicon->size(); ++trackid)
  {
    _tracklet_si = _track_map_silicon->get(trackid);
    if (!_tracklet_si)
    {
      continue;
    }
    auto crossing = _tracklet_si->get_crossing();
    if (Verbosity() > 8)
    {
      std::cout << " silicon stub: " << trackid << " eta " << _tracklet_si->get_eta()
        << " pt " << _tracklet_si->get_pt() << " si z " << TrackSeedHelper::get_z(_tracklet_si)
        << " crossing " << crossing << std::endl;
    }

    if (Verbosity() > 1)
    {
      cout << " Si track " << trackid << " crossing " << crossing << endl;
    }
  }

  // Find all matches of tpc and si tracklets in eta and phi, x and y
  std::multimap<unsigned int, unsigned int> tpc_matches;
  std::set<unsigned int> tpc_matched_set;
  std::set<unsigned int> tpc_unmatched_set;
  findEtaPhiMatches(tpc_matched_set, tpc_unmatched_set, tpc_matches);

  // check z matching for all matches of tpc and si
  // for _pp_mode=false, assume zero crossings for all tracks
  // for _pp_mode=true, correct tpc seed z according to crossing number, do nothing if crossing number is not set
  // remove matches from tpc_matches if z matching is not satisfied
  std::multimap<unsigned int, unsigned int> bad_map;
  checkZMatches(tpc_matches, bad_map);

  // update tpc_matched_set and tpc_unmatched_set
  tpc_matched_set.clear();
  for (const auto& [key, _] : tpc_matches)
  {
    tpc_matched_set.insert(key);
  }

  for (const auto& [key, _] : bad_map)
  {
    if (!tpc_matched_set.count(key))
    {
        tpc_unmatched_set.insert(key);
    }
  }

  // We have a complete list of all eta/phi matched tracks in the map "tpc_matches"
  // make the combined track seeds from tpc_matches
  for (auto [tpcid, si_id] : tpc_matches)
  {
    auto svtxseed = std::make_unique<SvtxTrackSeed_v2>();
    svtxseed->set_silicon_seed_index(si_id);
    svtxseed->set_tpc_seed_index(tpcid);
    // In pp mode, if a matched track does not have INTT clusters we have to find the crossing geometrically
    // Record the geometrically estimated crossing in the track seeds for later use if needed
    short int crossing_estimate = findCrossingGeometrically(tpcid, si_id);
    svtxseed->set_crossing_estimate(crossing_estimate);
    _svtx_seed_map->insert(svtxseed.get());

    if (Verbosity() > 1)
    {
      std::cout << "  combined seed id " << _svtx_seed_map->size() - 1 << " si id " << si_id << " tpc id " << tpcid << " crossing estimate " << crossing_estimate << std::endl;
    }
  }

  // Also make the unmatched TPC seeds into SvtxTrackSeeds
  for (auto tpcid : tpc_unmatched_set)
  {
    auto svtxseed = std::make_unique<SvtxTrackSeed_v2>();
    svtxseed->set_tpc_seed_index(tpcid);
    _svtx_seed_map->insert(svtxseed.get());

    if (Verbosity() > 1)
    {
      std::cout << "  converted unmatched TPC seed id " << _svtx_seed_map->size() - 1 << " tpc id " << tpcid << std::endl;
    }
  }

  if (Verbosity() > 0)
  {
    std::cout << "final svtx seed map size " << _svtx_seed_map->size() << std::endl;
  }

  if (Verbosity() > 1)
  {
    for (const auto &seed : *_svtx_seed_map)
    {
      seed->identify();
      std::cout << std::endl;
    }

    cout << "PHSiliconTpcTrackMatching::process_event(PHCompositeNode *topNode) Leaving process_event" << endl;
  }
  m_event++;
  return Fun4AllReturnCodes::EVENT_OK;
}

short int PHSiliconTpcTrackMatching::findCrossingGeometrically(unsigned int tpcid, unsigned int si_id)
{
  // loop over all matches and check for ones with no INTT clusters in the silicon seed
  TrackSeed *si_track = _track_map_silicon->get(si_id);
  const short int crossing = si_track->get_crossing();
  const double si_z = TrackSeedHelper::get_z(si_track);

  TrackSeed *tpc_track = _track_map->get(tpcid);
  const double tpc_z = TrackSeedHelper::get_z(tpc_track);

  // this is an initial estimate of the bunch crossing based on the z-mismatch of the seeds for this track
  short int crossing_estimate = (short int) getBunchCrossing(tpcid, tpc_z - si_z);

  if (Verbosity() > 1)
  {
    std::cout << "findCrossing: "
              << " tpcid " << tpcid << " si_id " << si_id << " tpc_z " << tpc_z << " si_z " << si_z << " dz " << tpc_z - si_z
              << " INTT crossing " << crossing << " crossing_estimate " << crossing_estimate << std::endl;
  }

  return crossing_estimate;
}

double PHSiliconTpcTrackMatching::getBunchCrossing(unsigned int trid, double z_mismatch)
{
  const double vdrift = _tGeometry->get_drift_velocity();  // cm/ns
  const double z_bunch_separation = sphenix_constants::time_between_crossings * vdrift; // cm

  // The sign of z_mismatch will depend on which side of the TPC the tracklet is in
  TrackSeed *track = _track_map->get(trid);

  // crossing
  double crossings = z_mismatch / z_bunch_separation;

  // Check the TPC side for the first cluster in the track
  unsigned int side = 10;
  std::set<short int> side_set;
  for (TrackSeed::ConstClusterKeyIter iter = track->begin_cluster_keys();
       iter != track->end_cluster_keys();
       ++iter)
  {
    TrkrDefs::cluskey cluster_key = *iter;
    unsigned int trkrid = TrkrDefs::getTrkrId(cluster_key);
    if (trkrid == TrkrDefs::tpcId)
    {
      side = TpcDefs::getSide(cluster_key);
      side_set.insert(side);
    }
  }

  if (side == 10)
  {
    return SHRT_MAX;
  }

  if (side_set.size() == 2 && Verbosity() > 1)
  {
    std::cout << "     WARNING: tpc seed " << trid << " changed TPC sides, "
              << "  final side " << side << std::endl;
  }

  // if side = 1 (north, +ve z side), a positive t0 will make the cluster late relative to true z, so it will look like z is less positive
  // so a negative z mismatch for side 1 means a positive t0, and positive crossing, so reverse the sign for side 1
  if (side == 1)
  {
    crossings *= -1.0;
  }

  if (Verbosity() > 1)
  {
    std::cout << "  gettrackid " << trid << " side " << side << " z_mismatch " << z_mismatch << " crossings " << crossings << std::endl;
  }

  return crossings;
}

int PHSiliconTpcTrackMatching::End(PHCompositeNode * /*unused*/)
{
  if(_test_windows)
  {
  _file->cd();
  _tree->Write();
  _file->Close();
  }
  return Fun4AllReturnCodes::EVENT_OK;
}

int PHSiliconTpcTrackMatching::GetNodes(PHCompositeNode *topNode)
{
  //---------------------------------
  // Get additional objects off the Node Tree
  //---------------------------------

  _cluster_crossing_map = findNode::getClass<TrkrClusterCrossingAssoc>(topNode, "TRKR_CLUSTERCROSSINGASSOC");
  if (!_cluster_crossing_map)
  {
    //cerr << PHWHERE << " ERROR: Can't find TRKR_CLUSTERCROSSINGASSOC " << endl;
    // return Fun4AllReturnCodes::ABORTEVENT;
  }

  _track_map_silicon = findNode::getClass<TrackSeedContainer>(topNode, _silicon_track_map_name);
  if (!_track_map_silicon)
  {
    cerr << PHWHERE << " ERROR: Can't find SiliconTrackSeedContainer " << endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  _track_map = findNode::getClass<TrackSeedContainer>(topNode, _track_map_name);
  if (!_track_map)
  {
    cerr << PHWHERE << " ERROR: Can't find " << _track_map_name.c_str() << endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  _svtx_seed_map = findNode::getClass<TrackSeedContainer>(topNode, "SvtxTrackSeedContainer");
  if (!_svtx_seed_map)
  {
    std::cout << "Creating node SvtxTrackSeedContainer" << std::endl;
    /// Get the DST Node
    PHNodeIterator iter(topNode);
    PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));

    /// Check that it is there
    if (!dstNode)
    {
      std::cerr << "DST Node missing, quitting" << std::endl;
      throw std::runtime_error("failed to find DST node in PHActsSourceLinks::createNodes");
    }

    /// Get the tracking subnode
    PHNodeIterator dstIter(dstNode);
    PHCompositeNode *svtxNode = dynamic_cast<PHCompositeNode *>(dstIter.findFirst("PHCompositeNode", "SVTX"));

    /// Check that it is there
    if (!svtxNode)
    {
      svtxNode = new PHCompositeNode("SVTX");
      dstNode->addNode(svtxNode);
    }

    _svtx_seed_map = new TrackSeedContainer_v1();
    PHIODataNode<PHObject> *node = new PHIODataNode<PHObject>(_svtx_seed_map, "SvtxTrackSeedContainer", "PHObject");
    svtxNode->addNode(node);
  }

  _cluster_map = findNode::getClass<TrkrClusterContainer>(topNode, "TRKR_CLUSTER");
  if (!_cluster_map)
  {
    std::cout << PHWHERE << " ERROR: Can't find node TRKR_CLUSTER" << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  _tGeometry = findNode::getClass<ActsGeometry>(topNode, "ActsGeometry");
  if (!_tGeometry)
  {
    std::cout << PHWHERE << "Error, can't find acts tracking geometry" << std::endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

void PHSiliconTpcTrackMatching::findEtaPhiMatches(
    std::set<unsigned int> &tpc_matched_set,
    std::set<unsigned int> &tpc_unmatched_set,
    std::multimap<unsigned int, unsigned int> &tpc_matches)
{
  // loop over the TPC track seeds
  for (unsigned int phtrk_iter = 0;
       phtrk_iter < _track_map->size();
       ++phtrk_iter)
  {
    _tracklet_tpc = _track_map->get(phtrk_iter);
    if (!_tracklet_tpc)
    {
      continue;
    }

    unsigned int tpcid = phtrk_iter;
    if (Verbosity() > 1)
    {
      std::cout
          << __LINE__
          << ": Processing seed itrack: " << tpcid
          << ": nhits: " << _tracklet_tpc->size_cluster_keys()
          << ": Total tracks: " << _track_map->size()
          << ": phi: " << _tracklet_tpc->get_phi()
          << endl;
    }

    double tpc_phi, tpc_eta, tpc_pt;
    float tpc_px, tpc_py, tpc_pz;
    int tpc_q;
    Acts::Vector3 tpc_pos;
    if (_zero_field) {
      auto cluster_list = getTrackletClusterList(_tracklet_tpc);

      Acts::Vector3  mom;
      bool ok_track;

      std::tie(ok_track, tpc_phi, tpc_eta, tpc_pt, tpc_pos, mom) =
        TrackFitUtils::zero_field_track_params(_tGeometry, _cluster_map, cluster_list);
      if (!ok_track) { continue; }
      tpc_px = mom.x();
      tpc_py = mom.y();
      tpc_pz = mom.z();
      tpc_q = -100;
    } else {
      tpc_phi = _tracklet_tpc->get_phi();
      tpc_eta = _tracklet_tpc->get_eta();
      tpc_pt = fabs(1. / _tracklet_tpc->get_qOverR()) * (0.3 / 100.) * fieldstrength;

      tpc_pos = TrackSeedHelper::get_xyz(_tracklet_tpc);

      tpc_px = _tracklet_tpc->get_px();
      tpc_py = _tracklet_tpc->get_py();
      tpc_pz = _tracklet_tpc->get_pz();

      tpc_q = _tracklet_tpc->get_charge();
    }

    bool is_posQ = (tpc_q>0.);

    if (Verbosity() > 8)
    {
      std::cout << " tpc stub: " << tpcid << " eta " << tpc_eta << " phi " << tpc_phi << " pt " << tpc_pt << " tpc z " << TrackSeedHelper::get_z(_tracklet_tpc) << std::endl;
    }

    if (Verbosity() > 3)
    {
      cout << "TPC tracklet:" << endl;
      _tracklet_tpc->identify();
    }


    bool matched = false;

    // Now search the silicon track list for a match in eta and phi
    for (unsigned int phtrk_iter_si = 0;
         phtrk_iter_si < _track_map_silicon->size();
         ++phtrk_iter_si)
    {
      _tracklet_si = _track_map_silicon->get(phtrk_iter_si);
      if (!_tracklet_si)
      {

        continue;
      }

      double si_phi, si_eta, si_pt;
      float si_px, si_py, si_pz;
      int si_q;
      Acts::Vector3 si_pos;
      if (_zero_field) {
      auto cluster_list = getTrackletClusterList(_tracklet_si);

      Acts::Vector3  mom;
      bool ok_track;

      std::tie(ok_track, si_phi, si_eta, si_pt, si_pos, mom) =
        TrackFitUtils::zero_field_track_params(_tGeometry, _cluster_map, cluster_list);
      if (!ok_track) { continue; }
        si_px = mom.x();
        si_py = mom.y();
        si_pz = mom.z();
        si_q = -100;
      } else {
        si_eta = _tracklet_si->get_eta();
        si_phi = _tracklet_si->get_phi();

        si_pos = TrackSeedHelper::get_xyz(_tracklet_si);
        si_px = _tracklet_si->get_px();
        si_py = _tracklet_si->get_py();
        si_pz = _tracklet_si->get_pz();
        si_q = _tracklet_si->get_charge();
      }
      int si_crossing = _tracklet_si->get_crossing();
      unsigned int siid = phtrk_iter_si;

      if(_test_windows)
      {
        float data[] = {
          (float) m_event, (float) si_crossing,
          (float) si_q, (float) si_phi, (float) si_eta, (float) si_pos.x(), (float) si_pos.y(), (float) si_pos.z(), (float) si_px, (float) si_py, (float) si_pz,
          (float) tpc_q, (float) tpc_phi, (float) tpc_eta, (float) tpc_pos.x(), (float) tpc_pos.y(), (float) tpc_pos.z(), (float) tpc_px, (float) tpc_py, (float) tpc_pz,
          (float) tpcid, (float) siid
	};
        _tree->Fill(data);
      }

      bool eta_match = false;
      if (window_deta.in_window(is_posQ, tpc_pt, tpc_eta, si_eta))
      {
        eta_match = true;
      }
      else if (fabs(tpc_eta-si_eta) < _deltaeta_min)
      {
	eta_match = true;
      }
      if (!eta_match)
      {
        continue;
      }

      bool position_match = false;
      if (window_dx.in_window(is_posQ, tpc_pt, tpc_pos.x(), si_pos.x())
       && window_dy.in_window(is_posQ, tpc_pt, tpc_pos.y(), si_pos.y()))
      {
        position_match = true;
      }
      if (!position_match)
      {
        continue;
      }

      bool phi_match = false;
      if (window_dphi.in_window(is_posQ, tpc_pt, tpc_phi, si_phi))
      {
        phi_match = true;
        // if phi fails, account for case where |tpc_phi-si_phi|>PI
      } else if (fabs(tpc_phi-si_phi)>M_PI) {
        auto tpc_phi_wrap = tpc_phi;
        if ((tpc_phi_wrap - si_phi) > M_PI) {
          tpc_phi_wrap -= 2*M_PI;
        } else {
          tpc_phi_wrap += 2*M_PI;
        }
        phi_match = window_dphi.in_window(is_posQ, tpc_pt, tpc_phi_wrap, si_phi);
      }
      if (!phi_match)
      {
        continue;
      }

      if (Verbosity() > 3)
      {
        cout << " testing for a match for TPC track " << tpcid << " with pT " << _tracklet_tpc->get_pt()
             << " and eta " << _tracklet_tpc->get_eta() << " with Si track " << siid << " with crossing " << _tracklet_si->get_crossing() << endl;
        cout << " tpc_phi " << tpc_phi << " si_phi " << si_phi << " dphi " << tpc_phi - si_phi << " phi search " << _phi_search_win << " tpc_eta " << tpc_eta
             << " si_eta " << si_eta << " deta " << tpc_eta - si_eta << " eta search " << _eta_search_win  << endl;
        std::cout << "      tpc x " << tpc_pos.x() << " si x " << si_pos.x() << " tpc y " << tpc_pos.y() << " si y " << si_pos.y() << " tpc_z " << tpc_pos.z() << " si z " << si_pos.z() << std::endl;
        std::cout << "      x search " << _x_search_win  << " y search " << _y_search_win << " z search " << _z_search_win << std::endl;
      }

      // got a match, add to the list
      // These stubs are matched in eta, phi, x and y already
      matched = true;
      tpc_matches.insert(std::make_pair(tpcid, siid));
      tpc_matched_set.insert(tpcid);

      if (Verbosity() > 1)
      {
        cout << " found a match for TPC track " << tpcid << " with Si track " << siid << endl;
        cout << "          tpc_phi " << tpc_phi << " si_phi " << si_phi << " phi_match " << phi_match
             << " tpc_eta " << tpc_eta << " si_eta " << si_eta << " eta_match " << eta_match << endl;
        std::cout << "      tpc x " << tpc_pos.x() << " si x " << si_pos.x() << " tpc y " << tpc_pos.y() << " si y " << si_pos.y() << " tpc_z " << tpc_pos.z() << " si z " << si_pos.z() << std::endl;
      }

      // temporary!
      if (_test_windows && Verbosity() > 1)
      {
        cout << " Try_silicon: crossing" << si_crossing <<  "  pt " << tpc_pt << " tpc_phi " << tpc_phi << " si_phi " << si_phi << " dphi " << tpc_phi - si_phi <<  "   si_q" << si_q << "   tpc_q" << tpc_q
             << " tpc_eta " << tpc_eta << " si_eta " << si_eta << " deta " << tpc_eta - si_eta << " tpc_x " << tpc_pos.x() << " tpc_y " << tpc_pos.y() << " tpc_z " << tpc_pos.z()
             << " dx " << tpc_pos.x() - si_pos.x() << " dy " << tpc_pos.y() - si_pos.y() << " dz " << tpc_pos.z() - si_pos.z()
			 << endl;
      }

    }
    // if no match found, keep tpc seed for fitting
    if (!matched)
    {
      if (Verbosity() > 1)
      {
        cout << "inserted unmatched tpc seed " << tpcid << endl;
      }
      tpc_unmatched_set.insert(tpcid);
    }
  }

  return;
}
void PHSiliconTpcTrackMatching::checkZMatches(
    std::multimap<unsigned int, unsigned int> &tpc_matches,
    std::multimap<unsigned int, unsigned int> &bad_map)
{
  // for _pp_mode=false, assume zero crossings for all track matches
  // for _pp_mode=true, do crossing correction on track position z according to side and vdrift
  // z matching criteria follows window_z
  // there is a dz threshold cut to avoid window_z blow up at low pT

  float vdrift = _tGeometry->get_drift_velocity();

  for (auto [tpcid, si_id] : tpc_matches)
  {
    TrackSeed *tpc_track = _track_map->get(tpcid);
    TrackSeed *si_track = _track_map_silicon->get(si_id);

    short int crossing = si_track->get_crossing();
    float tpc_pt, tpc_z, si_z;
    int tpc_q;
    if (_zero_field) {
      auto cluster_list_tpc = getTrackletClusterList(tpc_track);
      auto cluster_list_si = getTrackletClusterList(si_track);

      tpc_pt = std::get<3>(TrackFitUtils::zero_field_track_params(_tGeometry, _cluster_map, cluster_list_tpc));
      tpc_z = std::get<4>(TrackFitUtils::zero_field_track_params(_tGeometry, _cluster_map, cluster_list_tpc)).z();
      tpc_q = -100;

      si_z = std::get<4>(TrackFitUtils::zero_field_track_params(_tGeometry, _cluster_map, cluster_list_si)).z();
    } else {
      tpc_pt = fabs(1. / _tracklet_tpc->get_qOverR()) * (0.3 / 100.) * fieldstrength;
      tpc_z = TrackSeedHelper::get_z(tpc_track);
      tpc_q = _tracklet_tpc->get_charge();
      si_z = TrackSeedHelper::get_z(si_track);
    }

    // get TPC side from one of the TPC clusters
    std::vector<TrkrDefs::cluskey> temp_clusters = getTrackletClusterList(tpc_track);
    if(temp_clusters.size() == 0) { continue; }
    unsigned int this_side = TpcDefs::getSide(temp_clusters[0]);

    bool is_posQ = (tpc_q>0.);

    float z_mismatch = tpc_z - si_z;
    float tpc_z_corrected = _clusterCrossingCorrection.correctZ(tpc_z, this_side, crossing);
    float z_mismatch_corrected = tpc_z_corrected - si_z;

    bool z_match = false;
    if (_pp_mode)
    {
      if (crossing == SHRT_MAX)
      {
        if (Verbosity() > 2)
        {
          std::cout << " drop si_track " << si_id << " with eta " << si_track->get_eta() << " and z " << TrackSeedHelper::get_z(si_track) << " because crossing is undefined " << std::endl;
        }
        continue;
      }

      if (window_dz.in_window(is_posQ, tpc_pt, tpc_z_corrected, si_z) && (fabs(z_mismatch_corrected) < _crossing_deltaz_max))
      {
        z_match = true;
      }
      else if (fabs(z_mismatch_corrected) < _crossing_deltaz_min)
      {
	z_match = true;
      }
    }
    else
    {
      if (window_dz.in_window(is_posQ, tpc_pt, tpc_z, si_z) && (fabs(z_mismatch) < _crossing_deltaz_max))
      {
        z_match = true;
      }
      else if (fabs(z_mismatch) < _crossing_deltaz_min)
      {
	z_match = true;
      }
    }

    if (z_match)
    {
      if (Verbosity() > 1)
      {
        std::cout << "  Success:  crossing " << crossing << " tpcid " << tpcid << " si id " << si_id
                  << " tpc z " << tpc_z << " si z " << si_z << " z_mismatch " << z_mismatch << "tpc z corrected " << tpc_z_corrected
                  << " z_mismatch_corrected " << z_mismatch_corrected << " drift velocity " << vdrift << std::endl;
      }
    }
    else
    {
      if (Verbosity() > 1)
      {
        std::cout << "  FAILURE:  crossing " << crossing << " tpcid " << tpcid << " si id " << si_id
                  << " tpc z " << tpc_z << " si z " << si_z << " z_mismatch " << z_mismatch << "tpc_z_corrected " << tpc_z_corrected
                  << " z_mismatch_corrected " << z_mismatch_corrected << std::endl;
      }

      bad_map.insert(std::make_pair(tpcid, si_id));
    }
  }

  // remove bad entries from tpc_matches
  for (auto [tpcid, si_id] : bad_map)
  {
    // Have to iterate over tpc_matches and examine each pair to find the one matching bad_map
    // this logic works because we call the equal range on vertex_map for every id_pair
    // so we only delete one entry per equal range call
    auto ret = tpc_matches.equal_range(tpcid);
    for (auto it = ret.first; it != ret.second; ++it)
    {
      if (it->first == tpcid && it->second == si_id)
      {
        if (Verbosity() > 1)
        {
          std::cout << "                        erasing tpc_matches entry for tpcid " << tpcid << " si_id " << si_id << std::endl;
        }
        tpc_matches.erase(it);
        break;  // the iterator is no longer valid
      }
    }
  }

  return;
}

std::vector<TrkrDefs::cluskey> PHSiliconTpcTrackMatching::getTrackletClusterList(TrackSeed* tracklet)
{
  std::vector<TrkrDefs::cluskey> cluskey_vec;
  for (auto clusIter = tracklet->begin_cluster_keys();
       clusIter != tracklet->end_cluster_keys();
       ++clusIter)
  {
    auto key = *clusIter;
    auto cluster = _cluster_map->findCluster(key);
    if (!cluster)
    {
      if(Verbosity() > 1)
      {
        std::cout << PHWHERE << "Failed to get cluster with key " << key << std::endl;
      }
      continue;
    }

    /// Make a safety check for clusters that couldn't be attached to a surface
    auto surf = _tGeometry->maps().getSurface(key, cluster);
    if (!surf)
    {
      continue;
    }

    // drop some bad layers in the TPC completely
    unsigned int layer = TrkrDefs::getLayer(key);
    if (layer == 7 || layer == 22 || layer == 23 || layer == 38 || layer == 39)
    {
      continue;
    }

    cluskey_vec.push_back(key);
  }  // end loop over clusters for this track
  return cluskey_vec;
}
