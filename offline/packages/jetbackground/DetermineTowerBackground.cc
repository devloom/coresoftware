#include "DetermineTowerBackground.h"

#include "TowerBackground.h"
#include "TowerBackgroundv1.h"

#include <calobase/RawTower.h>
#include <calobase/RawTowerContainer.h>
#include <calobase/RawTowerDefs.h>
#include <calobase/RawTowerGeom.h>
#include <calobase/RawTowerGeomContainer.h>
#include <calobase/TowerInfo.h>
#include <calobase/TowerInfoContainer.h>

#include <eventplaneinfo/Eventplaneinfo.h>
#include <eventplaneinfo/EventplaneinfoMap.h>

#include <jetbase/Jet.h>
#include <jetbase/JetContainer.h>

#include <g4main/PHG4Particle.h>
#include <g4main/PHG4TruthInfoContainer.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/SubsysReco.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHObject.h>
#include <phool/getClass.h>
#include <phool/phool.h>

#include <TLorentzVector.h>

// standard includes
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

DetermineTowerBackground::DetermineTowerBackground(const std::string &name)
  : SubsysReco(name)
{
  _UE.resize(3, std::vector<float>(1, 0));
}

int DetermineTowerBackground::InitRun(PHCompositeNode *topNode)
{
  return CreateNode(topNode);
}

