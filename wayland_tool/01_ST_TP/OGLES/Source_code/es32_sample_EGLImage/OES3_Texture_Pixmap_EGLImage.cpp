/****************************************************************************/
// (C) 2009-2018 Renesas Electronics Corporation. All rights reserved.
//
// OpenGL ES 3.2 sample application
//
// FILE     : OES3_Texture_Pixmap_EGLImage
// CREATED  : 2014.05.28
// MODIFIED : 2018.03.08
// AUTHOR   : Renesas Electronics Corporation
// DEVICE   : 
// HISTORY  : 
//            2014.05.28
//            - Created release code.
//            2016.03.10
//            - Support OpenGL ES 3.1.
//            2018.03.08
//            - Support OpenGL ES 3.2.
/****************************************************************************/
/****************************************************************************
 *  INCLUDES
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <pthread.h>

#include  <sys/time.h>
#include  <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include "drm_fourcc.h"
#include "libkms.h"

#ifndef NULL
#define NULL (void *)0
#endif /* NULL */

#include "GLES3/gl32.h"
#include "GLES3/gl2ext.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "EGL/eglext_REL.h"

#include "vertices.h"       // vertex data
#include "texture.h"        // texture data
#include "RTMatrix.h"

/****************************************************************************
 *  DECLARATIONS
 ****************************************************************************/
void InitApplicationForPixmap(void);
void RenderToPixmap(void);
void CreateTexture(void);
void ChangeCurrent( GLuint );
void InitShaderForPixmap(void);
void SetupGLSLUniformForPixmap(void);
void DrawIndexStrips(void);
void *RenderThread(void *lpParameter);
void AppTerminate(void);

void InitApplicationForWindow(void);
void RenderToWindow(void);
void CreateTextureEGLImage( void );
void DrawSqure( void );
void InitShaderForWindow(void);
void SetupGLSLUniformForWindow( void );

/* EGL image function */
typedef EGLImageKHR (*EGLCREATEIMAGEKHR)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, EGLint* attr_list);
typedef EGLBoolean (*EGLDESTROYIMAGEKHR)(EGLDisplay dpy, EGLImageKHR image);
static EGLCREATEIMAGEKHR eglCreateImageKHR;
static EGLDESTROYIMAGEKHR eglDestroyImageKHR;

/* EGL image to texture image function */
typedef void (*GLEGLIMAGETARGETRENDERBUFFERSTORAGEOES)(GLenum target, GLeglImageOES image);
static GLEGLIMAGETARGETRENDERBUFFERSTORAGEOES glEGLImageTargetTexture2D;

/****************************************************************************
 *  GLOBALS
 ****************************************************************************/

int		g_nScreenWidth;
int		g_nScreenHeight;

pthread_t	g_hRenderThread;
int			g_hThreadTerminateEvent;

EGLDisplay	dpy;
EGLSurface	windowsurface;
EGLSurface	pixmapsurface;
EGLContext	context;

EGLImageKHR	eglimage = EGL_NO_IMAGE_KHR;

GLuint		g_uiProgramObject;
GLuint		g_hTexture;
GLuint		g_hTexture_eglimage;
GLuint		g_uiFragShader, g_uiVertShader;

const char	c_szVertShaderBinFile[]  = "./OES3_VertShaderSample.vsc";  // VertexShader file
const char	c_szFragShaderBinFile[]  = "./OES3_FragShaderSample.fsc";  // FragmentShader file
const char	c_szVertShaderSrcFile[]  = "./OES3_VertShaderSample.vsh";  // VertexShader file
const char	c_szFragShaderSrcFile[]  = "./OES3_FragShaderSample.fsh";  // FragmentShader file

GLuint		g_uiProgramObjectCom;
GLuint		g_uiFragShaderCom, g_uiVertShaderCom;

