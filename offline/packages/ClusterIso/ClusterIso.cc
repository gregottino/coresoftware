/*!
 * \file ClusterIso.cc
 * \brief
 * \author Francesco Vassalli <Francesco.Vassalli@colorado.edu>
 * \author Chase Smith <chsm5267@colorado.edu>
 * \version $Revision: 2 $
 * \date $Date: July 17th, 2018 $
 */

#include "ClusterIso.h"

#include <calobase/RawCluster.h>
#include <calobase/RawClusterContainer.h>
#include <calobase/RawClusterUtility.h>
#include <calobase/RawTowerGeom.h>
#include <calobase/RawTowerGeomContainer.h>
#include <calobase/TowerInfo.h>
#include <calobase/TowerInfoContainer.h>

#include <globalvertex/GlobalVertex.h>
#include <globalvertex/GlobalVertexMap.h>

#include <fun4all/Fun4AllBase.h>  // for Fun4AllBase::VERBOSITY_MORE
#include <fun4all/SubsysReco.h>

#include <phool/getClass.h>

#include <CLHEP/Vector/ThreeVector.h>

#include <iostream>
#include <map>
#include <utility>

/** \Brief Function to get correct tower eta
 *
 * Each calorimeter tower's eta is calculated using the vertex (0,0,0)
 * which is incorrect in many collisions. This function
 * uses geometry to find a given tower's eta using the correct vertex.
 */
double ClusterIso::getTowerEta(RawTowerGeom *tower_geom, double vx, double vy, double vz)
{
  float r;
  if (vx == 0 && vy == 0 && vz == 0)
  {
    r = tower_geom->get_eta();
  }
  else
  {
    double radius = sqrt((tower_geom->get_center_x() - vx) * (tower_geom->get_center_x() - vx) + (tower_geom->get_center_y() - vy) * (tower_geom->get_center_y() - vy));
    double theta = atan2(radius, tower_geom->get_center_z() - vz);
    r = -log(tan(theta / 2.));
  }
  return r;
}

/**
 * Contructor takes the argument of the class name, the minimum eT of the clusters which defaults to 0,
 * and the isolation cone size which defaults to 0.3.
 */
ClusterIso::ClusterIso(const std::string &kname, float eTCut = 0.0, int coneSize = 3, bool do_subtracted = true, bool do_unsubtracted = true)
  : SubsysReco(kname)
  , m_do_subtracted(do_subtracted)
  , m_do_unsubtracted(do_unsubtracted)
  , m_use_towerinfo(true)
{
  if (Verbosity() >= VERBOSITY_SOME)
  {
    std::cout << Name() << "::ClusterIso constructed" << '\n';
  }
  if (coneSize == 0 && Verbosity() >= VERBOSITY_QUIET)
  {
    std::cout << "WARNING in " << Name() << "ClusterIso:: cone size is zero" << '\n';
  }
  m_vx = m_vy = m_vz = 0;
  setConeSize(coneSize);
  seteTCut(eTCut);
  if (Verbosity() >= VERBOSITY_EVEN_MORE)
  {
    std::cout << Name() << "::ClusterIso::m_coneSize is:" << m_coneSize << '\n';
    std::cout << Name() << "::ClusterIso::m_eTCut is:" << m_eTCut << '\n';
  }
  if (!do_subtracted && !do_unsubtracted && Verbosity() >= VERBOSITY_QUIET)
  {
    std::cout << "WARNING in " << Name() << "ClusterIso:: all processes turned off doing nothing" << '\n';
  }
}

int ClusterIso::Init(PHCompositeNode * /*topNode*/)
{
  return 0;
}

/**
 * Set the minimum transverse energy required for a cluster to have its isolation calculated
 */
void ClusterIso::seteTCut(float eTCut)
{
  this->m_eTCut = eTCut;
}

/**
 * Set the size of isolation cone as integer multiple of 0.1, (i.e. 3 will use an R=0.3 cone)
 */
void ClusterIso::setConeSize(int coneSize)
{
  this->m_coneSize = coneSize / 10.0;
}

/**
 * Returns the minimum transverse energy required for a cluster to have its isolation calculated
 */
/*const*/ float ClusterIso::geteTCut()
{
  return m_eTCut;
}

