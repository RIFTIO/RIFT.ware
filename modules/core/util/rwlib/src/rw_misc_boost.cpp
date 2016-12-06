#include <boost/crc.hpp>

#include "rwlib.h"

//these two are identical:
//static boost::crc_ccitt_type crc_ccitt;
//static boost::crc_basic<16> crc_ccitt( 0x1021, 0xffff, 0, false, false );

/* This one is a "broken" version of the ccitt crc16 used by redis et
   al.  Same polynomial, just uses zero for initial state instead of ffff. */
static boost::crc_basic<16> crc_ccitt_exffff( 0x1021, 0/*"should" be ffff*/, 0, false, false );

uint16_t rw_crc16_ccitt_zero(const char *buf, int len) {
  crc_ccitt_exffff.reset();
  crc_ccitt_exffff.process_bytes(buf, len);
  return crc_ccitt_exffff.checksum();
}