const char	c_szVertShaderComBinFile[]  = "./OES3_VertShaderSampleCom.vsc";  // VertexShader file
const char	c_szFragShaderComBinFile[]  = "./OES3_FragShaderSample.fsc";     // FragmentShader file
const char	c_szVertShaderComSrcFile[]  = "./OES3_VertShaderSampleCom.vsh";  // VertexShader file
const char	c_szFragShaderComSrcFile[]  = "./OES3_FragShaderSample.fsh";     // FragmentShader file

static EGLNativePixmapTypeREL  sNativePixmap;

static struct 
{
	int fd;

	struct kms_driver *kms;
	kms_bo *bo;
	void *vaddr;
} g_sDrm;

/****************************************************************************
 *  DEFINES
 ****************************************************************************/

#define VERTEX_ARRAY		0
#define NORMAL_ARRAY		1
#define UV1_ARRAY			2
#define DIFFUSE_ARRAY		3

#define UV2_ARRAY			1

#define IMAGE_WIDTH			512
#define IMAGE_HEIGHT		512

#define TP_DRM_DEVNAME		"rcar-du"

/***********************************************************************************
 Function Name      : MainThread()
 Description        : The Main Thread
 ************************************************************************************/
void *MainThread(void *pParam)
{
	int loop = 60;

	printf("main start.\n");
	
	while(loop--)
	{
		usleep(1000 * 1000);
	}

	printf("main terminate.\n");
	
	g_hThreadTerminateEvent = 0;

	return pParam;
}

/*******************************************************************************
 * Function Name  : InitApplicationForPixmap
 * Description    : Beginning process.
 *******************************************************************************/
void InitApplicationForPixmap( void )
{
	EGLBoolean eRetStatus;

	eRetStatus = eglMakeCurrent(dpy, pixmapsurface, pixmapsurface, context);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglMakeCurrent failed.\n");
	}

	// Create texture
	CreateTexture( );

	InitShaderForPixmap( );

	glClearColor( 0.5f, 0.5f, 0.5f, 1.0f );
	glViewport( 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT );
}

/*******************************************************************************
 * Function Name  : RenderToPixmap
 * Description    : Drawing process to Pixmap.
 *******************************************************************************/
void RenderToPixmap( void )
{
	static int nCount = 1, nColor = 0;
	EGLBoolean eRetStatus;

	eRetStatus = eglMakeCurrent(dpy, pixmapsurface, pixmapsurface, context);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglMakeCurrent failed.\n");
	}

	if ( ( nCount % 200 ) == 0 )
	{
		nColor++;
		nColor %= 3;
	}

	nCount++;

	switch( nColor )
	{
	case 0: // Red.
		glClearColor( 0.5f, 0.0f, 0.0f, 1.0f );
	break;
	case 1: // Green.
		glClearColor( 0.0f, 0.5f, 0.0f, 1.0f );
	break;
	case 2: // Blue.
		glClearColor( 0.0f, 0.0f, 0.5f, 1.0f );
	break;
	default:
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}

	glViewport( 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT );

	// Beginning of scene
	glClear( GL_COLOR_BUFFER_BIT );
	glEnable( GL_CULL_FACE );

	// Current change
	ChangeCurrent( g_hTexture );

	SetupGLSLUniformForPixmap( );
	glUseProgram( g_uiProgramObject );

	DrawIndexStrips( );
}

/*******************************************************************************
 * Function Name  : InitApplicationForWindow
 * Description    : Beginning process.
 *******************************************************************************/
void InitApplicationForWindow( void )
{
	EGLBoolean eRetStatus;

	eRetStatus = eglMakeCurrent(dpy, windowsurface, windowsurface, context);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglMakeCurrent failed.\n");
	}

	// Create texture
	CreateTextureEGLImage();

	InitShaderForWindow( );

	glClearColor( 0.5f, 0.5f, 0.5f, 1.0f );
	glViewport( 0, 0, g_nScreenWidth, g_nScreenHeight );
}

