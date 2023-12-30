/****************************************************************************/
// (C) 2009-2014 Renesas Electronics Corporation. All rights reserved.
//
// OpenGL ES 3.0 sample application.
//
// FILE     : RTMatrix.h
// CREATED  : 2009.03.09
// MODIFIED : -
// AUTHOR   : Renesas Electronics Corporation
// DEVICE   : 
// HISTORY  : 
//            2009.03.09
//            - Created release code.
//
/****************************************************************************/

#ifndef __RTMATRIX_H_
#define __RTMATRIX_H_

/****************************************************************************
 *  INCLUDES
 ****************************************************************************/

#include<stdlib.h>


/****************************************************************************
 *  DEFINES
 ****************************************************************************/

typedef struct _RTVECTOR3
{
    float x, y, z;
} RTVECTOR3;

typedef struct _RTMATRIX
{
    float f[16];
} RTMATRIX;


/****************************************************************************
 *  DECLARATIONS
 ****************************************************************************/

void RTMatrixIdentity( RTMATRIX& sMat );
void RTMatrixMultiply( RTMATRIX& mDist, const RTMATRIX& mSrc0, const RTMATRIX& mSrc1 );
void RTMatrixRotationX( RTMATRIX& mDist, float fAngle );
void RTMatrixRotationY( RTMATRIX& mDist, float fAngle );
void RTMatrixRotationZ( RTMATRIX& mDist, float fAngle );
void RTMatrixTranslation( RTMATRIX& mDist, float fX, float fY, float fZ );
void RTMatrixPerspective( RTMATRIX& mDist, float fFOVy, float fAspect, float fNear, float fFar );

void RTVec3Normalize( RTVECTOR3& mDist, const RTVECTOR3& mSrc );
void RTVec3CrossProduct( RTVECTOR3& mDist, const RTVECTOR3& mSrc0, const RTVECTOR3& mSrc1 );


/****************************************************************************
 *  GLOBALS
 ****************************************************************************/

float g_fIdentity[16] = 
{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};


/****************************************************************************
 *  MATRIX FUNCTIONS.
 ****************************************************************************/

void RTMatrixIdentity( RTMATRIX& sMat )
{
    memcpy( (float*)sMat.f, (float*)g_fIdentity, sizeof(float)*16 );
}

