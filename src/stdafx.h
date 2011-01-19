// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)

    #define NOMINMAX
    #pragma warning( disable : 4290 ) // warnings about not being able to handle throw 
    #pragma warning( disable : 4996 ) // warnings about not being able to handle throw 

    #ifndef VC_EXTRALEAN
    #define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
    #endif

    #include "targetver.h"

    #define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

    #include <afxwin.h>         // MFC core and standard components
    #include <afxext.h>         // MFC extensions

    #ifndef _AFX_NO_OLE_SUPPORT
    #include <afxole.h>         // MFC OLE classes
    #include <afxodlgs.h>       // MFC OLE dialog classes
    #include <afxdisp.h>        // MFC Automation classes
    #endif // _AFX_NO_OLE_SUPPORT

    #ifndef _AFX_NO_DB_SUPPORT
    #include <afxdb.h>                      // MFC ODBC database classes
    #endif // _AFX_NO_DB_SUPPORT

    #ifndef _AFX_NO_DAO_SUPPORT
    #include <afxdao.h>                     // MFC DAO database classes
    #endif // _AFX_NO_DAO_SUPPORT

    #ifndef _AFX_NO_OLE_SUPPORT
    #include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
    #endif
    #ifndef _AFX_NO_AFXCMN_SUPPORT
    #include <afxcmn.h>                     // MFC support for Windows Common Controls
    #endif // _AFX_NO_AFXCMN_SUPPORT

    #include <afxsock.h>            // MFC socket extensions

    #include <direct.h>
#endif
