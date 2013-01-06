/*
 ============================================================================
 Name        : CStopwatchForm.cpp
 Author      : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CStopwatchForm implementation
 ============================================================================
 */
#include <eikmfne.h>
#include <aknpopupfieldtext.h>
#include <MLauncher.rsg>
#include "MLauncher.hrh"
#include "StopwatchSettings.h"

CStopwatchForm::CStopwatchForm(TStopwatchData &aData) :
    iData(&aData)
    {
    // No implementation required
    }

CStopwatchForm::~CStopwatchForm()
{
}

CStopwatchForm* CStopwatchForm::NewLC(TStopwatchData &aData)
{
    CStopwatchForm* self = new (ELeave)CStopwatchForm(aData);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CStopwatchForm* CStopwatchForm::NewL(TStopwatchData &aData)
{
    CStopwatchForm* self=CStopwatchForm::NewLC(aData);
    CleanupStack::Pop(); // self;
    return self;
}

void CStopwatchForm::ConstructL()
{

}

void CStopwatchForm::PreLayoutDynInitL()
{
    CEikDurationEditor* durationEd = (CEikDurationEditor*)Control( EStopwatchSettingDuration );
    durationEd->SetDuration( iData->iDuration );

    CAknPopupFieldText* popupFieldTextWhen = (CAknPopupFieldText*)Control( EStopwatchSettingPlayWhen );
    popupFieldTextWhen->SetCurrentValueIndex( iData->iPlayWhenIndex );

    CAknPopupFieldText* popupFieldTextWhat = (CAknPopupFieldText*)Control( EStopwatchSettingPlayWhat );
    popupFieldTextWhat->SetCurrentValueIndex( iData->iPlayWhatIndex );
}

TBool CStopwatchForm::SaveFormDataL()
{
    CEikDurationEditor* durationEd = (CEikDurationEditor*)Control( EStopwatchSettingDuration );
    iData->iDuration=durationEd->Duration();

    CAknPopupFieldText* popupFieldTextWhen = (CAknPopupFieldText*)Control( EStopwatchSettingPlayWhen );
    iData->iPlayWhenIndex=(TStopwatchData::TPlayWhen)popupFieldTextWhen->CurrentValueIndex();

    CAknPopupFieldText* popupFieldTextWhat = (CAknPopupFieldText*)Control( EStopwatchSettingPlayWhat );
    iData->iPlayWhatIndex=(TStopwatchData::TPlayWhat)popupFieldTextWhat->CurrentValueIndex();

    return ETrue;
}

TBool CStopwatchForm::OkToExitL(TInt aButtonId)
{
    TBool exit=CAknForm::OkToExitL(aButtonId);
    if(aButtonId==EAknSoftkeyOk)
    {
        SaveFormDataL();
        exit=ETrue;
    };
    return exit;
}
/*
CAknSettingItem * CStopwatchForm::CreateSettingItemL (TInt aIdentifier)
{
    // method is used to create specific setting item as required at run-time.
    // aIdentifier is used to determine what kind of setting item should be
    // created
    CAknSettingItem* settingItem = NULL;

    switch (aIdentifier)
    {
    case EStopwatchSettingDuration:
        settingItem = new (ELeave) CAknTimeOffsetSettingItem(aIdentifier, iData->iTime);
        break;
    case EStopwatchSettingPlayWhen:
        settingItem = new (ELeave) CAknBinaryPopupSettingItem(aIdentifier, iData->iPlayToCountdown);
        break;
    case EStopwatchSettingPlayWhat:
        settingItem = new (ELeave) CAknEnumeratedTextPopupSettingItem(aIdentifier, iData->iPlaySelection);
        break;
    default:
        break;
    };
    return settingItem;
}

void CStopwatchForm::EditCurrentItemL()
{
    // invoke EditItemL on the current item
    EditItemL(ListBox()->CurrentItemIndex(), ETrue);  // invoked from menu

    //Updating CAknPasswordSettingItem.
    if(ListBox()->CurrentItemIndex()==0)
    {
        (*(SettingItemArray()))[ListBox()->CurrentItemIndex()]->UpdateListBoxTextL();
    }

    StoreSettingsL();
};
*/
