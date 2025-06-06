// Tell emacs that this is a C++ source
//  -*- C++ -*-.
#ifndef EVENTPLANERECO_H
#define EVENTPLANERECO_H

//===========================================================
/// \author Ejiro Umaka
//===========================================================

#include <cdbobjects/CDBTTree.h>
#include <fun4all/SubsysReco.h>

#include <string> // for string
#include <vector> // for vector

class CDBHistos;
class TProfile2D;

class PHCompositeNode;

class EventPlaneReco : public SubsysReco {
public:
  EventPlaneReco(const std::string &name = "EventPlaneReco");
  ~EventPlaneReco() override = default;
  int InitRun(PHCompositeNode *topNode) override;
  int process_event(PHCompositeNode *topNode) override;
  int End(PHCompositeNode * /*topNode*/) override;

  void ResetMe();
  void set_sepd_epreco(bool sepdEpReco) { _sepdEpReco = sepdEpReco; }
  void set_default_calibfile(bool default_calib) { _default_calib = default_calib; }
  void set_mbd_epreco(bool mbdEpReco) { _mbdEpReco = mbdEpReco; }
  void set_sEPD_Mip_cut(const float e) { _epd_e = e; }
  void set_sEPD_Charge_cut(const float c) { _epd_charge_min = c; }
  void set_MBD_Min_Qcut(const float f) { _mbd_e = f; }
  void set_MBD_Vetex_cut(const float v) { _mbd_vertex_cut = v; }
  void set_Ep_orders(const unsigned int n) { m_MaxOrder = n; }

private:
  int CreateNodes(PHCompositeNode *topNode);

  unsigned int m_MaxOrder{3};
  int m_runNo{54912};
  std::string FileName;
  std::vector<std::vector<double>> south_q;
  std::vector<std::vector<double>> north_q;
  std::vector<std::vector<double>> northsouth_q;

  std::vector<std::pair<double, double>> south_Qvec;
  std::vector<std::pair<double, double>> north_Qvec;
  std::vector<std::pair<double, double>> northsouth_Qvec;

 // recentering utility
 std::vector<std::vector<double>> south_q_subtract;
 std::vector<std::vector<double>> north_q_subtract;
   
 // shifting utility
 std::vector<double> shift_north;
 std::vector<double> shift_south;
 std::vector<double> shift_northsouth;
 std::vector<double> tmp_south_psi;
 std::vector<double> tmp_north_psi;
 std::vector<double> tmp_northsouth_psi;

// recentering histograms
 TProfile2D *tprof_mean_cos_north_epd_input[6]{};
 TProfile2D *tprof_mean_sin_north_epd_input[6]{};
 TProfile2D *tprof_mean_cos_south_epd_input[6]{};
 TProfile2D *tprof_mean_sin_south_epd_input[6]{};
    
// shifting histograms
 const int _imax{12};
 TProfile2D *tprof_cos_north_epd_shift_input[6][12]{};
 TProfile2D *tprof_sin_north_epd_shift_input[6][12]{};
 TProfile2D *tprof_cos_south_epd_shift_input[6][12]{};
 TProfile2D *tprof_sin_south_epd_shift_input[6][12]{};
 TProfile2D *tprof_cos_northsouth_epd_shift_input[6][12]{};
 TProfile2D *tprof_sin_northsouth_epd_shift_input[6][12]{};

  bool _mbdEpReco{false};
  bool _sepdEpReco{false};
  bool _do_ep{false};
  bool _default_calib{true};
    
  float _nsum{0.0};
  float _ssum{0.0};
  float _mbdvtx{999.0};
  float _epd_charge_min{5.0};
  float _epd_e{10.0};
  float _mbd_e{10.0};
  float _mbdQ{0.0};
  float _mbd_vertex_cut{60.0};

 
};

#endif // EVENTPLANERECO_H
