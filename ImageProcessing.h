 
//**************************************************************************
//* WARNING: This file was automatically generated.  Any changes you make  *
//*          to this file will be lost if you generate the file again.     *
//**************************************************************************
#ifndef IMAGEPROCESSING_TASK_INCLUDE
#define IMAGEPROCESSING_TASK_INCLUDE
#include <nivision.h>
#include <nimachinevision.h>
#ifdef __cplusplus
	extern "C" {
#endif

/**
 * Processes image with following parameters:
 * 
 * Color Threshold
 * HSL
 * H: 0 - 255
 * S: 71 - 255
 * L: 184 - 255
 * 
 * Lookup Table
 * Equalize
 * 
 * Lookup Table
 * Reverse
 * 
 * Shape Detection
 * Shape: Lines
 * 
 */


int IVA_ProcessImage(Image *image);

#ifdef __cplusplus
	}
#endif

#endif // ifndef IMAGEPROCESSING_TASK_INCLUDE
