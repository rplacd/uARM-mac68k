#ifndef _PXA255_DSP_H_
#define _PXA255_DSP_H_

#include "../utilities/compiler_hacks.h"
#include "mem.h"
#include "CPU.h"



typedef struct{
	
	UInt64 acc0;
	
}Pxa255dsp;



Boolean pxa255dspInit(Pxa255dsp* dsp, ArmCpu* cpu);


#endif