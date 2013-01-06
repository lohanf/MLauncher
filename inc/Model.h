/*
 ============================================================================
 Name		: Model.h
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2011 Florin Lohan. All rights reserved.
 Description : CModel declaration
 ============================================================================
 */

#ifndef MODEL_H
#define MODEL_H

// INCLUDES
#include <e32std.h>
#include <e32base.h>
#include <f32file.h>

typedef TUint32 TMLAddr;
// CLASS DECLARATION

/**
 *  CModel
 * 
 */
class CModel : public CBase
{
public:
	
	~CModel();
	static CModel* NewL(RFs &aFs);
	
	void Load();
	void Save();

private:
	CModel();
	void ConstructL(RFs &aFs);
	
	TUint8* ExpandL(TInt aSize, TMLAddr &aAddr);
	void GetMemoryInfo(TInt &aNrSegments, TInt &aLastSegmentFill);

private://member vars
	RFs *iFs; //not owned

	RPointerArray<TUint8> iSegments;
	
	TUint8 *iLastSegment; // convenience pointer, do not delete ("not owned")
};




#endif // MODEL_H