int DetermineTowerBackground::process_event(PHCompositeNode *topNode)
{
  if (Verbosity() > 0)
  {
    std::cout << "DetermineTowerBackground::process_event: entering with do_flow = " << _do_flow << ", seed type = " << _seed_type << ", ";
    if (_seed_type == 0)
    {
      std::cout << " D = " << _seed_jet_D << std::endl;
    }
    else if (_seed_type == 1)
    {
      std::cout << " pT = " << _seed_jet_pt << std::endl;
    }
    else
    {
      std::cout << " UNDEFINED seed behavior! " << std::endl;
    }
  }

  // clear seed eta/phi positions
  _seed_eta.resize(0);
  _seed_phi.resize(0);

  // pull out the tower containers and geometry objects at the start
  RawTowerContainer *towersEM3 = nullptr;
  RawTowerContainer *towersIH3 = nullptr;
  RawTowerContainer *towersOH3 = nullptr;
  TowerInfoContainer *towerinfosEM3 = nullptr;
  TowerInfoContainer *towerinfosIH3 = nullptr;
  TowerInfoContainer *towerinfosOH3 = nullptr;
  if (m_use_towerinfo)
  {
    EMTowerName = m_towerNodePrefix + "_CEMC_RETOWER";
    IHTowerName = m_towerNodePrefix + "_HCALIN";
    OHTowerName = m_towerNodePrefix + "_HCALOUT";
    towerinfosEM3 = findNode::getClass<TowerInfoContainer>(topNode, EMTowerName);
    towerinfosIH3 = findNode::getClass<TowerInfoContainer>(topNode, IHTowerName);
    towerinfosOH3 = findNode::getClass<TowerInfoContainer>(topNode, OHTowerName);
    if (!towerinfosEM3)
    {
      std::cout << "DetermineTowerBackground::process_event: Cannot find node " << EMTowerName << std::endl;
      exit(1);
    }
    if (!towerinfosIH3)
    {
      std::cout << "DetermineTowerBackground::process_event: Cannot find node " << IHTowerName << std::endl;
      exit(1);
    }
    if (!towerinfosOH3)
    {
      std::cout << "DetermineTowerBackground::process_event: Cannot find node " << OHTowerName << std::endl;
      exit(1);
    }
  }
  else
  {
    towersEM3 = findNode::getClass<RawTowerContainer>(topNode, "TOWER_CALIB_CEMC_RETOWER");
    towersIH3 = findNode::getClass<RawTowerContainer>(topNode, "TOWER_CALIB_HCALIN");
    towersOH3 = findNode::getClass<RawTowerContainer>(topNode, "TOWER_CALIB_HCALOUT");

    if (Verbosity() > 0)
    {
      std::cout << "DetermineTowerBackground::process_event: " << towersEM3->size() << " TOWER_CALIB_CEMC_RETOWER towers" << std::endl;
      std::cout << "DetermineTowerBackground::process_event: " << towersIH3->size() << " TOWER_CALIB_HCALIN towers" << std::endl;
      std::cout << "DetermineTowerBackground::process_event: " << towersOH3->size() << " TOWER_CALIB_HCALOUT towers" << std::endl;
    }
  }

  RawTowerGeomContainer *geomIH = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALIN");
  RawTowerGeomContainer *geomOH = findNode::getClass<RawTowerGeomContainer>(topNode, "TOWERGEOM_HCALOUT");

  // seed type 0 is D > 3 R=0.2 jets run on retowerized CEMC
  if (_seed_type == 0)
  {
    JetContainer *reco2_jets;
    if (m_use_towerinfo)
    {
      reco2_jets = findNode::getClass<JetContainer>(topNode, "AntiKt_TowerInfo_HIRecoSeedsRaw_r02");
    }
    else
    {
      reco2_jets = findNode::getClass<JetContainer>(topNode, "AntiKt_Tower_HIRecoSeedsRaw_r02");
    }
    if (Verbosity() > 1)
    {
      std::cout << "DetermineTowerBackground::process_event: examining possible seeds (1st iteration) ... " << std::endl;
    }

    _index_SeedD = reco2_jets->property_index(Jet::PROPERTY::prop_SeedD);
    _index_SeedItr = reco2_jets->property_index(Jet::PROPERTY::prop_SeedItr);
    for (auto *this_jet : *reco2_jets)
    {
      float this_pt = this_jet->get_pt();
      float this_phi = this_jet->get_phi();
      float this_eta = this_jet->get_eta();

      if (this_jet->get_pt() < 5)
      {
        // mark that this jet was not selected as a seed (and did not have D determined)
        this_jet->set_property(_index_SeedD, 0);
        this_jet->set_property(_index_SeedItr, 0);

        continue;
      }

      if (Verbosity() > 2)
      {
        std::cout << "DetermineTowerBackground::process_event: possible seed jet with pt / eta / phi = " << this_pt << " / " << this_eta << " / " << this_phi << ", examining constituents..." << std::endl;
      }

      std::map<int, double> constituent_ETsum;

      for (const auto &comp : this_jet->get_comp_vec())
      {
        int comp_ieta = -1;
        int comp_iphi = -1;
        float comp_ET = 0;
        int comp_isBad = -99;

        RawTower *tower;
        TowerInfo *towerinfo;
        RawTowerGeom *tower_geom;

        if (m_use_towerinfo)
        {
          if (comp.first == 5 || comp.first == 26)
          {
            towerinfo = towerinfosIH3->get_tower_at_channel(comp.second);
            unsigned int towerkey = towerinfosIH3->encode_key(comp.second);
            comp_ieta = towerinfosIH3->getTowerEtaBin(towerkey);
            comp_iphi = towerinfosIH3->getTowerPhiBin(towerkey);
            const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALIN, comp_ieta, comp_iphi);
            tower_geom = geomIH->get_tower_geometry(key);
            comp_ET = towerinfo->get_energy() / cosh(tower_geom->get_eta());
            comp_isBad = towerinfo->get_isHot() || towerinfo->get_isNoCalib() || towerinfo->get_isNotInstr() || towerinfo->get_isBadChi2();
          }
          else if (comp.first == 7 || comp.first == 27)
          {
            towerinfo = towerinfosOH3->get_tower_at_channel(comp.second);
            unsigned int towerkey = towerinfosOH3->encode_key(comp.second);
            comp_ieta = towerinfosOH3->getTowerEtaBin(towerkey);
            comp_iphi = towerinfosOH3->getTowerPhiBin(towerkey);
            const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALOUT, comp_ieta, comp_iphi);
            tower_geom = geomOH->get_tower_geometry(key);
            comp_ET = towerinfo->get_energy() / cosh(tower_geom->get_eta());
            comp_isBad = towerinfo->get_isHot() || towerinfo->get_isNoCalib() || towerinfo->get_isNotInstr() || towerinfo->get_isBadChi2();
          }
          else if (comp.first == 13 || comp.first == 28)
          {
            towerinfo = towerinfosEM3->get_tower_at_channel(comp.second);
            unsigned int towerkey = towerinfosEM3->encode_key(comp.second);
            comp_ieta = towerinfosEM3->getTowerEtaBin(towerkey);
            comp_iphi = towerinfosEM3->getTowerPhiBin(towerkey);
            const RawTowerDefs::keytype key = RawTowerDefs::encode_towerid(RawTowerDefs::CalorimeterId::HCALIN, comp_ieta, comp_iphi);
            tower_geom = geomIH->get_tower_geometry(key);
            comp_ET = towerinfo->get_energy() / cosh(tower_geom->get_eta());
            comp_isBad = towerinfo->get_isHot() || towerinfo->get_isNoCalib() || towerinfo->get_isNotInstr() || towerinfo->get_isBadChi2();
          }
        }
        else
        {
          if (comp.first == 5)
          {
            tower = towersIH3->getTower(comp.second);
            tower_geom = geomIH->get_tower_geometry(tower->get_key());

            comp_ieta = geomIH->get_etabin(tower_geom->get_eta());
            comp_iphi = geomIH->get_phibin(tower_geom->get_phi());
            comp_ET = tower->get_energy() / cosh(tower_geom->get_eta());
          }
          else if (comp.first == 7)
          {
            tower = towersOH3->getTower(comp.second);
            tower_geom = geomOH->get_tower_geometry(tower->get_key());

            comp_ieta = geomIH->get_etabin(tower_geom->get_eta());
            comp_iphi = geomIH->get_phibin(tower_geom->get_phi());
            comp_ET = tower->get_energy() / cosh(tower_geom->get_eta());
          }
          else if (comp.first == 13)
          {
            tower = towersEM3->getTower(comp.second);
            tower_geom = geomIH->get_tower_geometry(tower->get_key());

            comp_ieta = geomIH->get_etabin(tower_geom->get_eta());
            comp_iphi = geomIH->get_phibin(tower_geom->get_phi());
            comp_ET = tower->get_energy() / cosh(tower_geom->get_eta());
          }
        }
        if (comp_isBad)
        {
          if (Verbosity() > 4)
          {
            std::cout << "DetermineTowerBackground::process_event: --> --> Skipping constituent in layer " << comp.first << " at ieta / iphi = " << comp_ieta << " / " << comp_iphi << "due to masking" << std::endl;
          }
          continue;
        }
        int comp_ikey = (1000 * comp_ieta) + comp_iphi;

        if (Verbosity() > 4)
        {
          std::cout << "DetermineTowerBackground::process_event: --> --> constituent in layer " << comp.first << " at ieta / iphi = " << comp_ieta << " / " << comp_iphi << ", filling map with key = " << comp_ikey << " and ET = " << comp_ET << std::endl;
        }

        constituent_ETsum[comp_ikey] += comp_ET;

        if (Verbosity() > 4)
        {
          std::cout << "DetermineTowerBackground::process_event: --> --> ET sum map at key = " << comp_ikey << " now has ET = " << constituent_ETsum[comp_ikey] << std::endl;
        }
      }

      // now iterate over constituent_ET sums to find maximum and mean
      float constituent_max_ET = 0;
      float constituent_sum_ET = 0;
      int nconstituents = 0;

      if (Verbosity() > 4)
      {
        std::cout << "DetermineTowerBackground::process_event: --> now iterating over map..." << std::endl;
      }
      for (auto &map_iter : constituent_ETsum)
      {
        if (Verbosity() > 4)
        {
          std::cout << "DetermineTowerBackground::process_event: --> --> map has key # " << map_iter.first << " and ET = " << map_iter.second << std::endl;
        }
        nconstituents++;
        constituent_sum_ET += map_iter.second;
        constituent_max_ET = std::max<double>(map_iter.second, constituent_max_ET);
      }

      float mean_constituent_ET = constituent_sum_ET / nconstituents;
      float seed_D = constituent_max_ET / mean_constituent_ET;

      // store D value as property for offline analysis / debugging
      this_jet->set_property(_index_SeedD, seed_D);

      if (Verbosity() > 3)
      {
        std::cout << "DetermineTowerBackground::process_event: --> jet has < ET > = " << constituent_sum_ET << " / " << nconstituents << " = " << mean_constituent_ET << ", max-ET = " << constituent_max_ET << ", and D = " << seed_D << std::endl;
      }

      if (seed_D > _seed_jet_D)
      {
        _seed_eta.push_back(this_eta);
        _seed_phi.push_back(this_phi);

        // set first iteration seed property
        this_jet->set_property(_index_SeedItr, 1.0);

        if (Verbosity() > 1)
        {
          std::cout << "DetermineTowerBackground::process_event: --> adding seed at eta / phi = " << this_eta << " / " << this_phi << " ( R=0.2 jet with pt = " << this_pt << ", D = " << seed_D << " ) " << std::endl;
        }
      }
      else
      {
        // mark that this jet was considered but not used as a seed
        this_jet->set_property(_index_SeedItr, 0.0);

        if (Verbosity() > 3)
        {
          std::cout << "DetermineTowerBackground::process_event: --> discarding potential seed at eta / phi = " << this_eta << " / " << this_phi << " ( R=0.2 jet with pt = " << this_pt << ", D = " << seed_D << " ) " << std::endl;
        }
      }
    }
  }

  // seed type 1 is the set of those jets above which, when their
  // kinematics are updated for the first background subtraction, have
  // pT > 20 GeV
  if (_seed_type == 1)
  {
    JetContainer *reco2_jets;
    if (m_use_towerinfo)
    {
      reco2_jets = findNode::getClass<JetContainer>(topNode, "AntiKt_TowerInfo_HIRecoSeedsSub_r02");
    }
    else
    {
      reco2_jets = findNode::getClass<JetContainer>(topNode, "AntiKt_Tower_HIRecoSeedsSub_r02");
    }
    if (Verbosity() > 1)
    {
      std::cout << "DetermineTowerBackground::process_event: examining possible seeds (2nd iteration) ... " << std::endl;
    }

    _index_SeedD = reco2_jets->property_index(Jet::PROPERTY::prop_SeedD);
    _index_SeedItr = reco2_jets->property_index(Jet::PROPERTY::prop_SeedItr);
    for (auto *this_jet : *reco2_jets)
    {
      float this_pt = this_jet->get_pt();
      float this_phi = this_jet->get_phi();
      float this_eta = this_jet->get_eta();

      if (this_jet->get_pt() < _seed_jet_pt)
      {
        // mark that this jet was considered but not used as a seed
        this_jet->set_property(_index_SeedItr, 0.0);

        continue;
      }

      _seed_eta.push_back(this_eta);
      _seed_phi.push_back(this_phi);

      // set second iteration seed property
      this_jet->set_property(_index_SeedItr, 2.0);

      if (Verbosity() > 1)
      {
        std::cout << "DetermineTowerBackground::process_event: --> adding seed at eta / phi = " << this_eta << " / " << this_phi << " ( R=0.2 jet with pt = " << this_pt << " ) " << std::endl;
      }
    }
  }

  // get the binning from the geometry (different for 1D vs 2D...)
  if (_HCAL_NETA < 0)
  {
    _HCAL_NETA = geomIH->get_etabins();
    _HCAL_NPHI = geomIH->get_phibins();

    // resize UE density and energy vectors
    _UE[0].resize(_HCAL_NETA, 0);
    _UE[1].resize(_HCAL_NETA, 0);
    _UE[2].resize(_HCAL_NETA, 0);

    _EMCAL_E.resize(_HCAL_NETA, std::vector<float>(_HCAL_NPHI, 0));
    _IHCAL_E.resize(_HCAL_NETA, std::vector<float>(_HCAL_NPHI, 0));
    _OHCAL_E.resize(_HCAL_NETA, std::vector<float>(_HCAL_NPHI, 0));

    _EMCAL_ISBAD.resize(_HCAL_NETA, std::vector<int>(_HCAL_NPHI, 0));
    _IHCAL_ISBAD.resize(_HCAL_NETA, std::vector<int>(_HCAL_NPHI, 0));
    _OHCAL_ISBAD.resize(_HCAL_NETA, std::vector<int>(_HCAL_NPHI, 0));

    // for flow determination, build up a 1-D phi distribution of
    // energies from all layers summed together, populated only from eta
    // strips which do not have any excluded phi towers
    _FULLCALOFLOW_PHI_E.resize(_HCAL_NPHI, 0);
    _FULLCALOFLOW_PHI_VAL.resize(_HCAL_NPHI, 0);

    if (Verbosity() > 0)
    {
      std::cout << "DetermineTowerBackground::process_event: setting number of towers in eta / phi: " << _HCAL_NETA << " / " << _HCAL_NPHI << std::endl;
    }
  }

  // reset all maps map
  for (int ieta = 0; ieta < _HCAL_NETA; ieta++)
  {
    for (int iphi = 0; iphi < _HCAL_NPHI; iphi++)
    {
      _EMCAL_E[ieta][iphi] = 0;
      _IHCAL_E[ieta][iphi] = 0;
      _OHCAL_E[ieta][iphi] = 0;
      _EMCAL_ISBAD[ieta][iphi] = 0;
      _IHCAL_ISBAD[ieta][iphi] = 0;
      _OHCAL_ISBAD[ieta][iphi] = 0;
    }
  }

  for (int iphi = 0; iphi < _HCAL_NPHI; iphi++)
  {
    _FULLCALOFLOW_PHI_E[iphi] = 0;
    _FULLCALOFLOW_PHI_VAL[iphi] = 0;
  }

  if (m_use_towerinfo)
  {
    // iterate over EMCal towerinfos
    if (!towerinfosEM3 || !towerinfosIH3 || !towerinfosOH3)
    {
      std::cout << PHWHERE << "missing tower info object, doing nothing" << std::endl;
      return Fun4AllReturnCodes::ABORTRUN;
    }
    unsigned int nchannels_em = towerinfosEM3->size();
    for (unsigned int channel = 0; channel < nchannels_em; channel++)
    {
      unsigned int key = towerinfosEM3->encode_key(channel);
      int this_etabin = towerinfosEM3->getTowerEtaBin(key);
      int this_phibin = towerinfosEM3->getTowerPhiBin(key);
      TowerInfo *tower = towerinfosEM3->get_tower_at_channel(channel);
      float this_E = tower->get_energy();
      int this_isBad = tower->get_isHot() || tower->get_isNoCalib() || tower->get_isNotInstr() || tower->get_isBadChi2();
      _EMCAL_E[this_etabin][this_phibin] += this_E;
      _EMCAL_ISBAD[this_etabin][this_phibin] = this_isBad;
    }

    // iterate over IHCal towerinfos
    unsigned int nchannels_ih = towerinfosIH3->size();
    for (unsigned int channel = 0; channel < nchannels_ih; channel++)
    {
      unsigned int key = towerinfosIH3->encode_key(channel);
      int this_etabin = towerinfosIH3->getTowerEtaBin(key);
      int this_phibin = towerinfosIH3->getTowerPhiBin(key);
      TowerInfo *tower = towerinfosIH3->get_tower_at_channel(channel);
      float this_E = tower->get_energy();
      int this_isBad = tower->get_isHot() || tower->get_isNoCalib() || tower->get_isNotInstr() || tower->get_isBadChi2();
      _IHCAL_E[this_etabin][this_phibin] += this_E;
      _IHCAL_ISBAD[this_etabin][this_phibin] = this_isBad;
    }

    // iterate over OHCal towerinfos
    unsigned int nchannels_oh = towerinfosOH3->size();
    for (unsigned int channel = 0; channel < nchannels_oh; channel++)
    {
      unsigned int key = towerinfosOH3->encode_key(channel);
      int this_etabin = towerinfosOH3->getTowerEtaBin(key);
      int this_phibin = towerinfosOH3->getTowerPhiBin(key);
      TowerInfo *tower = towerinfosOH3->get_tower_at_channel(channel);
      float this_E = tower->get_energy();
      int this_isBad = tower->get_isHot() || tower->get_isNoCalib() || tower->get_isNotInstr() || tower->get_isBadChi2();
      _OHCAL_E[this_etabin][this_phibin] += this_E;
      _OHCAL_ISBAD[this_etabin][this_phibin] = this_isBad;
    }
  }
  else
  {
    // iterate over EMCal towers
    RawTowerContainer::ConstRange begin_end = towersEM3->getTowers();
    for (RawTowerContainer::ConstIterator rtiter = begin_end.first; rtiter != begin_end.second; ++rtiter)
    {
      RawTower *tower = rtiter->second;

      RawTowerGeom *tower_geom = geomIH->get_tower_geometry(tower->get_key());

      float this_eta = tower_geom->get_eta();
      float this_phi = tower_geom->get_phi();
      int this_etabin = geomIH->get_etabin(this_eta);
      int this_phibin = geomIH->get_phibin(this_phi);
      float this_E = tower->get_energy();

      _EMCAL_E[this_etabin][this_phibin] += this_E;

      if (Verbosity() > 2 && tower->get_energy() > 1)
      {
        std::cout << "DetermineTowerBackground::process_event: EMCal tower eta ( bin ) / phi ( bin ) / E = " << std::setprecision(6) << this_eta << " ( " << this_etabin << " ) / " << this_phi << " ( " << this_phibin << " ) / " << this_E << std::endl;
      }
    }

    // iterate over IHCal towers
    RawTowerContainer::ConstRange begin_end_IH = towersIH3->getTowers();
    for (RawTowerContainer::ConstIterator rtiter = begin_end_IH.first; rtiter != begin_end_IH.second; ++rtiter)
    {
      RawTower *tower = rtiter->second;
      RawTowerGeom *tower_geom = geomIH->get_tower_geometry(tower->get_key());

      float this_eta = tower_geom->get_eta();
      float this_phi = tower_geom->get_phi();
      int this_etabin = geomIH->get_etabin(this_eta);
      int this_phibin = geomIH->get_phibin(this_phi);
      float this_E = tower->get_energy();

      _IHCAL_E[this_etabin][this_phibin] += this_E;

      if (Verbosity() > 2 && tower->get_energy() > 1)
      {
        std::cout << "DetermineTowerBackground::process_event: IHCal tower at eta ( bin ) / phi ( bin ) / E = " << std::setprecision(6) << this_eta << " ( " << this_etabin << " ) / " << this_phi << " ( " << this_phibin << " ) / " << this_E << std::endl;
      }
    }

    // iterate over OHCal towers
    RawTowerContainer::ConstRange begin_end_OH = towersOH3->getTowers();
    for (RawTowerContainer::ConstIterator rtiter = begin_end_OH.first; rtiter != begin_end_OH.second; ++rtiter)
    {
      RawTower *tower = rtiter->second;
      RawTowerGeom *tower_geom = geomOH->get_tower_geometry(tower->get_key());

      float this_eta = tower_geom->get_eta();
      float this_phi = tower_geom->get_phi();
      int this_etabin = geomOH->get_etabin(this_eta);
      int this_phibin = geomOH->get_phibin(this_phi);
      float this_E = tower->get_energy();

      _OHCAL_E[this_etabin][this_phibin] += this_E;

      if (Verbosity() > 2 && tower->get_energy() > 1)
      {
        std::cout << "DetermineTowerBackground::process_event: OHCal tower at eta ( bin ) / phi ( bin ) / E = " << std::setprecision(6) << this_eta << " ( " << this_etabin << " ) / " << this_phi << " ( " << this_phibin << " ) / " << this_E << std::endl;
      }
    }
  }
  // first, calculate flow: Psi2 & v2, if enabled

  //_Psi2 is left as 0
  // since _v2 is derived from _Psi2, initializing _Psi2 to NAN will set _v2 = NAN
  //_is_flow_failure tags events where _Psi2 & _v2 are set to 0 because there are no strips for flow
  // and when sEPD _Psi2 has no determined _Psi2 because the event is outside +/- z = 60cm
  _Psi2 = 0;
  _v2 = 0;
  _nStrips = 0;
  _is_flow_failure = false;

  if (_do_flow == 0)
  {
    if (Verbosity() > 0)
    {
      std::cout << "DetermineTowerBackground::process_event: flow not enabled, setting Psi2 = " << _Psi2 << " ( " << _Psi2 / M_PI << " * pi ) , v2 = " << _v2 << std::endl;
    }
  }

  if (_do_flow >= 1)
  {
    // check for the case when every tower is excluded
    int nStripsAvailableForFlow = 0;
    int nStripsUnavailableForFlow = 0;

    for (int layer = 0; layer < 3; layer++)
    {
      int local_max_eta = _HCAL_NETA;
      int local_max_phi = _HCAL_NPHI;

      for (int eta = 0; eta < local_max_eta; eta++)
      {
        bool isAnyTowerExcluded = false;

        for (int phi = 0; phi < local_max_phi; phi++)
        {
          float this_eta = geomIH->get_etacenter(eta);
          float this_phi = geomIH->get_phicenter(phi);

          bool isExcluded = false;

          // if the tower is masked, exclude it
          // nobody can understand these nested ?s, which just makes this error prone and a maintenance headache
          //          int this_isBad = (layer == 0 ? _EMCAL_ISBAD[eta][phi] : (layer == 1 ? _IHCAL_ISBAD[eta][phi] : _OHCAL_ISBAD[eta][phi]));

          int this_isBad;
          if (layer == 0)
          {
            this_isBad = _EMCAL_ISBAD[eta][phi];
          }
          else if (layer == 1)
          {
            this_isBad = _IHCAL_ISBAD[eta][phi];
          }
          else
          {
            this_isBad = _OHCAL_ISBAD[eta][phi];
          }
          if (this_isBad)
          {
            isExcluded = true;
          }
          for (unsigned int iseed = 0; iseed < _seed_eta.size(); iseed++)
          {
            float deta = this_eta - _seed_eta[iseed];
            float dphi = this_phi - _seed_phi[iseed];
            if (dphi > M_PI)
            {
              dphi -= 2 * M_PI;
            }
            if (dphi < -M_PI)
            {
              dphi += 2 * M_PI;
            }
            float dR = sqrt(pow(deta, 2) + pow(dphi, 2));
            if (dR < 0.4)
            {
              isExcluded = true;
              if (Verbosity() > 10)
              {
                // please do not use these kind of nested ?'s - really hard to understand
                // float my_E = (layer == 0 ? _EMCAL_E[eta][phi] : (layer == 1 ? _IHCAL_E[eta][phi] : _OHCAL_E[eta][phi]));
                float my_E;
                if (layer == 0)
                {
                  my_E = _EMCAL_E[eta][phi];
                }
                else if (layer == 1)
                {
                  my_E = _IHCAL_E[eta][phi];
                }
                else
                {
                  my_E = _OHCAL_E[eta][phi];
                }
                std::cout << " setting excluded mark for tower with E / eta / phi = " << my_E << " / " << this_eta << " / " << this_phi << " from seed at eta / phi = " << _seed_eta[iseed] << " / " << _seed_phi[iseed] << std::endl;
              }
            }
          }

          // if even a single tower in this eta strip is excluded, we
          // can't use the strip for flow determination
          if (isExcluded)
          {
            isAnyTowerExcluded = true;
          }
        }  // close phi loop

        // if this eta strip can be used for flow determination, fill it now
        if (!isAnyTowerExcluded)
        {
          if (Verbosity() > 4)
          {
            std::cout << " strip at layer " << layer << ", eta " << eta << " has no excluded towers and can be used for flow determination " << std::endl;
          }
          nStripsAvailableForFlow++;

          for (int phi = 0; phi < local_max_phi; phi++)
          {
            float this_phi = geomIH->get_phicenter(phi);

            if (layer == 0)
            {
              _FULLCALOFLOW_PHI_E[phi] += _EMCAL_E[eta][phi];
            }
            if (layer == 1)
            {
              _FULLCALOFLOW_PHI_E[phi] += _IHCAL_E[eta][phi];
            }
            if (layer == 2)
            {
              _FULLCALOFLOW_PHI_E[phi] += _OHCAL_E[eta][phi];
            }

            _FULLCALOFLOW_PHI_VAL[phi] = this_phi;  // should really set this globally only one time
          }
        }
        else
        {
          if (Verbosity() > 4)
          {
            std::cout << " strip at layer " << layer << ", eta " << eta << " DOES have excluded towers and CANNOT be used for flow determination " << std::endl;
          }
          nStripsUnavailableForFlow++;
        }

      }  // close eta loop
    }  // close layer loop

    // flow determination

    float Q_x = 0;
    float Q_y = 0;
    float E = 0;

    if (Verbosity() > 0)
    {
      std::cout << "DetermineTowerBackground::process_event: # of strips (summed over layers) available / unavailable for flow determination: " << nStripsAvailableForFlow << " / " << nStripsUnavailableForFlow << std::endl;
    }

    if (nStripsAvailableForFlow > 0)
    {
      for (int iphi = 0; iphi < _HCAL_NPHI; iphi++)
      {
        E += _FULLCALOFLOW_PHI_E[iphi];
        Q_x += _FULLCALOFLOW_PHI_E[iphi] * cos(2 * _FULLCALOFLOW_PHI_VAL[iphi]);
        Q_y += _FULLCALOFLOW_PHI_E[iphi] * sin(2 * _FULLCALOFLOW_PHI_VAL[iphi]);
      }

      if (_do_flow == 1)
      {
        _Psi2 = std::atan2(Q_y, Q_x) / 2.0;
      }
      else if (_do_flow == 2)
      {
        PHG4TruthInfoContainer *truthinfo = findNode::getClass<PHG4TruthInfoContainer>(topNode, "G4TruthInfo");

        if (!truthinfo)
        {
          std::cout << "DetermineTowerBackground::process_event: FATAL , G4TruthInfo does not exist , cannot extract truth flow with do_flow = " << _do_flow << std::endl;
          return -1;
        }

        PHG4TruthInfoContainer::Range range = truthinfo->GetPrimaryParticleRange();

        float Hijing_Qx = 0;
        float Hijing_Qy = 0;

        for (PHG4TruthInfoContainer::ConstIterator iter = range.first; iter != range.second; ++iter)
        {
          PHG4Particle *g4particle = iter->second;

          if (truthinfo->isEmbeded(g4particle->get_track_id()) != 0)
          {
            continue;
          }

          TLorentzVector t;
          t.SetPxPyPzE(g4particle->get_px(), g4particle->get_py(), g4particle->get_pz(), g4particle->get_e());

          float truth_pt = t.Pt();
          if (truth_pt < 0.4)
          {
            continue;
          }
          float truth_eta = t.Eta();
          if (std::fabs(truth_eta) > 1.1)
          {
            continue;
          }
          float truth_phi = t.Phi();
          int truth_pid = g4particle->get_pid();

          if (Verbosity() > 10)
          {
            std::cout << "DetermineTowerBackground::process_event: determining truth flow, using particle w/ pt / eta / phi " << truth_pt << " / " << truth_eta << " / " << truth_phi << " , embed / PID = " << truthinfo->isEmbeded(g4particle->get_track_id()) << " / " << truth_pid << std::endl;
          }

          Hijing_Qx += truth_pt * std::cos(2 * truth_phi);
          Hijing_Qy += truth_pt * std::sin(2 * truth_phi);
        }

        _Psi2 = std::atan2(Hijing_Qy, Hijing_Qx) / 2.0;

        if (Verbosity() > 0)
        {
          std::cout << "DetermineTowerBackground::process_event: flow extracted from Hijing truth particles, setting Psi2 = " << _Psi2 << " ( " << _Psi2 / M_PI << " * pi ) " << std::endl;
        }
      }
      else if (_do_flow == 3)
      {
        // get event plane map
        EventplaneinfoMap *epmap = findNode::getClass<EventplaneinfoMap>(topNode, "EventplaneinfoMap");
        if (!epmap)
        {
          std::cout << "DetermineTowerBackground::process_event: FATAL, EventplaneinfoMap does not exist, cannot extract sEPD flow with do_flow = " << _do_flow << std::endl;
          exit(-1);
        }
        if (!(epmap->empty()))
        {
          auto *EPDNS = epmap->get(EventplaneinfoMap::sEPDNS);
          _Psi2 = EPDNS->get_shifted_psi(2);
        }
        else
        {
          _is_flow_failure = true;
        }
      }

      // determine v2 from calo regardless of origin of Psi2
      double sum_cos2dphi = 0;
      for (int iphi = 0; iphi < _HCAL_NPHI; iphi++)
      {
        sum_cos2dphi += _FULLCALOFLOW_PHI_E[iphi] * std::cos(2 * (_FULLCALOFLOW_PHI_VAL[iphi] - _Psi2));
      }

      _v2 = sum_cos2dphi / E;

      _nStrips = nStripsAvailableForFlow;
    }
    else
    {
      _Psi2 = 0;
      _v2 = 0;
      _nStrips = 0;
      _is_flow_failure = true;
      if (Verbosity() > 0)
      {
        std::cout << "DetermineTowerBackground::process_event: no full strips available for flow modulation, setting v2 and Psi = 0" << std::endl;
      }
    }

    if (Verbosity() > 0)
    {
      std::cout << "DetermineTowerBackground::process_event: unnormalized Q vector (Qx, Qy) = ( " << Q_x << ", " << Q_y << " ) with Sum E_i = " << E << std::endl;
      std::cout << "DetermineTowerBackground::process_event: Psi2 = " << _Psi2 << " ( " << _Psi2 / M_PI << " * pi " << (_do_flow == 2 ? "from Hijing " : "") << ") , v2 = " << _v2 << " ( using " << _nStrips << " ) " << std::endl;
    }
  }  // if do flow

  // now calculate energy densities...
  _nTowers = 0;  // store how many towers were used to determine bkg

  // starting with the EMCal first...
  for (int layer = 0; layer < 3; layer++)
  {
    int local_max_eta = _HCAL_NETA;
    int local_max_phi = _HCAL_NPHI;

    for (int eta = 0; eta < local_max_eta; eta++)
    {
      float total_E = 0;
      int total_tower = 0;

      for (int phi = 0; phi < local_max_phi; phi++)
      {
        float this_eta = geomIH->get_etacenter(eta);
        float this_phi = geomIH->get_phicenter(phi);

        bool isExcluded = false;

        // nobody can understand these nested ?s, which just makes this error prone and a maintenance headache
        //        float my_E = (layer == 0 ? _EMCAL_E[eta][phi] : (layer == 1 ? _IHCAL_E[eta][phi] : _OHCAL_E[eta][phi]));
        //        int this_isBad = (layer == 0 ? _EMCAL_ISBAD[eta][phi] : (layer == 1 ? _IHCAL_ISBAD[eta][phi] : _OHCAL_ISBAD[eta][phi]));
        //	please use the following if/else if/else
        float my_E;
        int this_isBad;
        if (layer == 0)
        {
          my_E = _EMCAL_E[eta][phi];
          this_isBad = _EMCAL_ISBAD[eta][phi];
        }
        else if (layer == 1)
        {
          my_E = _IHCAL_E[eta][phi];
          this_isBad = _IHCAL_ISBAD[eta][phi];
        }
        else
        {
          my_E = _OHCAL_E[eta][phi];
          this_isBad = _OHCAL_ISBAD[eta][phi];
        }
        // if the tower is masked (energy identically zero), exclude it
        if (this_isBad)
        {
          isExcluded = true;
          if (Verbosity() > 10)
          {
            std::cout << " tower in layer " << layer << " at eta / phi = " << this_eta << " / " << this_phi << " with E = " << my_E << " excluded due to masking" << std::endl;
          }
        }
        for (unsigned int iseed = 0; iseed < _seed_eta.size(); iseed++)
        {
          float deta = this_eta - _seed_eta[iseed];
          float dphi = this_phi - _seed_phi[iseed];
          if (dphi > M_PI)
          {
            dphi -= 2 * M_PI;
          }
          if (dphi < -M_PI)
          {
            dphi += 2 * M_PI;
          }
          float dR = sqrt(pow(deta, 2) + pow(dphi, 2));
          if (dR < 0.4)
          {
            isExcluded = true;
            if (Verbosity() > 10)
            {
              std::cout << " setting excluded mark from seed at eta / phi = " << _seed_eta[iseed] << " / " << _seed_phi[iseed] << std::endl;
            }
          }
        }

        if (!isExcluded)
        {
          if (layer == 0)
          {
            total_E += _EMCAL_E[eta][phi] / (1 + 2 * _v2 * std::cos(2 * (this_phi - _Psi2)));
          }
          if (layer == 1)
          {
            total_E += _IHCAL_E[eta][phi] / (1 + 2 * _v2 * std::cos(2 * (this_phi - _Psi2)));
          }
          if (layer == 2)
          {
            total_E += _OHCAL_E[eta][phi] / (1 + 2 * _v2 * std::cos(2 * (this_phi - _Psi2)));
          }
          total_tower++;  // towers in this eta range & layer
          _nTowers++;     // towers in entire calorimeter
        }
        else
        {
          if (Verbosity() > 10)
          {
            std::cout << " tower at eta / phi = " << this_eta << " / " << this_phi << " with E = " << total_E << " excluded due to seed " << std::endl;
          }
        }
      }

      std::pair<float, float> etabounds = geomIH->get_etabounds(eta);
      std::pair<float, float> phibounds = geomIH->get_phibounds(0);

      float deta = etabounds.second - etabounds.first;
      float dphi = phibounds.second - phibounds.first;
      float total_area = total_tower * deta * dphi;

      if ( total_tower > 0 )
      {
        _UE[layer].at(eta) = total_E / total_tower; // calculate the UE density
      } 
      else 
      {
        if (Verbosity() > 0)
        {
          std::cout << "DetermineTowerBackground::process_event: WARNING, no towers in layer " << layer << " / eta " << eta << ", setting UE density to 0" << std::endl;
        }
      
        _UE[layer].at(eta) = 0; // no towers, no UE
      }

      if (Verbosity() > 3)
      {
        std::cout << "DetermineTowerBackground::process_event: at layer / eta index ( eta range ) = " << layer << " / " << eta << " ( " << etabounds.first << " - " << etabounds.second << " ) , total E / total Ntower / total area = " << total_E << " / " << total_tower << " / " << total_area << " , UE per tower = " << total_E / total_tower << std::endl;
      }
    }
  }

  if (Verbosity() > 0)
  {
    for (int layer = 0; layer < 3; layer++)
    {
      std::cout << "DetermineTowerBackground::process_event: summary UE in layer " << layer << " : ";
      for (int eta = 0; eta < _HCAL_NETA; eta++)
      {
        std::cout << _UE[layer].at(eta) << " , ";
      }
      std::cout << std::endl;
    }
  }

  FillNode(topNode);

  if (Verbosity() > 0)
  {
    std::cout << "DetermineTowerBackground::process_event: exiting" << std::endl;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int DetermineTowerBackground::CreateNode(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);

  // Looking for the DST node
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    std::cout << PHWHERE << "DST Node missing, doing nothing." << std::endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  // store the jet background stuff under a sub-node directory
  PHCompositeNode *bkgNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "JETBACKGROUND"));
  if (!bkgNode)
  {
    bkgNode = new PHCompositeNode("JETBACKGROUND");
    dstNode->addNode(bkgNode);
  }

  // create the TowerBackground node...
  TowerBackground *towerbackground = findNode::getClass<TowerBackground>(topNode, _backgroundName);
  if (!towerbackground)
  {
    towerbackground = new TowerBackgroundv1();
    PHIODataNode<PHObject> *bkgDataNode = new PHIODataNode<PHObject>(towerbackground, _backgroundName, "PHObject");
    bkgNode->addNode(bkgDataNode);
  }
  else
  {
    std::cout << PHWHERE << "::ERROR - " << _backgroundName << " pre-exists, but should not" << std::endl;
    exit(-1);
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

void DetermineTowerBackground::FillNode(PHCompositeNode *topNode)
{
  TowerBackground *towerbackground = findNode::getClass<TowerBackground>(topNode, _backgroundName);
  if (!towerbackground)
  {
    std::cout << " ERROR -- can't find TowerBackground node after it should have been created" << std::endl;
    return;
  }

  towerbackground->set_UE(0, _UE[0]);
  towerbackground->set_UE(1, _UE[1]);
  towerbackground->set_UE(2, _UE[2]);

  towerbackground->set_v2(_v2);

  towerbackground->set_Psi2(_Psi2);

  towerbackground->set_nStripsUsedForFlow(_nStrips);

  towerbackground->set_nTowersUsedForBkg(_nTowers);

  towerbackground->set_flow_failure_flag(_is_flow_failure);

  return;
}
