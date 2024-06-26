#ifndef CENTRALITY_CENTRALITYINFOV1_H
#define CENTRALITY_CENTRALITYINFOV1_H

#include "CentralityInfo.h"

#include <iostream>
#include <map>

class CentralityInfov1 : public CentralityInfo
{
 public:
  CentralityInfov1() = default;
  ~CentralityInfov1() override {}

  void identify(std::ostream &os = std::cout) const override;
  void Reset() override;
  int isValid() const override { return 1; }

  PHObject* CloneMe() const override { return new CentralityInfov1(*this); }
  void CopyTo(CentralityInfo *info) override;

  bool has_quantity(const PROP prop_id) const override;
  float get_quantity(const PROP prop_id) const override;
  void set_quantity(const PROP prop_id, const float value) override;

  bool has_centile(const PROP prop_id) const override;
  float get_centile(const PROP prop_id) const override;
  void set_centile(const PROP prop_id, const float value) override;

 private:
  std::map<int, float> _quantity_map;
  std::map<int, float> _centile_map;

  ClassDefOverride(CentralityInfov1, 1);
};

#endif
