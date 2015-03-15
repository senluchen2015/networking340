#include "table.h"

#if defined(GENERIC)
ostream & Table::Print(ostream &os) const
{
  // WRITE THIS
  os << "Table: \n";
  return os;
}
#endif

#if defined(LINKSTATE)

#endif

#if defined(DISTANCEVECTOR)

ostream & Table::Print(ostream &os) const
{
  // WRITE THIS
  os << "Table: \n";
  for (map<unsigned, map<unsigned, double> >::const_iterator it = table.begin(); it != table.end(); it++) {
    os << "cost via " << it->first << ": ";
    for (map<unsigned, double>::const_iterator innerit = it->second.begin(); innerit != it->second.end(); innerit++) {
      os << " to " << innerit->first << ": " << innerit->second;
    }
    os << "\n------------------------------------------------" << endl;
  }
  return os;
}

#endif