/*******************************************************************************
 * Function Name  : RenderToWindow
 * Description    : Drawing process.
 *******************************************************************************/
void RenderToWindow( void )
{
	static int nCount = 1, nColor = 0;
	EGLBoolean eRetStatus;

	eRetStatus = eglMakeCurrent(dpy, windowsurface, windowsurface, context);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglMakeCurrent failed.\n");
	}

	if ( ( nCount % 200 ) == 0 )
	{
		nColor++;
		nColor %= 3;
	}

	nCount++;

	switch( nColor )
	{
	case 0: // Yellow.
		glClearColor( 0.5f, 0.5f, 0.0f, 1.0f );
	break;
	case 1: // Magenta.
		glClearColor( 0.0f, 0.5f, 0.5f, 1.0f );
	break;
	case 2: // Purple.
		glClearColor( 0.5f, 0.0f, 0.5f, 1.0f );
	break;
	default:
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}

	glViewport( 0, 0, g_nScreenWidth, g_nScreenHeight );

	// Beginning of scene
	glClear( GL_COLOR_BUFFER_BIT );
	glDisable( GL_CULL_FACE );

	// Current change
	ChangeCurrent( g_hTexture_eglimage );

	SetupGLSLUniformForWindow( );
	glUseProgram( g_uiProgramObjectCom );
    
    DrawSqure();
}

/*******************************************************************************
 * Function Name  : CreateTextureEGLImage
 * Description    : Create texture for EGLImage.
 *******************************************************************************/
void CreateTextureEGLImage( void )
{
	// Get texture handle
	glGenTextures( 1,&g_hTexture_eglimage );
	glBindTexture( GL_TEXTURE_2D, g_hTexture_eglimage );

    glEGLImageTargetTexture2D(GL_TEXTURE_2D, eglimage);
	{
		GLint glerr = glGetError();
		if(glerr) printf("<ERROR name=\"CreateTextureEGLImage\" glerr=\"0x%x\"/>\n", glerr);
	}
}

/*******************************************************************************
 * Function Name  : CreateTexture
 * Description    : Create texture.
 *******************************************************************************/
void CreateTexture( void )
{

	unsigned int	*pTex;

	// Get texture handle
	glGenTextures( 1,&g_hTexture );
	glBindTexture( GL_TEXTURE_2D, g_hTexture );

	// Create texture area
	// Set texture data to the area
	pTex = const_cast<unsigned int *>((texture + (texture[0] / sizeof(unsigned int))));
	glTexImage2D( GL_TEXTURE_2D , 0 , GL_RGBA , texture[2] , texture[1] ,0 , GL_RGBA , GL_UNSIGNED_BYTE , pTex );

	{
		GLint glerr = glGetError();
		if(glerr) printf("<ERROR name=\"CreateTexture\" glerr=\"0x%x\"/>\n", glerr);
	}
}

/*******************************************************************************
 * Function Name  : ChangeCurrent
 * Description    : Chage current status.
 *******************************************************************************/
void ChangeCurrent( GLuint htex )
{
	// Set drawing texture
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, htex );

	glUniform1i( glGetUniformLocation( g_uiProgramObject, "sampler2d"), 0 );

	// Set texture quality
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glUniform1i( glGetUniformLocation( g_uiProgramObject, "uTexFlag"), 1 );
}

/*******************************************************************************
 * Function Name  : SetupGLSLUniformForPixmap
 * Description    : Set all uniform constants.
 *******************************************************************************/
