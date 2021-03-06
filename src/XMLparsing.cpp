
/*
XML parser framework is available for the S60 platform starting from 2nd Edition, Feature Pack 2.

CParser is a SAX-like parser, which parses XML files or XML strings, element by element.
To use it, a content handler class derived from MContentHandler must be implemented.
The content handler class implements interface methods such as OnStartElementL() and
OnEndElementL(), which are then called by the parser (CParser) for each XML tag. The content
handler implementation and MIME type of the parsed document are passed to the CParser object
at construction.
*/

// Link against xmlframework.lib
#include <xml/parser.h>
#include <xml/XmlFrameworkConstants.h>
#include <senxmlutils.h>

#include <f32file.h>
#include <eikenv.h>

#include "XMLparsing.h"
//#include "FFListView.h"
#include "TrackInfo.h"
#include "MLauncherPreferences.h"
#include "MLauncher.pan"
#include "log.h"

_LIT8(KXmlMimeType, "text/xml");
_LIT8(KYes8,"yes");

const TInt KMaxReadFromFile=2096; //2 KB
/*
// Implements MContentHandler interface
CMyContentHandler sax;
// Mime type of the parsed document
_LIT8(KXmlMimeType, "text/xml");
// Contruct the parser object
CParser* parser = CParser::NewLC(KXmlMimeType, sax);
// Start parsing XML from a descriptor
// CMyContentHandler will receive events
// generated by the parser
parser.ParseL(aDesMyXML);
// Destroy the parser when done.
CleanupStack::PopAndDestroy();
*/


/////////////////////////////////////////
// Implementation of CPlsSelectionCH
namespace Xml
{
RFs* CPlsSelectionCH::iFs=NULL;
CPlsSelectionCH* CPlsSelectionCH::NewLC()
{
	CPlsSelectionCH *self=new(ELeave) CPlsSelectionCH;
	CleanupStack::PushL(self);
	return self;
}

CPlsSelectionCH::CPlsSelectionCH()
{};

CPlsSelectionCH::~CPlsSelectionCH()
{
	delete iTempTree;//normally this is NULL at this point
	delete iDescription;//normally this is NULL at this point
};

void CPlsSelectionCH::OnStartElementL(const RTagInfo &aElement, const RAttributeArray &aAttributes, TInt aErrorCode)
{
	HBufC *path=NULL;
	TBool isDir;
	TBool isSource;
	TBool isSelected(EFalse);

	if(aElement.LocalName().DesC()==_L8("folder"))isDir=ETrue;
	else isDir=EFalse;
	if(aElement.LocalName().DesC()==_L8("source"))isSource=ETrue;
	else isSource=EFalse;

	//iLog->Write(_L8("Start Element "));
	//iLog->Write(aElement.LocalName().DesC());
	//iLog->Write(_L8(" "));

	TInt i;
	if(aElement.LocalName().DesC()==_L8("version"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("major"))
		    {
		        //iLog->Write(aAttributes[i].Value().DesC());
		        //get the version number
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iVmajor);
		    }
		    else
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("minor"))
		    {
		        //iLog->Write(aAttributes[i].Value().DesC());
		        //get the version number
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iVminor);
		    }
		    else
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("size"))
		    {
		        //iLog->Write(aAttributes[i].Value().DesC());
		        //get the size
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iSize);
		    }
		};
		return;
	};
	if(aElement.LocalName().DesC()==_L8("description"))
	{
		iGetContent=ETrue;
		return;
	};
	if(aElement.LocalName().DesC()==_L8("selection"))
	{
		__ASSERT_DEBUG(iTempTree==NULL,Panic(ETempTreeNotNull));
		iTempTree=CFileDirPool::NewL();
		return;
	};
	

	//source, folder or file
	for(i=0;i<aAttributes.Count();i++)
	{
		if(aAttributes[i].Attribute().LocalName().DesC()==_L8("path"))
		{
			//iLog->Write(aAttributes[i].Value().DesC());
			//copy the path
			//path=HBufC::NewLC(aAttributes[i].Value().DesC().Size());
			//path->Des().Copy(aAttributes[i].Value().DesC());
			path=SenXmlUtils::ToUnicodeLC(aAttributes[i].Value().DesC());
		}
		else
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("selected"))
				if(aAttributes[i].Value().DesC()==_L8("yes"))isSelected=ETrue;
				else isSelected=EFalse;
	};
	//iLog->Write(KEOL8);

	TInt flags(0);
	if(isDir)flags|=TFileDirEntry::EIsDir;
	if(isSource)flags|=TFileDirEntry::EIsDir | TFileDirEntry::EIsSource;
    if(isSelected)flags|=TFileDirEntry::EIsSelected;

	if(isSource)
	{
		TFileDirEntry *source=iTempTree->NewFileDirEntry(*path,flags,NULL); //if parent is NULL, it must be a source
		iTempTree->iCurrentParent=source;
		iCurrentSource=source;
	}
	else
	{
		//a normal element
		TFileDirEntry *child=iTempTree->NewFileDirEntry(*path,flags,iTempTree->iCurrentParent);
		iTempTree->iCurrentParent=child;
	};
		   
	if(path)CleanupStack::PopAndDestroy(path);//path was NewLC-ed
};

