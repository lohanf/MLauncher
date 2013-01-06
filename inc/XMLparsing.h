/*
============================================================================
 Name        : XMLparsing.h
 Author      : Florin Lohan
 Copyright   : Florin Lohan
 Description : XML Parsing classes.
============================================================================
*/

#ifndef XMLPARSING_H_
#define XMLPARSING_H_

#include <e32base.h>
#include <xml/contenthandler.h>

class RFile;
class CFileDirPool;
class TFileDirEntry;
class CMLauncherPreferences;
namespace Xml
{

class CPlsSelectionCH : public CBase, public MContentHandler
{
	public: //construction
	    static CPlsSelectionCH* NewLC();
	    ~CPlsSelectionCH();
	private:
	    CPlsSelectionCH();
	protected: //MContent handler
	    void OnStartDocumentL(const RDocumentParameters &/*aDocParam*/, TInt /*aErrorCode*/){};
	    void OnEndDocumentL(TInt /*aErrorCode*/){};
	    void OnStartElementL(const RTagInfo &aElement, const RAttributeArray &aAttributes, TInt aErrorCode);
	    void OnEndElementL(const RTagInfo &aElement, TInt aErrorCode);
	    void OnContentL(const TDesC8 &aBytes, TInt aErrorCode);
	    void OnStartPrefixMappingL(const RString &/*aPrefix*/, const RString &/*aUri*/, TInt /*aErrorCode*/){};
	    void OnEndPrefixMappingL(const RString &/*aPrefix*/, TInt /*aErrorCode*/){};
	    void OnIgnorableWhiteSpaceL(const TDesC8 &/*aBytes*/, TInt /*aErrorCode*/){};
	    void OnSkippedEntityL(const RString &/*aName*/, TInt /*aErrorCode*/){};
	    void OnProcessingInstructionL(const TDesC8 &/*aTarget*/, const TDesC8 &/*aData*/, TInt /*aErrorCode*/){};
	    void OnError(TInt aErrorCode);
	    TAny *GetExtendedInterface(const TInt32 /*aUid*/){return NULL;};
    public: //own stuff
        void ParseSelectionL(const TDesC& aFilename, CFileDirPool *aSelection);
        void ParseVersionBufferL(const TDesC8& aBuffer, TInt& aVmajor, TInt& aVminor, TInt& aSize, HBufC* &aDescription);
    private:
    	CFileDirPool *iTempTree; //owned
    	TFileDirEntry *iCurrentSource; //not owned
    	/*
        CFileDirEntry *iCurrentElement; //do not delete, not owned
        CFileDirEntry *iCurrentSource; //not owned
        CFileDirPool *iSources; //owned
        */
        HBufC* iDescription;
        TInt iVmajor,iVminor,iSize;
        TBool iGetContent;
    public:
        static RFs *iFs; //not owned
};


class CFileDetailsCH : public CBase, public MContentHandler
{
public: //construction
    static CFileDetailsCH* NewLC();
    ~CFileDetailsCH();
private:
    CFileDetailsCH();
protected: //MContent handler
    void OnStartDocumentL(const RDocumentParameters& /*aDocParam*/, TInt /*aErrorCode*/){};
    void OnEndDocumentL(TInt /*aErrorCode*/){};
    void OnStartElementL(const RTagInfo&aElement, const RAttributeArray &aAttributes, TInt aErrorCode);
    void OnEndElementL(const RTagInfo &aElement, TInt aErrorCode);
    void OnContentL(const TDesC8 &/*aBytes*/, TInt /*aErrorCode*/){};
    void OnStartPrefixMappingL(const RString &/*aPrefix*/, const RString &/*aUri*/, TInt /*aErrorCode*/){};
    void OnEndPrefixMappingL(const RString &/*aPrefix*/, TInt /*aErrorCode*/){};
    void OnIgnorableWhiteSpaceL(const TDesC8 &/*aBytes*/, TInt /*aErrorCode*/){};
    void OnSkippedEntityL(const RString &/*aName*/, TInt /*aErrorCode*/){};
    void OnProcessingInstructionL(const TDesC8 &/*aTarget*/, const TDesC8 &/*aData*/, TInt /*aErrorCode*/){};
    void OnError(TInt aErrorCode);
    TAny *GetExtendedInterface(const TInt32 /*aUid*/){return NULL;};
public: //own stuff
    void ParseFileDetailsL(const TDesC8& aFileDetails);
public: //parsed data
	TFileName iFileName;
	TInt iFileSize;
	TInt iTotalNrFiles;
	TInt iTotalSize;
	RArray<TInt> iFileSizes;
};

class CPreferencesCH : public CBase, public MContentHandler
{
public: //construction
    static CPreferencesCH* NewLC();
    ~CPreferencesCH();
private:
    CPreferencesCH();
protected: //MContent handler
    void OnStartDocumentL(const RDocumentParameters& /*aDocParam*/, TInt /*aErrorCode*/){};
    void OnEndDocumentL(TInt /*aErrorCode*/){};
    void OnStartElementL(const RTagInfo&aElement, const RAttributeArray &aAttributes, TInt aErrorCode);
    void OnEndElementL(const RTagInfo &aElement, TInt aErrorCode);
    void OnContentL(const TDesC8 &/*aBytes*/, TInt /*aErrorCode*/){};
    void OnStartPrefixMappingL(const RString &/*aPrefix*/, const RString &/*aUri*/, TInt /*aErrorCode*/){};
    void OnEndPrefixMappingL(const RString &/*aPrefix*/, TInt /*aErrorCode*/){};
    void OnIgnorableWhiteSpaceL(const TDesC8 &/*aBytes*/, TInt /*aErrorCode*/){};
    void OnSkippedEntityL(const RString &/*aName*/, TInt /*aErrorCode*/){};
    void OnProcessingInstructionL(const TDesC8 &/*aTarget*/, const TDesC8 &/*aData*/, TInt /*aErrorCode*/){};
    void OnError(TInt aErrorCode);
    TAny *GetExtendedInterface(const TInt32 /*aUid*/){return NULL;};
public: //own stuff
    void ParsePreferencesFileL(const TDesC& aFilename,CMLauncherPreferences *aPreferences);
private: //parsed data
	TInt iVersion;
	TInt iPlaylistIndex;
	CMLauncherPreferences *iPreferences; //not owned
public:
	static RFs *iFs; //not owned
};
}
#endif /*XMLPARSING_H_*/
