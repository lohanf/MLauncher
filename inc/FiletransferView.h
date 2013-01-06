#ifndef FILETRANSFERVIEW_H_
#define FILETRANSFERVIEW_H_

#include <aknview.h>

class CFiletransferContainer;

class CFiletransferView : public CAknView
{
public: //constructors and destructors
    void ConstructL();
    CFiletransferView();
    virtual ~CFiletransferView();
    static CFiletransferView* NewLC();

public: // from CAknView

    TUid Id() const;
    void DoActivateL(const TVwsViewId& aPrevViewId,TUid aCustomMessageId, const TDesC8& aCustomMessage);
    void DoDeactivate();

    /**
     *  HandleStatusPaneSizeChange.
     *  Called by the framework when the application status pane
		*  size is changed.
     */
	void HandleStatusPaneSizeChange();

	/**
     * From CEikAppUi, HandleCommandL.
     * Takes care of command handling.
     * @param aCommand Command to be handled.
     */
    void HandleCommandL( TInt aCommand );

public:
    enum TDirection
    {
 		EDirectionSending=1,
 		EDirectionReceiving
 	};
 	void NewTransferL(const TInt aNrFiles, const TInt aTotalSize, const RArray<TInt> &aFileSizes);
 	void NewFileL(const TInt aDirection,const TDes& aFilename,const TInt aSize);
 	void UpdateData(const TInt aBytesTransfered);
 	void UpdateAllData();
public: // Data

    /**
    * The container
    * Owned by us
    */
	CFiletransferContainer* iFiletransferContainer;

public:	//this data is accessed directly by the container
	//data for the labels
	TInt iDirection;
	TFileName iLabelTextFilename;
	TInt iCurrentFileSize,iBytesTransfered;
	TInt iCurrentFileNo,iTotalFilesNo;
	TInt iTotalBytesTransfered,iTotalBytes2Transfer;
	TInt iInstantSpeed,iAverageSpeed;
	TTime iStartTime;
	TTime iLastTime;
	TInt iElapsedTime,iEstimatedTime;
	TInt iUpdateTreshold;
	TInt iBytesTransferedLastTime;

	RArray<TInt> iFileSizes;

	//TInt iXX;// temp
};

#endif /*FILETRANSFERVIEW_H_*/