void CPlsSelectionCH::OnEndElementL(const RTagInfo &aElement, TInt aErrorCode)
{
	//iLog->Write(_L8("End Element"));
	//iLog->Write(KEOL8);
	if(iTempTree && iTempTree->iCurrentParent)
		iTempTree->iCurrentParent=iTempTree->iCurrentParent->GetProperParent(iTempTree);
	iGetContent=EFalse;
};

void CPlsSelectionCH::OnContentL(const TDesC8 &aBytes, TInt aErrorCode)
{
	if(iGetContent)
	    if (iDescription) // some data already added
		{
	    	iDescription = iDescription->ReAllocL(iDescription->Length() + aBytes.Length());
	    	HBufC* newData = HBufC::NewLC(aBytes.Length());
			newData->Des().Copy(aBytes);
			iDescription->Des() += newData->Des();
			CleanupStack::PopAndDestroy(newData);
		}
	    else // no data yet added.
		{
	    	iDescription = HBufC::NewL(aBytes.Length());
	    	iDescription->Des().Copy(aBytes);
		};
};

void CPlsSelectionCH::OnError(TInt aErrorCode)
{
	//TODO: do something here
};


void CPlsSelectionCH::ParseSelectionL(const TDesC& aFilename, CFileDirPool *aSelection)
{
	TInt size;

	//open the file and dump the content into xmlContent
	RFile file;
	__ASSERT_DEBUG(iFs,Panic(EiFsIsNull));
	TInt err=file.Open(*iFs,aFilename,EFileRead);
	if(err)return; //it is OK if there is no selection file, no need to Leave

	file.Size(size);
	if(size>KMaxReadFromFile)
		size=KMaxReadFromFile;
	HBufC8 *xmlContent=HBufC8::NewLC(size);
	TPtr8 ptr(xmlContent->Des());
	CParser* parser = CParser::NewLC(KXmlMimeType, *this);
	while(1)
	{
		err=file.Read(ptr);
		if(err || ptr.Length()==0)break; //error reading or reached the end of the file
		parser->ParseL(ptr);
	};
	file.Close();
	parser->ParseEndL();
	CleanupStack::PopAndDestroy(parser);
	CleanupStack::PopAndDestroy(xmlContent);

	if(iTempTree)
	{
		aSelection->iCurrentParent=aSelection->iRoot;
		iTempTree->iCurrentParent=iTempTree->iRoot;
		aSelection->ApplySelection(iTempTree);
	}

    //clean
    delete iTempTree;
    iTempTree=NULL;
};

void CPlsSelectionCH::ParseVersionBufferL(const TDesC8& aBuffer, TInt& aVmajor, TInt& aVminor, TInt& aSize, HBufC* &aDescription)
{
	LOG(ELogGeneral,1,"ParseVersionBufferL: start");
	// Contruct the parser object & parse
    CParser* parser = CParser::NewLC(KXmlMimeType, *this);
    iGetContent=EFalse;
    parser->ParseL(aBuffer);
    parser->ParseEndL();
    LOG0("ParseVersionBufferL: done parsing");
    //clean
    CleanupStack::PopAndDestroy(parser);
    iDescription->Des().Trim();

    //fill variables
    aDescription=iDescription;
    iDescription=NULL;
    aVmajor=iVmajor;
    aVminor=iVminor;
    aSize=iSize;

    LOG(ELogGeneral,-1,"ParseVersionBufferL: end");
};
/////////////////////////////////////////////////////////////
CFileDetailsCH* CFileDetailsCH::NewLC()
{
	CFileDetailsCH *self=new(ELeave) CFileDetailsCH;
	CleanupStack::PushL(self);
	return self;
};