void SetupGLSLUniformForPixmap( void )
{
	static float fTheta = 0.0f;
	float		fAngle = 1.0f;
	RTMATRIX	ModelMatrix,ViewMatrix,ProjectionMatrix, MVMatrix, PMVMatrix;
	RTVECTOR3	CameraPosition = { 3.0f, -1.3f, 0.0f };
	RTVECTOR3	CameraPointing = { 0.0f,  1.1f, 0.0f };
	RTVECTOR3	UpVector       = { 0.0f, 1.0f, 0.0f };
	float		fFOVHoriz      = 60.0f * PI / 180.0f;    // Default : 60.0 [degree].
	float		fAspectRatio   = (float)IMAGE_WIDTH / (float)IMAGE_HEIGHT;

	fTheta -= ( fAngle / 180.0f ) * PI;

	RTMatrixIdentity( ModelMatrix );
	RTMatrixIdentity( ViewMatrix );
	RTMatrixIdentity( ProjectionMatrix );
	RTMatrixRotationY( ModelMatrix, fTheta );

	RTMatrixPerspective( ProjectionMatrix, fFOVHoriz, fAspectRatio, 0.1f, 1000.0f );
	RTMatrixLookAt( ViewMatrix, CameraPosition, CameraPointing, UpVector );

	RTMatrixMultiply( MVMatrix, ModelMatrix, ViewMatrix );
	RTMatrixMultiply( PMVMatrix, MVMatrix, ProjectionMatrix );

	int i32Location = glGetUniformLocation( g_uiProgramObject, "uPMVMatrix" );
	glUniformMatrix4fv( i32Location, 1, GL_FALSE, PMVMatrix.f );
}

/*******************************************************************************
 * Function Name  : SetupGLSLUniformForWindow
 * Description    : Set all uniform constants.
 *******************************************************************************/
void SetupGLSLUniformForWindow( void )
{
	static float fTheta = 0.0f;
	float		fAngle = 1.0f;
	RTMATRIX	ModelMatrix,ViewMatrix,ProjectionMatrix, MVMatrix, PMVMatrix;
	RTVECTOR3	CameraPosition = { 3.0f, -1.3f, 0.0f };
	RTVECTOR3	CameraPointing = { 0.0f,  1.1f, 0.0f };
	RTVECTOR3	UpVector       = { 0.0f, 1.0f, 0.0f };
	float		fFOVHoriz      = 60.0f * PI / 180.0f;    // Default : 60.0 [degree].
	float		fAspectRatio   = (float)g_nScreenWidth / (float)g_nScreenHeight;

	fTheta -= ( fAngle / 180.0f ) * PI;

	RTMatrixIdentity( ModelMatrix );
	RTMatrixIdentity( ViewMatrix );
	RTMatrixIdentity( ProjectionMatrix );
	RTMatrixRotationY( ModelMatrix, fTheta );

	RTMatrixPerspective( ProjectionMatrix, fFOVHoriz, fAspectRatio, 0.1f, 1000.0f );
	RTMatrixLookAt( ViewMatrix, CameraPosition, CameraPointing, UpVector );

	RTMatrixMultiply( MVMatrix, ModelMatrix, ViewMatrix );
	RTMatrixMultiply( PMVMatrix, MVMatrix, ProjectionMatrix );

	int i32Location = glGetUniformLocation( g_uiProgramObjectCom, "uPMVMatrix" );
	glUniformMatrix4fv( i32Location, 1, GL_FALSE, PMVMatrix.f );
}

/*******************************************************************************
 * Function Name    : DrawIndexStrips
 * Description      : Renders strips of triangles using indexed vertices
 *******************************************************************************/
