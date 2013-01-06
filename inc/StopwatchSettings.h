/*
 ============================================================================
 Name        : StopwatchSettings.h
 Author	     : Florin Lohan
 Version	 : 1.0
 Copyright   : Florin Lohan
 Description : CStopwatchForm declaration
 ============================================================================
 */

#ifndef STOPWATCHFORM_H
#define STOPWATCHFORM_H

// INCLUDES
#include <aknform.h>

const TInt KDefaultCountdownTimerSeconds=420; //7 minutes

// CLASS DECLARATION

/**
 *  CStopwatchForm
 *
 */
class TStopwatchData
{
public:
    enum TPlayWhen
    {
        EPlayAtCountdown =0,
        EPlayUntilCountdown
    };
    enum TPlayWhat
    {
        EPlaySelection =0,
        EPlayAlarm
    };
    TTimeIntervalSeconds  iDuration;
    TPlayWhen iPlayWhenIndex;
    TPlayWhat iPlayWhatIndex;

    //constructor
    TStopwatchData(): iDuration(KDefaultCountdownTimerSeconds), iPlayWhenIndex(EPlayAtCountdown), iPlayWhatIndex(EPlaySelection) {};
};

class CStopwatchForm : public CAknForm
{
public:
    // Constructors and destructor

    /**
     * Destructor.
     */
    ~CStopwatchForm();

    /**
     * Two-phased constructor.
     */
    static CStopwatchForm* NewL(TStopwatchData &aData);

    /**
     * Two-phased constructor.
     */
    static CStopwatchForm* NewLC(TStopwatchData &aData);

private:

    /**
     * Constructor for performing 1st stage construction
     */
    CStopwatchForm(TStopwatchData &aData);

    /**
     * EPOC default constructor for performing 2nd stage construction
     */
    void ConstructL();

protected: // from CAknForm

    virtual void PreLayoutDynInitL();

    virtual TBool SaveFormDataL();


    virtual TBool OkToExitL(TInt aButtonId);
    /**
     * Function:    EditCurrentItemL
     *
     * Discussion:  Starts the setting page for the currently selected item
     *              in the list.
     */
    //void EditCurrentItemL();

    //void HandleListBoxEventL(CEikListBox* /*aListBox*/, TListBoxEvent aEventType);

public:
    TStopwatchData *iData;
};

#endif // STOPWATCHFORM_H
