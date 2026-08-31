#pragma once
#include "tools.h"
#include <cstring>

struct FetchUnitInput {
  Wire<1> needSquash;
  Wire<32> SquashPC;
  Wire<1> FetchValid;
  Wire<32> PredictPC;
  Wire<1> haltSignal;
};

struct FetchUnitOutput{
  Register<32> programCounter;
  Register<1> haltFetched;
};

struct FetchUnit : dark::Module<FetchUnitInput, FetchUnitOutput>{
  void work() override;
};