void DrawIndexStrips( void )
{
	float colors[NUMBER_OF_INDEXED_VERTICES*4];

	for ( int i = 0; i < NUMBER_OF_INDEXED_VERTICES; i++ )
	{
		colors[i*4+0] = 1.0f;   colors[i*4+1] = 1.0f;
		colors[i*4+2] = 1.0f;   colors[i*4+3] = 1.0f;
	}

	glEnableVertexAttribArray( VERTEX_ARRAY );
	glEnableVertexAttribArray( NORMAL_ARRAY );
	glEnableVertexAttribArray( UV1_ARRAY );
	glEnableVertexAttribArray( DIFFUSE_ARRAY );

	glVertexAttribPointer( VERTEX_ARRAY,  3, GL_FLOAT, GL_FALSE, 0, Vertices );
	glVertexAttribPointer( NORMAL_ARRAY,  3, GL_FLOAT, GL_FALSE, 0, Normals );
	glVertexAttribPointer( UV1_ARRAY,     2, GL_FLOAT, GL_FALSE, 0, UVs );
	glVertexAttribPointer( DIFFUSE_ARRAY, 4, GL_FLOAT, GL_FALSE, 0, colors );

	glDrawElements( GL_TRIANGLES, NUMBER_OF_INDEXED_STRIPS*3, GL_UNSIGNED_SHORT, uIndexedStripsArray );

	glDisableVertexAttribArray( VERTEX_ARRAY );
	glDisableVertexAttribArray( NORMAL_ARRAY );
	glDisableVertexAttribArray( UV1_ARRAY );
	glDisableVertexAttribArray( DIFFUSE_ARRAY );
}

/*******************************************************************************
 * Function Name    : DrawSqure
 * Description      : Renders strips of triangles using indexed vertices
 *******************************************************************************/
void DrawSqure( void )
{
	const GLfloat vertex_rect_strip[] = {
	//  left-bottom        left-top          right-bottom      right-top
		-0.8f,-0.8f,1.0f , -0.8f,0.8f,1.0f , 0.8f,-0.8f,1.0f , 0.8f,0.8f,1.0f
	};
	const GLfloat teximage_coord[] = {
		0.0f,1.0f , 0.0f, 0.0f , 1.0f,1.0f , 1.0f,0.0f
	};

	glEnableVertexAttribArray( VERTEX_ARRAY );
	glEnableVertexAttribArray( UV2_ARRAY );
	glVertexAttribPointer( VERTEX_ARRAY,  3, GL_FLOAT, GL_FALSE, 0, vertex_rect_strip );
	glVertexAttribPointer( UV2_ARRAY,     2, GL_FLOAT, GL_FALSE, 0, teximage_coord );

	/*
		- set a primitive type and a position of vertices
	*/
	glDrawArrays(GL_TRIANGLE_STRIP , 0 , 4);

	glDisableVertexAttribArray( UV2_ARRAY );
	glDisableVertexAttribArray( VERTEX_ARRAY );
}

/*******************************************************************************
 * Function Name  : LoadShader
 * Description    : Loading of shader files.
 *******************************************************************************/
void LoadShader(const char* shader_path, const char* &m_pData, GLint &m_Size)
{
	bool m_bOpen;

	FILE* pFile = fopen(shader_path, "rb");
	if (pFile)
	{
		// Get the file size
		fseek(pFile, 0, SEEK_END);
		m_Size = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);

		// read the data, append a 0 byte as the data might represent a string
		char* pData = new char[m_Size + 1];
		pData[m_Size] = '\0';
		size_t BytesRead = fread(pData, 1, m_Size, pFile);

		if (BytesRead != m_Size)
		{
			delete [] pData;
			m_Size = 0;
		}
		else
		{
			m_pData = pData;
			m_bOpen = true;
		}
		fclose(pFile);

	}
	else
	{
		m_Size = 0;
	}
}

/*******************************************************************************
 * Function Name  : InitShaderForPixmap
 * Description    : Initialization of shader.
 *******************************************************************************/
