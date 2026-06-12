#ifndef __LOAD_CELL_PROBE_H
#define __LOAD_CELL_PROBE_H

#include <stdint.h> // uint8_t

struct load_cell_probe *load_cell_probe_oid_lookup(uint8_t oid);
int32_t load_cell_probe_get_last_raw_sample(struct load_cell_probe *lce);
void load_cell_probe_report_sample(struct load_cell_probe *lce
                        , int32_t sample);
void load_cell_probe_report_cell_sample(struct load_cell_probe *lce
                        , uint8_t cell_index, int32_t sample);
                        

#endif // load_cell_probe.h
