// Tell emacs that this is a C++ source
//  -*- C++ -*-.
#ifndef EVENTPLANEINFOV1_H
#define EVENTPLANEINFOV1_H

#include "Eventplaneinfo.h"

#include <cstddef>  // for size_t
#include <iostream>
#include <limits>
#include <utility>  // for pair, make_pair
#include <vector>

class PHObject;

class Eventplaneinfov1 : public Eventplaneinfo
{
 public:
  Eventplaneinfov1() = default;
  ~Eventplaneinfov1() override = default;

  void identify(std::ostream& os = std::cout) const override;
  void Reset() override { *this = Eventplaneinfov1(); }
  PHObject* CloneMe() const override { return new Eventplaneinfov1(*this); }

  void set_qvector(std::vector<std::pair<double, double>> Qvec) override { mQvec = Qvec; }
  std::pair<double, double> get_qvector(int order) const override { return std::make_pair(mQvec[order - 1].first, mQvec[order - 1].second); }
  double GetPsi(const double Qx, const double Qy, const unsigned int order) const override;

 private:
  std::vector<std::pair<double, double>> mQvec;
  ClassDefOverride(Eventplaneinfov1, 1);
};

#endif