CFileDetailsCH::CFileDetailsCH()
{};

CFileDetailsCH::~CFileDetailsCH()
{};

void CFileDetailsCH::OnStartElementL(const RTagInfo &aElement, const RAttributeArray &aAttributes, TInt aErrorCode)
{
	TInt i;
	LOG0("On start element");
	if(aElement.LocalName().DesC()==_L8("file"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("size"))
		    {
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iFileSize);
		        iFileSizes.Append(iFileSize);
		        LOG0("parsed: size");
		    }
		    else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("name"))
		    {
		    	HBufC16 *fileName=SenXmlUtils::ToUnicodeLC (aAttributes[i].Value().DesC());
		    	iFileName.Copy(*fileName);
		    	CleanupStack::PopAndDestroy(fileName);
		    	LOG0("parsed: name");
		    }
		    else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("totalnrfiles"))
		    {
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iTotalNrFiles);
		        LOG0("parsed: totalnrfiles");
		    }
		    else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("totalsize"))
		    {
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iTotalSize);
		        LOG0("parsed: totalsize");
		    };
		};
	}
	else if(aElement.LocalName().DesC()==_L8("fs"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("size"))
		    {
		        TLex8 lex(aAttributes[i].Value().DesC());
		        TInt size;
		        lex.Val(size);
		        iFileSizes.Append(size);
		        LOG0("parsed: fs:size");
		    };
		};
	};
};

void CFileDetailsCH::OnEndElementL(const RTagInfo &aElement, TInt aErrorCode)
{};

void CFileDetailsCH::OnError(TInt aErrorCode)
{
	LOG0("CFileDetailsCH: Parse error: %d",aErrorCode);
};


void CFileDetailsCH::ParseFileDetailsL(const TDesC8& aFileDetails)
{
	LOG(ELogBT,1,"ParseFileDetailsL: start (%d)",aFileDetails.Size());
	
    //all the code in this if is for logging purposes only
	HBufC *logStr=HBufC::NewLC(aFileDetails.Size());
	HBufC *logStrLine=HBufC::NewLC(aFileDetails.Size());

	logStr->Des().Copy(aFileDetails);
	TInt pos;
	TInt nr=0;
	TInt chars=0;

	do
	{
		pos=logStr->Locate('\n');
		LOG(ELogBT,0,"XML line%d: pos=%d",nr,pos);
		nr++;
		if(pos==KErrNotFound)
		{
			LOG(ELogBT,0,"%S",logStr);
		}
		else
		{
			logStrLine->Des().Copy(logStr->Left(pos));
			LOG(ELogBT,0,"%S",logStrLine);
			logStr->Des().Delete(0,pos+1);
			chars+=pos;
			LOG(ELogBT,0,"Chars parsed: %d",chars);
		};
	}
	while(pos!=KErrNotFound);
	CleanupStack::PopAndDestroy(logStrLine);
	CleanupStack::PopAndDestroy(logStr);
    //end of logging code

	//init some vars
	iTotalNrFiles=-1;
	iTotalSize=-1;
	// Contruct the parser object & parse
    CParser* parser = CParser::NewLC(KXmlMimeType, *this);
    parser->ParseBeginL();
    parser->ParseL(aFileDetails);
    parser->ParseEndL();
    //clean
    CleanupStack::PopAndDestroy(parser);
	LOG(ELogBT,-1,"ParseFileDetailsL: end");
};


/////////////////////////////////////////////////////////////
RFs* CPreferencesCH::iFs=NULL;
CPreferencesCH* CPreferencesCH::NewLC()
{
	CPreferencesCH *self=new(ELeave) CPreferencesCH;
	CleanupStack::PushL(self);
	return self;
};

CPreferencesCH::CPreferencesCH() : iPlaylistIndex(0)
{};

CPreferencesCH::~CPreferencesCH()
{};

