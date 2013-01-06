/*
============================================================================
 Name        : MLauncherListContainer.h
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : Declares view class for application.
============================================================================
*/

#ifndef __FFLISTCONTAINER_h__
#define __FFLISTCONTAINER_h__

// INCLUDES
#include <coecntrl.h>
#include <aknlists.h>
#include <eiklbo.h>
#include <e32const.h>
#ifdef __TOUCH_ENABLED__
#include <aknsfld.h>
#endif
#include "defs.h"

class CFFListView;
class TFileDirEntry;
class CFileDirPool;
class CAknSearchField;

// CLASS DECLARATION
class CFFListContainer : public CCoeControl, MEikListBoxObserver, MCoeControlObserver 
    {
    public: // New methods

        /**
        * NewL.
        * Two-phased constructor.
        * Create a CFFListContainer object, which will draw itself to aRect.
        * @param aRect The rectangle this view will be drawn to.
        * @return a pointer to the created instance of CFFListContainer.
        */
        static CFFListContainer* NewL( const TRect& aRect, CFileDirPool *aPool, CFFListView *aListView);

        /**
        * NewLC.
        * Two-phased constructor.
        * Create a CFFListContainer object, which will draw itself
        * to aRect.
        * @param aRect Rectangle this view will be drawn to.
        * @return A pointer to the created instance of CFFListContainer.
        */
        static CFFListContainer* NewLC( const TRect& aRect, CFileDirPool *aPool, CFFListView *aListView);

        /**
        * ~CFFListContainer
        * Virtual Destructor.
        */
        virtual ~CFFListContainer();

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        * Perform the second phase construction of a
        * CFFListContainer object.
        * @param aRect The rectangle this view will be drawn to.
        */
        void ConstructL(const TRect& aRect, CFileDirPool *aPool);

        /**
        * CFFListContainer.
        * C++ default constructor.
        */
        CFFListContainer();

    public:  // Functions from base classes

        /**
        * From CCoeControl, Draw
        * Draw this CFFListContainer to the screen.
        * @param aRect the rectangle of this view that needs updating
        */
        void Draw( const TRect& aRect ) const;

        /**
        * From CoeControl, SizeChanged.
        * Called by framework when the view size is changed.
        */
        void SizeChanged();

        /**
        * From CCoeControl,CountComponentControls.
        */
        TInt CountComponentControls() const;

        /**
        * From CCoeControl,ComponentControl.
        */
        CCoeControl* ComponentControl( TInt aIndex ) const;

        /**
        * From CCoeControl,OfferKeyEventL.
        */
        TKeyResponse OfferKeyEventL( const TKeyEvent& aKeyEvent, TEventCode aType );
        
        void HandleResourceChange(TInt aType);

#ifdef __TOUCH_ENABLED__
        void HandlePointerEventL (const TPointerEvent &aPointerEvent);
#endif
        
    public: // from MEikListBoxObserver
        virtual void  HandleListBoxEventL (CEikListBox *, TListBoxEvent );
        
        //from MCoeControlObserver 
        virtual void HandleControlEventL(CCoeControl *aControl, TCoeEvent aEventType);

        //own functions:
        void CreateIconsL(TBool aUpdateIcons);
        void AddListElementsL(CFileDirPool& aPool, TBool aUpdate=EFalse);
        void StoreSelection(CFileDirPool *aPool, TBool aDeleteDisplayName=ETrue);
        
        void SetCurrentItemAndMSK(TFileDirEntry& aDirEntry);
        void UpdateProgressBar();
        void ShowFindPopup();
        void SetListEmptyTextL();
    private:
        void CreateListL();
        void CreatePopupFindL();
    public: //list stuff

        CAknSingleGraphicStyleListBox *iFileList;
        CAknSearchField *iFindBox;
        CFFListView *iListView;//not owned, do not delete
#ifdef __TOUCH_ENABLED__
        TInt iLastSelectionIndex;
        TInt iDragSelectionIndex;
        //TInt iSelectionWidth;
        TBool iDragOperation;
        TPoint iDragStart;
#endif
        //stuff for the metadata progress bar
        const CFont *iFont;
        TInt iFontHeight;
    private:
        bool iHasSpecialElements;
    };

#endif // __FFLISTCONTAINER_h__

// End of File