void InitShaderForPixmap( void )
{
	g_uiProgramObject = glCreateProgram( );
	g_uiVertShader = glCreateShader( GL_VERTEX_SHADER );
	g_uiFragShader = glCreateShader( GL_FRAGMENT_SHADER );

	GLint       m_Size;
	const char* m_pData;

	LoadShader(c_szVertShaderBinFile, m_pData, m_Size);
	if (m_Size > 0)
	{
		glShaderBinary( 1, &g_uiVertShader, GL_SGX_BINARY_IMG, m_pData, m_Size);
	}
	else
	{
		LoadShader(c_szVertShaderSrcFile, m_pData, m_Size);
		if ( m_Size > 0)
		{
			GLint glerr;

			glShaderSource( g_uiVertShader, 1, &m_pData, NULL);

			glCompileShader(g_uiVertShader);
			glerr = glGetError();
			if(glerr) printf("<ERROR name=\"InitShaderForPixmap\" call=\"glShaderSource\" type=\"VertShader\" glerr=\"0x%x\"/>\n", glerr);
		}
		else
		{
			printf("File Open failed. The vsc or vsh file is needed.\n");
		}
	}

	LoadShader(c_szFragShaderBinFile, m_pData, m_Size);
	if (m_Size > 0)
	{
		glShaderBinary( 1, &g_uiFragShader, GL_SGX_BINARY_IMG, m_pData, m_Size);
	}
	else
	{
		LoadShader(c_szFragShaderSrcFile, m_pData, m_Size);
		if (m_Size > 0)
		{
			GLint glerr;

			glShaderSource( g_uiFragShader, 1, &m_pData, NULL);

			glCompileShader(g_uiFragShader);
			glerr = glGetError();
			if(glerr) printf("<ERROR name=\"InitShaderForPixmap\" call=\"glShaderSource\" type=\"FragShader\" glerr=\"0x%x\"/>\n", glerr);
		}
		else
		{
			printf("File Open failed. The fsc or fsh file is needed.\n");
		}
	}

	glAttachShader( g_uiProgramObject, g_uiVertShader );
	glAttachShader( g_uiProgramObject, g_uiFragShader );

	glBindAttribLocation( g_uiProgramObject, VERTEX_ARRAY,  "aVertex"  );
	glBindAttribLocation( g_uiProgramObject, NORMAL_ARRAY,  "aNormal"  );
	glBindAttribLocation( g_uiProgramObject, UV1_ARRAY,     "aUv1"     );
	glBindAttribLocation( g_uiProgramObject, DIFFUSE_ARRAY, "aDiffuse" );

	glLinkProgram( g_uiProgramObject );
	glUseProgram( g_uiProgramObject );
	
	{
		GLint glerr = glGetError();
		if(glerr) printf("<ERROR name=\"InitShaderForPixmap\" glerr=\"0x%x\"/>\n", glerr);
	}
}

/*******************************************************************************
 * Function Name  : InitShaderForWindow
 * Description    : Initialization of shader.
 *******************************************************************************/
void InitShaderForWindow( void )
{
	g_uiProgramObjectCom = glCreateProgram( );
	g_uiVertShaderCom = glCreateShader( GL_VERTEX_SHADER );
	g_uiFragShaderCom = glCreateShader( GL_FRAGMENT_SHADER );

	GLint       m_Size;
	const char* m_pData;

	LoadShader(c_szVertShaderComBinFile, m_pData, m_Size);
	if (m_Size > 0)
	{
		glShaderBinary( 1, &g_uiVertShaderCom, GL_SGX_BINARY_IMG, m_pData, m_Size);
	}
	else
	{
		LoadShader(c_szVertShaderComSrcFile, m_pData, m_Size);
		if ( m_Size > 0)
		{
			GLint glerr;

			glShaderSource( g_uiVertShaderCom, 1, &m_pData, NULL);

			glCompileShader(g_uiVertShaderCom);
			glerr = glGetError();
			if(glerr) printf("<ERROR name=\"InitShaderForWindow\" call=\"glShaderSource\" type=\"VertShader\" glerr=\"0x%x\"/>\n", glerr);
		}
		else
		{
			printf("File Open failed. The vsc or vsh file is needed.\n");
		}
	}

	LoadShader(c_szFragShaderComBinFile, m_pData, m_Size);
	if (m_Size > 0)
	{
		glShaderBinary( 1, &g_uiFragShaderCom, GL_SGX_BINARY_IMG, m_pData, m_Size);
	}
	else
	{
		LoadShader(c_szFragShaderComSrcFile, m_pData, m_Size);
		if (m_Size > 0)
		{
			GLint glerr;

			glShaderSource( g_uiFragShaderCom, 1, &m_pData, NULL);

			glCompileShader(g_uiFragShaderCom);
			glerr = glGetError();
			if(glerr) printf("<ERROR name=\"InitShaderForWindow\" call=\"glShaderSource\" type=\"FragShader\" glerr=\"0x%x\"/>\n", glerr);
		}
		else
		{
			printf("File Open failed. The fsc or fsh file is needed.\n");
		}
	}

	glAttachShader( g_uiProgramObjectCom, g_uiVertShaderCom );
	glAttachShader( g_uiProgramObjectCom, g_uiFragShaderCom );

	glBindAttribLocation( g_uiProgramObjectCom, VERTEX_ARRAY,  "aVertex"  );
	glBindAttribLocation( g_uiProgramObjectCom, UV2_ARRAY, "aMultiTexCoord0" );

	glLinkProgram( g_uiProgramObjectCom );
	glUseProgram( g_uiProgramObjectCom );
	
	{
		GLint glerr = glGetError();
		if(glerr) printf("<ERROR name=\"InitShaderForWindow\" glerr=\"0x%x\"/>\n", glerr);
	}
}