/**
 * Returns size of isolation cone as integer multiple of 0.1 (i.e. 3 is an R=0.3 cone)
 */
/*const*/ int ClusterIso::getConeSize()
{
  return (int) m_coneSize * 10;
}

/**
 * Must be called to set the new vertex for the cluster
 */
/*const*/ CLHEP::Hep3Vector ClusterIso::getVertex()
{
  return CLHEP::Hep3Vector(m_vx, m_vy, m_vz);
}

/** \Brief Calculates isolation energy for all electromagnetic calorimeter clusters over the specified eT cut.
 *
 * For each cluster in the EMCal this iterates through all of the towers in each calorimeter,
 * if the towers are within the isolation cone their energy is added to the sum of isolation energy.
 * Finally subtract the cluster energy from the sum
 */
int ClusterIso::process_event(PHCompositeNode *topNode)
{
  if (Verbosity() >= VERBOSITY_MORE)
  {
    std::cout << Name() << "::ClusterIso::process_event" << '\n';
  }
  /**
   * If there event is embedded in Au+Au or another larger background we want to
   * get isolation energy from the towers with a subtracted background. This first section
   * looks at those towers instead of the original objects which include the background.
   * NOTE: that during the background event subtraction the EMCal towers are grouped
   * together so we have to use the inner HCal geometry.
   */

  std::string RawCemcClusterNodeName = "CLUSTER_CEMC";
  if (m_use_towerinfo)
  {
    RawCemcClusterNodeName = m_cluster_node_name;
  }

  if (m_do_subtracted)
  {
    {
      if (Verbosity() >= VERBOSITY_EVEN_MORE)
      {
        std::cout << Name() << "::ClusterIso starting subtracted calculation" << '\n';
      }
      // get EMCal towers
      TowerInfoContainer *towersEM3old = findNode::getClass<TowerInfoContainer>(topNode, "TOWERINFO_CALIB_CEMC_RETOWER_SUB1");
      if (towersEM3old == nullptr)
      {
        m_do_subtracted = false;
        if (Verbosity() >= VERBOSITY_SOME)
        {
          std::cout << "In " << Name() << "::ClusterIso WARNING substracted towers do not exist subtracted isolation cannot be preformed \n";
        }
      }
      if (Verbosity() >= VERBOSITY_MORE)
      {
        std::cout << Name() << "::ClusterIso::process_event: " << towersEM3old->size() << " TOWERINFO_CALIB_CEMC_RETOWER_SUB1 towers" << '\n';
      }

      // get InnerHCal towers
      TowerInfoContainer *towersIH3 = findNode::getClass<TowerInfoContainer>(topNode, "TOWERINFO_CALIB_HCALIN_SUB1");
      if (Verbosity() >= VERBOSITY_MORE)
      {
        std::cout << Name() << "::ClusterIso::process_event: " << towersIH3->size() << " TOWERINFO_CALIB_HCALIN_SUB1 towers" << '\n';
      }

      // get outerHCal towers
      TowerInfoContainer *towersOH3 = findNode::getClass<TowerInfoContainer>(topNode, "TOWERINFO_CALIB_HCALOUT_SUB1");
      if (Verbosity() >= VERBOSITY_MORE)
      {
        std::cout << Name() << "::ClusterIso::process_event: " << towersOH3->size() << " TOWERINFO_CALIB_HCALOUT_SUB1 towers" << std::endl;
      }

      // get geometry of calorimeter towers
      RawTowerGeomContainer *geomEM = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALIN");
      RawTowerGeomContainer *geomIH = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALIN");
      RawTowerGeomContainer *geomOH = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALOUT");

      {
        RawClusterContainer *clusters = findNode::getClass<RawClusterContainer>(topNode, RawCemcClusterNodeName.c_str());
        RawClusterContainer::ConstRange begin_end = clusters->getClusters();
        RawClusterContainer::ConstIterator rtiter;
        if (Verbosity() >= VERBOSITY_SOME)
        {
          std::cout << Name() << "::ClusterIso sees " << clusters->size() << " clusters " << '\n';
        }

        // vertexmap is used to get correct collision vertex
        GlobalVertexMap *vertexmap = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");
        m_vx = m_vy = m_vz = 0;
        if (vertexmap && !vertexmap->empty())
        {
          GlobalVertex *vertex = (vertexmap->begin()->second);
          m_vx = vertex->get_x();
          m_vy = vertex->get_y();
          m_vz = vertex->get_z();
          if (Verbosity() >= VERBOSITY_SOME)
          {
            std::cout << Name() << "::ClusterIso Event Vertex Calculated at x:" << m_vx << " y:" << m_vy << " z:" << m_vz << '\n';
          }
        }

        for (rtiter = begin_end.first; rtiter != begin_end.second; ++rtiter)
        {
          RawCluster *cluster = rtiter->second;

          CLHEP::Hep3Vector vertex(m_vx, m_vy, m_vz);
          CLHEP::Hep3Vector E_vec_cluster = RawClusterUtility::GetEVec(*cluster, vertex);
          double cluster_energy = E_vec_cluster.mag();
          double cluster_eta = E_vec_cluster.pseudoRapidity();
          double cluster_phi = E_vec_cluster.phi();
          double et = cluster_energy / cosh(cluster_eta);
          double isoEt = 0;

          if (et < m_eTCut)
          {
            continue;
          }  // skip if cluster is under eT cut

          // calculate EMCal tower contribution to isolation energy
          {
            unsigned int ntowers = towersEM3old->size();
            for (unsigned int channel = 0; channel < ntowers; channel++)
            {
              TowerInfo *tower = towersEM3old->get_tower_at_channel(channel);
              if (!IsAcceptableTower(tower))
              {
                continue;
              }
              unsigned int towerkey = towersEM3old->encode_key(channel);
              int ieta = towersEM3old->getTowerEtaBin(towerkey);
              int iphi = towersEM3old->getTowerPhiBin(towerkey);

              const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALIN, ieta, iphi);

              RawTowerGeom *tower_geom = geomEM->get_tower_geometry(key);
              double this_phi = tower_geom->get_phi();
              double this_eta = tower_geom->get_eta();
              if (deltaR(cluster_eta, this_eta, cluster_phi, this_phi) < m_coneSize)
              {
                isoEt += tower->get_energy() / cosh(this_eta);  // if tower is in cone, add energy
              }
            }
          }

          // calculate Inner HCal tower contribution to isolation energy
          {
            unsigned int ntowers = towersIH3->size();
            for (unsigned int channel = 0; channel < ntowers; channel++)
            {
              TowerInfo *tower = towersIH3->get_tower_at_channel(channel);
              if (!IsAcceptableTower(tower))
              {
                continue;
              }
              unsigned int towerkey = towersIH3->encode_key(channel);
              int ieta = towersIH3->getTowerEtaBin(towerkey);
              int iphi = towersIH3->getTowerPhiBin(towerkey);
              const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALIN, ieta, iphi);
              RawTowerGeom *tower_geom = geomIH->get_tower_geometry(key);
              double this_phi = tower_geom->get_phi();
              double this_eta = getTowerEta(tower_geom, m_vx, m_vy, m_vz);
              if (deltaR(cluster_eta, this_eta, cluster_phi, this_phi) < m_coneSize)
              {
                isoEt += tower->get_energy() / cosh(this_eta);  // if tower is in cone, add energy
              }
            }
          }

          // calculate Outer HCal tower contribution to isolation energy
          {
            unsigned int ntowers = towersOH3->size();
            for (unsigned int channel = 0; channel < ntowers; channel++)
            {
              TowerInfo *tower = towersOH3->get_tower_at_channel(channel);
              if (!IsAcceptableTower(tower))
              {
                continue;
              }
              unsigned int towerkey = towersOH3->encode_key(channel);
              int ieta = towersOH3->getTowerEtaBin(towerkey);
              int iphi = towersOH3->getTowerPhiBin(towerkey);
              const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALOUT, ieta, iphi);
              RawTowerGeom *tower_geom = geomOH->get_tower_geometry(key);
              double this_phi = tower_geom->get_phi();
              double this_eta = getTowerEta(tower_geom, m_vx, m_vy, m_vz);
              if (deltaR(cluster_eta, this_eta, cluster_phi, this_phi) < m_coneSize)
              {
                isoEt += tower->get_energy() / cosh(this_eta);  // if tower is in cone, add energy
              }
            }
          }

          isoEt -= et;  // Subtract cluster eT from isoET
          if (Verbosity() >= VERBOSITY_EVEN_MORE)
          {
            std::cout << Name() << "::ClusterIso iso_et for ";
            cluster->identify();
            std::cout << "=" << isoEt << '\n';
          }
          cluster->set_et_iso(isoEt, (int) 10 * m_coneSize, true, true);
        }
      }
    }
  }
  if (m_do_unsubtracted)
  {
    /**
     * This second section repeats the isolation calculation without any background subtraction
     */
    if (Verbosity() >= VERBOSITY_EVEN_MORE)
    {
      std::cout << Name() << "::ClusterIso starting unsubtracted calculation" << '\n';
    }
    {
      // get EMCal towers
      TowerInfoContainer *towersEM3old = findNode::getClass<TowerInfoContainer>(topNode, "TOWERINFO_CALIB_CEMC");
      if (Verbosity() >= VERBOSITY_MORE)
      {
        std::cout << "ClusterIso::process_event: " << towersEM3old->size() << " TOWERINFO_CALIB_CEMC towers" << '\n';
      }

      // get InnerHCal towers
      TowerInfoContainer *towersIH3 = findNode::getClass<TowerInfoContainer>(topNode, "TOWERINFO_CALIB_HCALIN");
      if (Verbosity() >= VERBOSITY_MORE)
      {
        std::cout << "ClusterIso::process_event: " << towersIH3->size() << " TOWERINFO_CALIB_HCALIN towers" << '\n';
      }

      // get outerHCal towers
      TowerInfoContainer *towersOH3 = findNode::getClass<TowerInfoContainer>(topNode, "TOWERINFO_CALIB_HCALOUT");
      if (Verbosity() >= VERBOSITY_MORE)
      {
        std::cout << "ClusterIso::process_event: " << towersOH3->size() << " TOWERINFO_CALIB_HCALOUT towers" << std::endl;
      }

      // get geometry of calorimeter towers
      RawTowerGeomContainer *geomEM = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_CEMC");
      RawTowerGeomContainer *geomIH = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALIN");
      RawTowerGeomContainer *geomOH = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALOUT");

      {
        RawClusterContainer *clusters = findNode::getClass<RawClusterContainer>(topNode, RawCemcClusterNodeName.c_str());
        RawClusterContainer::ConstRange begin_end = clusters->getClusters();
        RawClusterContainer::ConstIterator rtiter;
        if (Verbosity() >= VERBOSITY_SOME)
        {
          std::cout << " ClusterIso sees " << clusters->size() << " clusters " << '\n';
        }

        // vertexmap is used to get correct collision vertex
        GlobalVertexMap *vertexmap = findNode::getClass<GlobalVertexMap>(topNode, "GlobalVertexMap");
        m_vx = m_vy = m_vz = 0;
        if (vertexmap && !vertexmap->empty())
        {
          GlobalVertex *vertex = (vertexmap->begin()->second);
          m_vx = vertex->get_x();
          m_vy = vertex->get_y();
          m_vz = vertex->get_z();
          if (Verbosity() >= VERBOSITY_SOME)
          {
            std::cout << Name() << "ClusterIso Event Vertex Calculated at x:" << m_vx << " y:" << m_vy << " z:" << m_vz << '\n';
          }
        }

        for (rtiter = begin_end.first; rtiter != begin_end.second; ++rtiter)
        {
          RawCluster *cluster = rtiter->second;

          CLHEP::Hep3Vector vertex(m_vx, m_vy, m_vz);
          CLHEP::Hep3Vector E_vec_cluster = RawClusterUtility::GetEVec(*cluster, vertex);
          double cluster_energy = E_vec_cluster.mag();
          double cluster_eta = E_vec_cluster.pseudoRapidity();
          double cluster_phi = E_vec_cluster.phi();
          double et = cluster_energy / cosh(cluster_eta);
          double isoEt = 0;
          if (Verbosity() >= VERBOSITY_MAX)
          {
            std::cout << Name() << "::ClusterIso processing";
            cluster->identify();
            std::cout << '\n';
          }
          if (et < m_eTCut)
          {
            if (Verbosity() >= VERBOSITY_MAX)
            {
              std::cout << "\t does not pass eT cut" << '\n';
            }
            continue;
          }  // skip if cluster is below eT cut

          // calculate EMCal tower contribution to isolation energy
          {
            unsigned int ntowers = towersEM3old->size();
            for (unsigned int channel = 0; channel < ntowers; channel++)
            {
              TowerInfo *tower = towersEM3old->get_tower_at_channel(channel);
              if (!IsAcceptableTower(tower))
              {
                continue;
              }
              unsigned int towerkey = towersEM3old->encode_key(channel);
              int ieta = towersEM3old->getTowerEtaBin(towerkey);
              int iphi = towersEM3old->getTowerPhiBin(towerkey);
              const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::CEMC, ieta, iphi);
              RawTowerGeom *tower_geom = geomEM->get_tower_geometry(key);
              double this_phi = tower_geom->get_phi();
              double this_eta = getTowerEta(tower_geom, m_vx, m_vy, m_vz);
              if (deltaR(cluster_eta, this_eta, cluster_phi, this_phi) < m_coneSize)
              {
                isoEt += tower->get_energy() / cosh(this_eta);  // if tower is in cone, add energy
              }
            }
          }
          if (Verbosity() >= VERBOSITY_MAX)
          {
            std::cout << "\t after EMCal isoEt:" << isoEt << '\n';
          }
          // calculate Inner HCal tower contribution to isolation energy
          {
            unsigned int ntowers = towersIH3->size();
            for (unsigned int channel = 0; channel < ntowers; channel++)
            {
              TowerInfo *tower = towersIH3->get_tower_at_channel(channel);
              if (!IsAcceptableTower(tower))
              {
                continue;
              }
              unsigned int towerkey = towersIH3->encode_key(channel);
              int ieta = towersIH3->getTowerEtaBin(towerkey);
              int iphi = towersIH3->getTowerPhiBin(towerkey);
              const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALIN, ieta, iphi);
              RawTowerGeom *tower_geom = geomIH->get_tower_geometry(key);
              double this_phi = tower_geom->get_phi();
              double this_eta = getTowerEta(tower_geom, m_vx, m_vy, m_vz);
              if (deltaR(cluster_eta, this_eta, cluster_phi, this_phi) < m_coneSize)
              {
                isoEt += tower->get_energy() / cosh(this_eta);  // if tower is in cone, add energy
              }
            }
          }
          if (Verbosity() >= VERBOSITY_MAX)
          {
            std::cout << "\t after innerHCal isoEt:" << isoEt << '\n';
          }
          // calculate Outer HCal tower contribution to isolation energy
          {
            unsigned int ntowers = towersOH3->size();
            for (unsigned int channel = 0; channel < ntowers; channel++)
            {
              TowerInfo *tower = towersOH3->get_tower_at_channel(channel);
              if (!IsAcceptableTower(tower))
              {
                continue;
              }
              unsigned int towerkey = towersOH3->encode_key(channel);
              int ieta = towersOH3->getTowerEtaBin(towerkey);
              int iphi = towersOH3->getTowerPhiBin(towerkey);
              const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALOUT, ieta, iphi);
              RawTowerGeom *tower_geom = geomOH->get_tower_geometry(key);
              double this_phi = tower_geom->get_phi();
              double this_eta = getTowerEta(tower_geom, m_vx, m_vy, m_vz);
              if (deltaR(cluster_eta, this_eta, cluster_phi, this_phi) < m_coneSize)
              {
                isoEt += tower->get_energy() / cosh(this_eta);  // if tower is in cone, add energy
              }
            }
          }
          if (Verbosity() >= VERBOSITY_MAX)
          {
            std::cout << "\t after outerHCal isoEt:" << isoEt << '\n';
          }
          isoEt -= et;  // Subtract cluster eT from isoET
          if (Verbosity() >= VERBOSITY_EVEN_MORE)
          {
            std::cout << Name() << "::ClusterIso iso_et for ";
            cluster->identify();
            std::cout << "=" << isoEt << '\n';
          }
          cluster->set_et_iso(isoEt, (int) 10 * m_coneSize, false, true);
        }
      }
    }
  }
  return 0;
}

int ClusterIso::End(PHCompositeNode * /*topNode*/)
{
  return 0;
}

bool ClusterIso::IsAcceptableTower(TowerInfo *tower)
{
  if (!tower->get_isGood())
  {
    return false;
  }
  return true;
}
