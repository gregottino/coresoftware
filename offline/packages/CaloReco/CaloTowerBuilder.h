// Tell emacs that this is a C++ source
//  -*- C++ -*-.
#ifndef CALORECO_CALOTOWERBUILDER_H
#define CALORECO_CALOTOWERBUILDER_H

#include "CaloTowerDefs.h"
#include "CaloWaveformProcessing.h"

#include <cdbobjects/CDBTTree.h>  // for CDBTTree

#include <fun4all/SubsysReco.h>

#include <limits>
#include <string>

class CaloWaveformProcessing;
class PHCompositeNode;
class TowerInfoContainer;
class TowerInfoContainerv3;

class CaloTowerBuilder : public SubsysReco
{
 public:
  explicit CaloTowerBuilder(const std::string &name = "CaloTowerBuilder");
  ~CaloTowerBuilder() override;

  int InitRun(PHCompositeNode *topNode) override;
  int process_event(PHCompositeNode *topNode) override;

  void CreateNodeTree(PHCompositeNode *topNode);

  int process_data(PHCompositeNode *topNode, std::vector<std::vector<float>> &waveforms);
  

  void set_detector_type(CaloTowerDefs::DetectorSystem dettype)
  {
    m_dettype = dettype;
    return;
  }

  void set_builder_type(CaloTowerDefs::BuilderType buildertype)
  {
    m_buildertype = buildertype;
    return;
  }

  void set_nsamples(int _nsamples)
  {
    m_nsamples = _nsamples;
    return;
  }
  void set_dataflag(bool flag)
  {
    m_isdata = flag;
    return;
  }

  void set_processing_type(CaloWaveformProcessing::process processingtype)
  {
    _processingtype = processingtype;
  }

  void set_softwarezerosuppression(bool usezerosuppression, int softwarezerosuppression)
  {
    m_nsoftwarezerosuppression = softwarezerosuppression;
    m_bdosoftwarezerosuppression = usezerosuppression;
  }

  void set_inputNodePrefix(const std::string &name)
  {
    m_inputNodePrefix = name;
    return;
  }

  void set_outputNodePrefix(const std::string &name)
  {
    m_outputNodePrefix = name;
    return;
  }

  void set_offlineflag(const bool f = true)
  {
    m_UseOfflinePacketFlag = f;
  }
  void set_timeFitLim(float low,float high)
  {
    m_setTimeLim = true;
    m_timeLim_low = low;
    m_timeLim_high = high;
    return;
  }

  void set_bitFlipRecovery(bool dobitfliprecovery)
  {
    m_dobitfliprecovery = dobitfliprecovery;
  }

  void set_tbt_softwarezerosuppression(const std::string &url)
  {
    m_zsURL = url;
    m_dotbtszs = true;
    return;
  }

  void set_zs_fieldname(const std::string &fieldname)
  {
    m_zs_fieldname = fieldname;
    return;
  }
  
  CaloWaveformProcessing *get_WaveformProcessing() {return WaveformProcessing;}
  
 private:
  int process_sim();
  bool skipChannel(int ich, int pid);
  static bool isSZS(float time, float chi2);
  CaloWaveformProcessing *WaveformProcessing{nullptr};
  TowerInfoContainer *m_CaloInfoContainer{nullptr};      //! Calo info
  TowerInfoContainer *m_CalowaveformContainer{nullptr};  // waveform from simulation
  CDBTTree *cdbttree = nullptr;
  CDBTTree *cdbttree_sepd_map = nullptr;
  CDBTTree *cdbttree_tbt_zs = nullptr;

  bool m_isdata{true};
  bool m_bdosoftwarezerosuppression{false};
  bool m_UseOfflinePacketFlag{false};
  bool m_dotbtszs{false};
  bool m_PacketNodesFlag{false};
  int m_packet_low{std::numeric_limits<int>::min()};
  int m_packet_high{std::numeric_limits<int>::min()};
  int m_nsamples{16};
  int m_nchannels{192};
  int m_nzerosuppsamples{2};
  int m_nsoftwarezerosuppression{40};
  CaloTowerDefs::DetectorSystem m_dettype{CaloTowerDefs::CEMC};
  CaloTowerDefs::BuilderType m_buildertype{CaloTowerDefs::kPRDFTowerv1};
  CaloWaveformProcessing::process _processingtype{CaloWaveformProcessing::NONE};
  std::string m_detector{"CEMC"};
  std::string m_inputNodePrefix{"WAVEFORM_"};
  std::string m_outputNodePrefix{"TOWERS_"};
  std::string TowerNodeName;
  bool m_setTimeLim{false};
  float m_timeLim_low{-3.0};
  float m_timeLim_high{4.0};
  bool m_dobitfliprecovery{false};

  int m_saturation{16383};
  std::string calibdir;
  std::string m_fieldname;
  std::string m_calibName;
  std::string m_directURL;
  std::string m_zsURL;
  std::string m_zs_fieldname{"zs_threshold"};

};

#endif  // CALOTOWERBUILDER_H