/*******************************************************************************
 * Function Name  : AcquireDRMMem
 * Description    : Acquire DRMMem
 *******************************************************************************/
void AcquireDRMMem(void)
{
	unsigned bo_attribs[] = {
					KMS_WIDTH,		IMAGE_WIDTH,
					KMS_HEIGHT,		IMAGE_HEIGHT,
					KMS_BO_TYPE,	KMS_BO_TYPE_SCANOUT_X8R8G8B8,
					KMS_TERMINATE_PROP_LIST
	};


	g_sDrm.fd = drmOpen(TP_DRM_DEVNAME, NULL);
	if (g_sDrm.fd >= 0)
	{
		drmDropMaster(g_sDrm.fd);

		kms_create(g_sDrm.fd, &g_sDrm.kms);
		kms_bo_create(g_sDrm.kms, bo_attribs, &g_sDrm.bo);
		kms_bo_map(g_sDrm.bo, &g_sDrm.vaddr);
	}
	
	sNativePixmap.width		= IMAGE_WIDTH;
	sNativePixmap.height	= IMAGE_HEIGHT;
	sNativePixmap.format	= EGL_NATIVE_PIXFORMAT_ARGB8888_REL;
	sNativePixmap.stride	= IMAGE_WIDTH;
	sNativePixmap.usage		= 0;
	sNativePixmap.pixelData	= g_sDrm.vaddr;
}

/*******************************************************************************
 * Function Name  : ReleaseDRMMem
 * Description    : Release DRMMem
 *******************************************************************************/
void ReleaseDRMMem(void)
{
	if (g_sDrm.fd >= 0)
	{
		kms_bo_destroy(&g_sDrm.bo);

		kms_destroy(&g_sDrm.kms);
		drmClose(g_sDrm.fd);
	}
}

/*******************************************************************************
 * Function Name  : main()
 * Description    : Initialization, message loop
 *******************************************************************************/
