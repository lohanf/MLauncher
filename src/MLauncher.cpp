/*
============================================================================
 Name        : MLauncher.cpp
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Main application class
============================================================================
*/

// INCLUDE FILES
#include <eikstart.h>
#include "MLauncherApplication.h"

LOCAL_C CApaApplication* NewApplication()
	{
	return new CMLauncherApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication( NewApplication );
	}

