/* This pseudo-C file is uploaded to Coverity for modeling. */
/* from ptp2 */

uint16_t htod16p (PTPParams *params, uint16_t var) { __coverity_tainted_data_return__(); }
uint16_t dtoh16p (PTPParams *params, uint16_t var) { __coverity_tainted_data_return__(); }
uint16_t htole16(uint16_t var) { __coverity_tainted_data_return__(); }
uint16_t htobe16(uint16_t var) { __coverity_tainted_data_return__(); }
void htod16ap (PTPParams *params, unsigned char *a, uint16_t val) { __coverity_tainted_data_argument__(a); }
void dtoh16ap (PTPParams *params, unsigned char *a, uint16_t val) { __coverity_tainted_data_argument__(a); }

uint32_t htod32p (PTPParams *params, uint32_t var) { __coverity_tainted_data_return__(); }
uint32_t dtoh32p (PTPParams *params, uint32_t var) { __coverity_tainted_data_return__(); }
uint32_t htole32 (uint32_t var) { __coverity_tainted_data_return__(); }
uint32_t htobe32 (uint32_t var) { __coverity_tainted_data_return__(); }
void htod32ap (PTPParams *params, unsigned char *a, uint32_t val) { __coverity_tainted_data_argument__(a); }
void dtoh32ap (PTPParams *params, unsigned char *a, uint32_t val) { __coverity_tainted_data_argument__(a); }

uint64_t dtoh64p (PTPParams *params, uint64_t var) { __coverity_tainted_data_return__(); }
uint64_t htod64p (PTPParams *params, uint64_t var) { __coverity_tainted_data_return__(); }

void htod64ap (PTPParams *params, unsigned char *a, uint64_t val) { __coverity_tainted_data_argument__(a); }
void dtoh64ap (PTPParams *params, unsigned char *a, uint64_t val) { __coverity_tainted_data_argument__(a); }