void CPreferencesCH::OnStartElementL(const RTagInfo &aElement, const RAttributeArray &aAttributes, TInt aErrorCode)
{
	TInt i;
	//LOG(ELogGeneral,1,"OnStartElementL++");
	if(aElement.LocalName().DesC()==_L8("version"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("v"))
		    {
		        TLex8 lex(aAttributes[i].Value().DesC());
		        lex.Val(iVersion);
		        //LOG0("parsed: version:v");
		    };
		};
	}
	else if(aElement.LocalName().DesC()==_L8("startingdir"))
	{
		HBufC *dir(NULL);
		TInt nrFiles(-1); //means unknown
		
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("dir"))
		    {
		    	dir=SenXmlUtils::ToUnicodeLC(aAttributes[i].Value().DesC());	
		    }
		    else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("nrfiles"))
		    {
		    	TLex8 lex(aAttributes[i].Value().DesC());
		    	lex.Val(nrFiles);
		    };
		};
		//LOG0("parsed: startingdir");
		iPreferences->AddStartingDirL(dir,nrFiles);
		CleanupStack::Pop(dir);
	}
	else if(aElement.LocalName().DesC()==_L8("blacklisteddir"))
	{
		HBufC *dir(NULL);
		TInt nrFiles(-1); //means unknown

		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("dir"))
			{
				dir=SenXmlUtils::ToUnicodeLC(aAttributes[i].Value().DesC());	
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("nrfiles"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(nrFiles);
			};
		};
		iPreferences->AddBlacklistedDirL(dir,nrFiles);
		CleanupStack::Pop(dir);
		//LOG0("parsed: blacklisteddir");
	}
	else if(aElement.LocalName().DesC()==_L8("keepselection"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("ks"))
		    {
		    	if(aAttributes[i].Value().DesC()==KYes8)
		    		iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesKeepSelection;
		    	else
		    		iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesKeepSelection;
		    	//LOG0("parsed: keepselection:ks");
		    };
		};
	}
	else if(aElement.LocalName().DesC()==_L8("launchembedded"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
		    if(aAttributes[i].Attribute().LocalName().DesC()==_L8("le"))
		    {
		    	if(aAttributes[i].Value().DesC()==KYes8)
		    		iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesLaunchEmbedded;
		    	else
		    		iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesLaunchEmbedded;
		    	//LOG0("parsed: launchembedded:le");
		    };
		};
	}
	else if(aElement.LocalName().DesC()==_L8("usepartialcheck"))
	{
	    for(i=0;i<aAttributes.Count();i++)
	    {
	        if(aAttributes[i].Attribute().LocalName().DesC()==_L8("upc"))
	        {
	            if(aAttributes[i].Value().DesC()==KYes8)
	                iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesUsePartialCheck;
	            else
	                iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesUsePartialCheck;
	            //LOG0("parsed: usepartialcheck:upc");
	        };
	    };
	}
	else if(aElement.LocalName().DesC()==_L8("useinternalmusicplayer"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("uimp"))
			{
				if(aAttributes[i].Value().DesC()==KYes8)
					iPreferences->iPFlags|=CMLauncherPreferences::EPreferencesUseInternalMusicPlayer;
				else
					iPreferences->iPFlags&=~CMLauncherPreferences::EPreferencesUseInternalMusicPlayer;
				//LOG0("parsed: usepartialcheck:upc");
			};
		};
	}
	else if(aElement.LocalName().DesC()==_L8("flags"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("f"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				TUint flags;
				lex.Val(flags,EBinary);
				iPreferences->iPFlags=flags;
				//LOG0("parsed: flags:f");
			};
		};
	}
	else if(aElement.LocalName().DesC()==_L8("musicplayerthemesdata"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("mptd"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(iPreferences->iMusicPlayerThemeData,EHex);
				//LOG0("parsed: musicplayerthemesdata:mptd");
			};
		};
	}
	else if(aElement.LocalName().DesC()==_L8("equalizer"))
		{
			for(i=0;i<aAttributes.Count();i++)
			{
				if(aAttributes[i].Attribute().LocalName().DesC()==_L8("preset"))
				{
					TLex8 lex(aAttributes[i].Value().DesC());
					lex.Val(iPreferences->iAudioEqualizerPreset);
					//LOG0("parsed: equalizer:preset (%d)",iPreferences->iAudioEqualizerPreset);
				};
			};
		}
	else if(aElement.LocalName().DesC()==_L8("volume"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("v"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(iPreferences->iVolume);
				//LOG0("parsed: volume:v");
			};
		};
	}
	else if(aElement.LocalName().DesC()==_L8("playlist"))
	{
		TBool hasCurrentAttribute(EFalse);
		TInt nrElements,index,position,duration(0),totalduration(0);
		HBufC *name(NULL);
		TBool isCurrent(EFalse);
		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("nrelements"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(nrElements);
				//LOG0("parsed: playlist:nrelements");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("index"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(index);
				//LOG0("parsed: playlist:index");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("position"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(position);
				//LOG0("parsed: playlist:position");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("duration"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(duration);
				//LOG0("parsed: playlist:duration");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("totalduration"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(totalduration);
				//LOG0("parsed: playlist:totalduration");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("name"))
			{
				if(aAttributes[i].Value().DesC().Length()>0)
					name=SenXmlUtils::ToUnicodeLC(aAttributes[i].Value().DesC());
				//LOG0("parsed: playlist:name");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("iscurrent"))
			{
				if(aAttributes[i].Value().DesC()==KYes8)
					isCurrent=ETrue;
				else
					isCurrent=EFalse;
				hasCurrentAttribute=ETrue;
				//LOG0("parsed: playlist:iscurrent");
			};					
		};
		//now add the list
		//LOG0("adding to playlist");
		CPlaylist *newPlaylist=new(ELeave)CPlaylist;
		newPlaylist->SetMetadata(nrElements,index,position,duration,totalduration,name,iPlaylistIndex,isCurrent); //transfer ownership for name
		iPreferences->iPlaylists->AppendL(newPlaylist);
		iPlaylistIndex++;
		if(name)
			CleanupStack::Pop(name);
		//LOG0("added to playlist");
	}
	else if(aElement.LocalName().DesC()==_L8("crossfading"))
	{
		for(i=0;i<aAttributes.Count();i++)
		{
			if(aAttributes[i].Attribute().LocalName().DesC()==_L8("timems"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(iPreferences->iCrossfadingTimeMs);
				//LOG0("parsed: crossfading:timems");
			}
			else if(aAttributes[i].Attribute().LocalName().DesC()==_L8("volrampupms"))
			{
				TLex8 lex(aAttributes[i].Value().DesC());
				lex.Val(iPreferences->iCrossfadingTimeVolRampUpMs);
				//LOG0("parsed: crossfading:volrampupms");
			};
		};
	};
	//LOG(ELogGeneral,-1,"OnStartElementL--");
};

void CPreferencesCH::OnEndElementL(const RTagInfo &aElement, TInt aErrorCode)
{};

void CPreferencesCH::OnError(TInt aErrorCode)
{
	LOG0("CPreferencesCH: Parse error: %d",aErrorCode);
};


void CPreferencesCH::ParsePreferencesFileL(const TDesC& aFilename,CMLauncherPreferences *aPreferences)
{
	LOG(ELogGeneral,1,"ParsePreferencesFileL: start");

	//init some vars
	iPreferences=aPreferences;

	//open the file and dump the content into xmlContent
	RFile file;
	TInt size;
	__ASSERT_DEBUG(iFs,Panic(EiFsIsNull));
	TInt err=file.Open(*iFs,aFilename,EFileRead);
	if(err)
	{
		aPreferences->iCFlags|=CMLauncherPreferences::ENoPreferencesFileFound;
		LOG(ELogGeneral,-1,"ParsePreferencesFileL: end (no preferences file found)");
		return; //it is OK if there is no preferences file, no need to Leave
	};

	file.Size(size);
	HBufC8 *xmlContent=HBufC8::NewLC(size);
	TPtr8 ptr(xmlContent->Des());
	file.Read(ptr);
	file.Close();

	// Contruct the parser object & parse
    CParser* parser = CParser::NewLC(KXmlMimeType, *this);
    parser->ParseL(*xmlContent);
    parser->ParseEndL();
    //clean
    CleanupStack::PopAndDestroy(parser);
    CleanupStack::PopAndDestroy(xmlContent);
	LOG(ELogGeneral,-1,"ParsePreferencesFileL: end");
};

}//namespace xml
