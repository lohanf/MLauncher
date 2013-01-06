/*
 ============================================================================
 Name		: MyFilesView.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2011 Florin Lohan. All rights reserved.
 Description : CMyFilesView declaration
 ============================================================================
 */

#ifndef MYFILESVIEW_H
#define MYFILESVIEW_H

// INCLUDES
#include <aknview.h>

class CMLauncherAppUi;
class CMyFilesContainer;
class CDocumentHandler;

// CLASS DECLARATION

/**
 *  CMyFilesView
 * 
 */
class CMyFilesView : public CAknView
{
public:
	// Constructors and destructor
	~CMyFilesView();
	static CMyFilesView* NewL();
	static CMyFilesView* NewLC();
private:
	CMyFilesView();
	void ConstructL();
public: //from base class
	/**
	 * From CEikAppUi, HandleCommandL.
	 * Takes care of command handling.
	 * @param aCommand Command to be handled.
	 */
	void HandleCommandL( TInt aCommand );

	/**
	 *  HandleStatusPaneSizeChange.
	 *  Called by the framework when the application status pane
	 *  size is changed.
	 */
	void HandleStatusPaneSizeChange();

	TUid Id() const;
	void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
	void DoDeactivate();
public:
	CMLauncherAppUi* MyAppUi(){return (CMLauncherAppUi*)AppUi();};
private:
	TInt Hide();
	TInt UnhideL();
	
public:
	CMyFilesContainer *iMyFilesContainer;
	CDocumentHandler *iDocHandler;
	RPointerArray<HBufC> iFiles;
	RPointerArray<HBufC> iPaths;
};
#endif // MYFILESVIEW_H
