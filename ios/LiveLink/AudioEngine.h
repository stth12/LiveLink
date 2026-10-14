#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *LLReceiverRef;
LLReceiverRef ll_create(void);
void ll_destroy(LLReceiverRef);
int ll_start(LLReceiverRef, double);
void ll_stop(LLReceiverRef);
void ll_authorize(LLReceiverRef, uint64_t);
void ll_mode(LLReceiverRef, int);
int ll_packet(LLReceiverRef, const uint8_t *, int);
uint32_t ll_loss(LLReceiverRef);
uint32_t ll_underruns(LLReceiverRef);
uint32_t ll_occupancy(LLReceiverRef);
uint32_t ll_rate(LLReceiverRef);
float ll_peak(LLReceiverRef);
#ifdef __cplusplus
}
#endif