int main(int argc, char** argv)
{
	EGLConfig configs[2];
	EGLBoolean eRetStatus;
	EGLint config_count;
	EGLint major, minor;
	EGLint cfg_attribs[] = {EGL_BUFFER_SIZE,     32,
				EGL_DEPTH_SIZE,      8,
				EGL_SURFACE_TYPE,    EGL_WINDOW_BIT | EGL_PIXMAP_BIT,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
				EGL_NONE};

	NativeWindowType eglWindow = 0;
	NativePixmapType eglPixmap = 0;

    EGLint             aiPixAttrib[] = {  EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                                          EGL_NONE };

	pthread_t tid_window;

	AcquireDRMMem();
	eglPixmap = (NativeWindowType)&sNativePixmap;

    eglCreateImageKHR  = (EGLCREATEIMAGEKHR)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (EGLDESTROYIMAGEKHR)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2D = (GLEGLIMAGETARGETRENDERBUFFERSTORAGEOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");

	dpy = eglGetDisplay((NativeDisplayType)EGL_DEFAULT_DISPLAY);
	if(dpy == EGL_NO_DISPLAY)
	{
		printf("eglGetDisplay failed.\n");
	}

	eRetStatus = eglInitialize(dpy, &major, &minor);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglInitialize failed.\n");
	}

	eRetStatus = eglBindAPI(EGL_OPENGL_ES_API);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglBindAPI failed.\n");
	}

	eRetStatus = eglChooseConfig(dpy, cfg_attribs, configs, 2, &config_count);
	if ((eRetStatus != EGL_TRUE) || (config_count == 0))
	{
		printf("eglChooseConfig failed.\n");
	}

	windowsurface = eglCreateWindowSurface(dpy, configs[0], eglWindow, NULL);
	if (windowsurface == EGL_NO_SURFACE)
	{
		printf("eglCreateWindowSurface failed.\n");
	}

	pixmapsurface = eglCreatePixmapSurface(dpy, configs[0], eglPixmap, NULL);
	if (pixmapsurface == EGL_NO_SURFACE)
	{
		printf("eglCreatePixmapSurface failed. (err=0x%x)\n", eglGetError());
	}

	EGLint ai32ContextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	context = eglCreateContext(dpy, configs[0], EGL_NO_CONTEXT, ai32ContextAttribs);
	if (context == EGL_NO_CONTEXT)
	{
		printf("eglCreateContext failed.\n");
	}

	// Get screen width and height
	eRetStatus = eglQuerySurface(dpy, windowsurface, EGL_WIDTH, &g_nScreenWidth);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglQuerySurface failed.\n");
	}
	
	eRetStatus = eglQuerySurface(dpy, windowsurface, EGL_HEIGHT, &g_nScreenHeight);
	if (eRetStatus != EGL_TRUE)
	{
		printf("eglQuerySurface failed.\n");
	}

	eglimage = eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, eglPixmap, NULL);
	if (eglimage == EGL_NO_IMAGE_KHR)
	{
		printf("eglCreateImageKHR failed. %x\n", eglGetError());
	}

	g_hThreadTerminateEvent = 1;
	
	pthread_create(&tid_window, NULL, MainThread, NULL);
	pthread_create(&g_hRenderThread, NULL, RenderThread, NULL);

	pthread_join(tid_window, NULL);
	pthread_join(g_hRenderThread, NULL);

	eglDestroyImageKHR(dpy, eglimage);

	eglDestroyContext(dpy, context);
	eglDestroySurface(dpy, windowsurface);
	eglDestroySurface(dpy, pixmapsurface);

	eglTerminate(dpy);

	ReleaseDRMMem();

	printf("Application terminate.\n");

	return 0;

}

/***********************************************************************************
 * Function Name  : RenderThread()
 * Description    : Rendering Thread
 ************************************************************************************/
void *RenderThread(void *lpParameter)
{
	printf("thread start.\n");

	InitApplicationForPixmap();
	InitApplicationForWindow();

	while(g_hThreadTerminateEvent){
		RenderToPixmap();
        eglWaitGL();

        RenderToWindow();
        eglSwapBuffers(dpy, windowsurface);
	}

	AppTerminate();

	printf("thread exit.\n");

	return lpParameter;
}

/***********************************************************************************
 * Function Name  : AppTerminate()
 * Description    : Terminate Process
 ************************************************************************************/
void AppTerminate(void)
{
    eglWaitGL();
	glDeleteTextures(1, &g_hTexture);

	eglSwapBuffers(dpy, windowsurface);
	glDeleteTextures(1, &g_hTexture_eglimage);

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglReleaseThread();
}
