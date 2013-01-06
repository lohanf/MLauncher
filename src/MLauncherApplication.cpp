/*
============================================================================
 Name        : MLauncherApplication.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Main application class
============================================================================
*/

// INCLUDE FILES
#include "MLauncher.hrh"
#include "MLauncherDocument.h"
#include "MLauncherApplication.h"
#include "log.h"

// ============================ MEMBER FUNCTIONS ===============================

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidMLauncherApp = { _UID3 };

// -----------------------------------------------------------------------------
// CMLauncherApplication::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* CMLauncherApplication::CreateDocumentL()
    {
    // Create an MLauncher document, and return a pointer to it
    return (static_cast<CApaDocument*>
                    ( CMLauncherDocument::NewL( *this ) ) );
    }

// -----------------------------------------------------------------------------
// CMLauncherApplication::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CMLauncherApplication::AppDllUid() const
    {
    // Return the UID for the MLauncher application
    return KUidMLauncherApp;
    }

CMLauncherApplication::~CMLauncherApplication()
{
	LOG_END;//
}
// End of File