void RTMatrixMultiply( RTMATRIX& mDist, const RTMATRIX& mSrc0, const RTMATRIX& mSrc1 )
{
    RTMATRIX sRes;

    sRes.f[ 0] = mSrc0.f[ 0]*mSrc1.f[ 0] + mSrc0.f[ 1]*mSrc1.f[ 4] + mSrc0.f[ 2]*mSrc1.f[ 8] + mSrc0.f[ 3]*mSrc1.f[12];
    sRes.f[ 1] = mSrc0.f[ 0]*mSrc1.f[ 1] + mSrc0.f[ 1]*mSrc1.f[ 5] + mSrc0.f[ 2]*mSrc1.f[ 9] + mSrc0.f[ 3]*mSrc1.f[13];
    sRes.f[ 2] = mSrc0.f[ 0]*mSrc1.f[ 2] + mSrc0.f[ 1]*mSrc1.f[ 6] + mSrc0.f[ 2]*mSrc1.f[10] + mSrc0.f[ 3]*mSrc1.f[14];
    sRes.f[ 3] = mSrc0.f[ 0]*mSrc1.f[ 3] + mSrc0.f[ 1]*mSrc1.f[ 7] + mSrc0.f[ 2]*mSrc1.f[11] + mSrc0.f[ 3]*mSrc1.f[15];

    sRes.f[ 4] = mSrc0.f[ 4]*mSrc1.f[ 0] + mSrc0.f[ 5]*mSrc1.f[ 4] + mSrc0.f[ 6]*mSrc1.f[ 8] + mSrc0.f[ 7]*mSrc1.f[12];
    sRes.f[ 5] = mSrc0.f[ 4]*mSrc1.f[ 1] + mSrc0.f[ 5]*mSrc1.f[ 5] + mSrc0.f[ 6]*mSrc1.f[ 9] + mSrc0.f[ 7]*mSrc1.f[13];
    sRes.f[ 6] = mSrc0.f[ 4]*mSrc1.f[ 2] + mSrc0.f[ 5]*mSrc1.f[ 6] + mSrc0.f[ 6]*mSrc1.f[10] + mSrc0.f[ 7]*mSrc1.f[14];
    sRes.f[ 7] = mSrc0.f[ 4]*mSrc1.f[ 3] + mSrc0.f[ 5]*mSrc1.f[ 7] + mSrc0.f[ 6]*mSrc1.f[11] + mSrc0.f[ 7]*mSrc1.f[15];

    sRes.f[ 8] = mSrc0.f[ 8]*mSrc1.f[ 0] + mSrc0.f[ 9]*mSrc1.f[ 4] + mSrc0.f[10]*mSrc1.f[ 8] + mSrc0.f[11]*mSrc1.f[12];
    sRes.f[ 9] = mSrc0.f[ 8]*mSrc1.f[ 1] + mSrc0.f[ 9]*mSrc1.f[ 5] + mSrc0.f[10]*mSrc1.f[ 9] + mSrc0.f[11]*mSrc1.f[13];
    sRes.f[10] = mSrc0.f[ 8]*mSrc1.f[ 2] + mSrc0.f[ 9]*mSrc1.f[ 6] + mSrc0.f[10]*mSrc1.f[10] + mSrc0.f[11]*mSrc1.f[14];
    sRes.f[11] = mSrc0.f[ 8]*mSrc1.f[ 3] + mSrc0.f[ 9]*mSrc1.f[ 7] + mSrc0.f[10]*mSrc1.f[11] + mSrc0.f[11]*mSrc1.f[15];

    sRes.f[12] = mSrc0.f[12]*mSrc1.f[ 0] + mSrc0.f[13]*mSrc1.f[ 4] + mSrc0.f[14]*mSrc1.f[ 8] + mSrc0.f[15]*mSrc1.f[12];
    sRes.f[13] = mSrc0.f[12]*mSrc1.f[ 1] + mSrc0.f[13]*mSrc1.f[ 5] + mSrc0.f[14]*mSrc1.f[ 9] + mSrc0.f[15]*mSrc1.f[13];
    sRes.f[14] = mSrc0.f[12]*mSrc1.f[ 2] + mSrc0.f[13]*mSrc1.f[ 6] + mSrc0.f[14]*mSrc1.f[10] + mSrc0.f[15]*mSrc1.f[14];
    sRes.f[15] = mSrc0.f[12]*mSrc1.f[ 3] + mSrc0.f[13]*mSrc1.f[ 7] + mSrc0.f[14]*mSrc1.f[11] + mSrc0.f[15]*mSrc1.f[15];

    mDist = sRes;
}

void RTMatrixRotationX( RTMATRIX& mDist, float fAngle )
{
    float fCos, fSin;

    fCos = (float)cos( fAngle );
    fSin = (float)sin( fAngle );

    mDist.f[ 0] = 1.0f; mDist.f[ 4] = 0.0f ; mDist.f[ 8] = 0.0f; mDist.f[12] = 0.0f;
    mDist.f[ 1] = 0.0f; mDist.f[ 5] = fCos ; mDist.f[ 9] = fSin; mDist.f[13] = 0.0f;
    mDist.f[ 2] = 0.0f; mDist.f[ 6] = -fSin; mDist.f[10] = fCos; mDist.f[14] = 0.0f;
    mDist.f[ 3] = 0.0f; mDist.f[ 7] = 0.0f ; mDist.f[11] = 0.0f; mDist.f[15] = 1.0f;
}

void RTMatrixRotationY( RTMATRIX& mDist, float fAngle )
{
    float fCos, fSin;

    fCos = (float)cos( fAngle );
    fSin = (float)sin( fAngle );

    mDist.f[ 0] = fCos; mDist.f[ 4] = 0.0f; mDist.f[ 8] = -fSin; mDist.f[12] = 0.0f;
    mDist.f[ 1] = 0.0f; mDist.f[ 5] = 1.0f; mDist.f[ 9] = 0.0f ; mDist.f[13] = 0.0f;
    mDist.f[ 2] = fSin; mDist.f[ 6] = 0.0f; mDist.f[10] = fCos ; mDist.f[14] = 0.0f;
    mDist.f[ 3] = 0.0f; mDist.f[ 7] = 0.0f; mDist.f[11] = 0.0f ; mDist.f[15] = 1.0f;
}

void RTMatrixRotationZ( RTMATRIX& mDist, float fAngle )
{
    float fCos, fSin;

    fCos = (float)cos( fAngle );
    fSin = (float)sin( fAngle );

    mDist.f[ 0] = fCos; mDist.f[ 4] = fSin; mDist.f[ 8] = 0.0f ; mDist.f[12] = 0.0f;
    mDist.f[ 1] =-fSin; mDist.f[ 5] = fCos; mDist.f[ 9] = 0.0f ; mDist.f[13] = 0.0f;
    mDist.f[ 2] = 0.0f; mDist.f[ 6] = 0.0f; mDist.f[10] = 1.0f ; mDist.f[14] = 0.0f;
    mDist.f[ 3] = 0.0f; mDist.f[ 7] = 0.0f; mDist.f[11] = 0.0f ; mDist.f[15] = 1.0f;
}

