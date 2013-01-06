/*
============================================================================
 Name        : MLauncherApplication.h
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Declares main application class.
============================================================================
*/

#ifndef __MLAUNCHERAPPLICATION_H__
#define __MLAUNCHERAPPLICATION_H__

// INCLUDES
#include <aknapp.h>

// CLASS DECLARATION

/**
* CMLauncherApplication application class.
* Provides factory to create concrete document object.
* An instance of CMLauncherApplication is the application part of the
* AVKON application framework for the MLauncher example application.
*/
class CMLauncherApplication : public CAknApplication
    {
    public: // Functions from base classes

        /**
        * From CApaApplication, AppDllUid.
        * @return Application's UID (KUidMLauncherApp).
        */
        TUid AppDllUid() const;
        
        
        virtual ~CMLauncherApplication();
        
    protected: // Functions from base classes

        /**
        * From CApaApplication, CreateDocumentL.
        * Creates CMLauncherDocument document object. The returned
        * pointer in not owned by the CMLauncherApplication object.
        * @return A pointer to the created document object.
        */
        CApaDocument* CreateDocumentL();
    };

#endif // __MLAUNCHERAPPLICATION_H__

// End of File
