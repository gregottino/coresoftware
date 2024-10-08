#include "G4TowerNtuple.h"

#include <calobase/RawTowerGeomContainer.h>
#include <calobase/TowerInfo.h>
#include <calobase/TowerInfoContainerv1.h>

#include <fun4all/Fun4AllHistoManager.h>
#include <fun4all/SubsysReco.h>  // for SubsysReco

#include <phool/getClass.h>

#include <TFile.h>
#include <TH1.h>
#include <TNtuple.h>

#include <cmath>     // for isfinite
#include <iostream>  // for operator<<, basic_ostream
#include <sstream>
#include <utility>  // for pair, make_pair

using namespace std;

G4TowerNtuple::G4TowerNtuple(const std::string &name, const std::string &filename)
  : SubsysReco(name)
  , nblocks(0)
  , hm(nullptr)
  , _filename(filename)
  , ntup(nullptr)
  , outfile(nullptr)
{
}

G4TowerNtuple::~G4TowerNtuple()
{
  //  delete ntup;
  delete hm;
}

int G4TowerNtuple::Init(PHCompositeNode * /*unused*/)
{
  hm = new Fun4AllHistoManager(Name());
  outfile = new TFile(_filename.c_str(), "RECREATE");
  ntup = new TNtuple("towerntup", "G4Towers", "detid:phi:eta:energy");
  //  ntup->SetDirectory(0);
  TH1 *h1 = new TH1F("energy1GeV", "energy 0-1GeV", 1000, 0, 1);
  eloss.push_back(h1);
  h1 = new TH1F("energy100GeV", "energy 0-100GeV", 1000, 0, 100);
  eloss.push_back(h1);
  return 0;
}

int G4TowerNtuple::process_event(PHCompositeNode *topNode)
{
  ostringstream nodename;
  ostringstream geonodename;
  set<string>::const_iterator iter;
  vector<TH1 *>::const_iterator eiter;
  for (iter = _node_postfix.begin(); iter != _node_postfix.end(); ++iter)
  {
    int detid = (_detid.find(*iter))->second;
    nodename.str("");
    nodename << "TOWERINFO_" << _tower_type[*iter];
    geonodename.str("");
    geonodename << "TOWERGEOM_" << *iter;
    RawTowerGeomContainer *towergeom = findNode::getClass<RawTowerGeomContainer>(topNode, geonodename.str());
    if (!towergeom)
    {
      cout << "no geometry node " << geonodename.str() << " for " << *iter << endl;
      continue;
    }

    TowerInfoContainer *towers = findNode::getClass<TowerInfoContainerv1>(topNode, nodename.str());
    if (towers)
    {
      double esum = 0;
      unsigned int nchannels = towers->size();
      for (unsigned int channel = 0; channel < nchannels; channel++)
      {
        TowerInfo *tower = towers->get_tower_at_channel(channel);
        double energy = tower->get_energy();
        if (!isfinite(energy))
        {
          cout << "invalid energy: " << energy << endl;
        }
        esum += energy;
        unsigned int towerkey = towers->encode_key(channel);
        int etabin = towers->getTowerEtaBin(towerkey);
        int phibin = towers->getTowerPhiBin(towerkey);

        // to search the map fewer times, cache the geom object until the layer changes
        double phi = towergeom->get_phicenter(phibin);
        double eta = towergeom->get_etacenter(etabin);
        ntup->Fill(detid,
                   phi,
                   eta,
                   energy);
      }
      for (eiter = eloss.begin(); eiter != eloss.end(); ++eiter)
      {
        (*eiter)->Fill(esum);
      }
    }
  }
  return 0;
}

int G4TowerNtuple::End(PHCompositeNode * /*topNode*/)
{
  outfile->cd();
  ntup->Write();
  outfile->Write();
  outfile->Close();
  delete outfile;
  hm->dumpHistos(_filename, "UPDATE");
  return 0;
}

void G4TowerNtuple::AddNode(const std::string &name, const std::string &twrtype, const int detid)
{
  ostringstream twrname;
  twrname << twrtype << "_" << name;
  _node_postfix.insert(name);
  _tower_type.insert(make_pair(name, twrname.str()));
  _detid[name] = detid;
  return;
}