void RTMatrixTranslation( RTMATRIX& mDist, float fX, float fY, float fZ )
{
	mDist.f[ 0]=1.0f; mDist.f[ 4]=0.0f;	mDist.f[ 8]=0.0f; mDist.f[12]=fX;
	mDist.f[ 1]=0.0f; mDist.f[ 5]=1.0f;	mDist.f[ 9]=0.0f; mDist.f[13]=fY;
	mDist.f[ 2]=0.0f; mDist.f[ 6]=0.0f;	mDist.f[10]=1.0f; mDist.f[14]=fZ;
	mDist.f[ 3]=0.0f; mDist.f[ 7]=0.0f;	mDist.f[11]=0.0f; mDist.f[15]=1.0f;
}

void RTMatrixPerspective( RTMATRIX& mDist, float fFOVy, float fAspect, float fNear, float fFar )
{
	float f, n;

	f = 1.0f / (float)tan(fFOVy * 0.5f);
	n = 1.0f / (fFar - fNear);

	mDist.f[ 0] = f / fAspect;
	mDist.f[ 1] = 0;
	mDist.f[ 2] = 0;
	mDist.f[ 3] = 0;

	mDist.f[ 4] = 0;
	mDist.f[ 5] = f;
	mDist.f[ 6] = 0;
	mDist.f[ 7] = 0;

	mDist.f[ 8] = 0;
	mDist.f[ 9] = 0;
	mDist.f[10] = fFar * n;
	mDist.f[11] = 1;

	mDist.f[12] = 0;
	mDist.f[13] = 0;
	mDist.f[14] = -fFar * fNear * n;
	mDist.f[15] = 0;

}

void RTMatrixLookAt( RTMATRIX& mDist, const RTVECTOR3& vEye, const RTVECTOR3 &vAt, const RTVECTOR3& vUp )
{
	RTVECTOR3 f, vUpActual, s, u;
	RTMATRIX  t;

	f.x = vEye.x - vAt.x;
	f.y = vEye.y - vAt.y;
	f.z = vEye.z - vAt.z;

	RTVec3Normalize( f, f );
	RTVec3Normalize( vUpActual, vUp );
	RTVec3CrossProduct( s, f, vUpActual );
	RTVec3CrossProduct( u, s, f );

	mDist.f[ 0] = s.x;
	mDist.f[ 1] = u.x;
	mDist.f[ 2] = -f.x;
	mDist.f[ 3] = 0;

	mDist.f[ 4] = s.y;
	mDist.f[ 5] = u.y;
	mDist.f[ 6] = -f.y;
	mDist.f[ 7] = 0;

	mDist.f[ 8] = s.z;
	mDist.f[ 9] = u.z;
	mDist.f[10] = -f.z;
	mDist.f[11] = 0;

	mDist.f[12] = 0;
	mDist.f[13] = 0;
	mDist.f[14] = 0;
	mDist.f[15] = 1;

	RTMatrixTranslation( t, -vEye.x, -vEye.y, -vEye.z );
	RTMatrixMultiply( mDist, t, mDist );
}


/****************************************************************************
 *  VECTOR FUNCTIONS.
 ****************************************************************************/

void RTVec3Normalize( RTVECTOR3& vDist, const RTVECTOR3& vSrc )
{
	float f, fNorm;

	fNorm = vSrc.x * vSrc.x + vSrc.y * vSrc.y + vSrc.z * vSrc.z;
	f = (float)( 1.0 / sqrt( (double)fNorm ) );

	vDist.x = vSrc.x * f;
	vDist.y = vSrc.y * f;
	vDist.z = vSrc.z * f;
}

void RTVec3CrossProduct( RTVECTOR3& vDist, const RTVECTOR3& vSrc0, const RTVECTOR3& vSrc1 )
{
    RTVECTOR3 vRes;

    vRes.x = vSrc0.y * vSrc1.z - vSrc0.z * vSrc1.y;
    vRes.y = vSrc0.z * vSrc1.x - vSrc0.x * vSrc1.z;
    vRes.z = vSrc0.x * vSrc1.y - vSrc0.y * vSrc1.x;

	vDist = vRes;
}

#endif
