#ifndef DBG_H
#define DBG_H

#define DBG // enables serial printouts

#ifdef DBG

#define dbgf   Serial.printf
#define dbgl   Serial.println
#define dbgp   Serial.print

#else

#define dbgf   (void)
#define dbgl   (void)
#define dbgp   (void)

#endif

#endif