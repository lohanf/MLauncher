/*
 ============================================================================
 Name		: PreferencesForm.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CPreferencesView implementation
 ============================================================================
 */
#include <eikmfne.h>
#include <eikdpage.h>
#include <aknnumedwin.h> 
#include <aknpopupfieldtext.h>
#include <MLauncher.rsg>
#include "MLauncher.hrh"
#include "PreferencesView.h"
#include "MLauncherPreferences.h"
#include "MLauncherDocument.h"
#include "MLauncherAppUi.h"

CPreferencesView::CPreferencesView()
{
	// No implementation required
}

CPreferencesView::~CPreferencesView()
{
}

CPreferencesView* CPreferencesView::NewLC(TInt aFocusPage)
{
	CPreferencesView* self = new (ELeave) CPreferencesView();
	CleanupStack::PushL(self);
	self->ConstructL(aFocusPage);
	return self;
}

CPreferencesView* CPreferencesView::NewL(TInt aFocusPage)
{
	CPreferencesView* self = CPreferencesView::NewLC(aFocusPage);
	CleanupStack::Pop(); // self;
	return self;
}

void CPreferencesView::ConstructL(TInt aFocusPage)
{
	iPreferences=((CMLauncherDocument*)MyAppUi()->Document())->iPreferences;
	iFocusPage=aFocusPage;
}

void CPreferencesView::DoDeactivate()
{
    /*
	if(iSourcesContainer)
    {
        AppUi()->RemoveFromStack(iSourcesContainer);
        delete iSourcesContainer;
        iSourcesContainer = NULL;
    }
    */
}

void CPreferencesView::DoActivateL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
    /*
	if(!iSourcesContainer)
    {
    	iSourcesContainer = CSourcesContainer::NewL( ClientRect(), this);
    };
    iSourcesContainer->SetMopParent(this);
    AppUi()->AddToStackL( *this, iSourcesContainer );
    iChangesMade=EFalse;
    */
}

TUid CPreferencesView::Id() const
{
    return TUid::Uid(KSourcesViewId); //defined in hrh
}

void CPreferencesView::HandleCommandL( TInt aCommand )
{
	
}
