#ifndef MAPPERSHH
#define MAPPERSHH

#include "cartridge.hh"
#include "byuuML_query.hh"

namespace ARS {
  namespace Mappers {
    std::unique_ptr<Cartridge> MakeBareChip(byuuML::cursor mapper_tag, std::unordered_map<std::string, std::unique_ptr<Memory> > memory);
    std::unique_ptr<Cartridge> MakeDevCart(byuuML::cursor mapper_tag, std::unordered_map<std::string, std::unique_ptr<Memory> > memory);
  }
}

#endif
