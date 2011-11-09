/*
 * ======================================================================
 *             Texas Instruments OMAP(TM) Platform Software
 * (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
 *
 * Use of this software is controlled by the terms and conditions found
 * in the license agreement under which this software has been supplied.
 *
 *=======================================================================
 */
 
#ifndef NEON_ROTATION_INTERFACE_H
#define NEON_ROTATION_INTERFACE_H

typedef enum TI_NEON_ROTATION_VAL
{
		NEON_ROTNONE = 0,
    NEON_ROT90,
    NEON_ROT180,
    NEON_ROT270
} TI_NEON_ROTATION_VAL;

typedef struct NEON_FUNCTION_ARGS
{
		unsigned char* pIn;
		unsigned char* pOut;
		unsigned short width;
		unsigned short height;
		unsigned char rotate;
}NEON_FUNCTION_ARGS;
		    					
typedef int (*NEON_fpo)(NEON_FUNCTION_ARGS *);	    					

#endif //NEON_ROTATION_INTERFACE_H		    					
