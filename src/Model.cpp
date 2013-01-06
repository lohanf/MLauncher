/*
 ============================================================================
 Name		: Model.cpp
 Author	  : Florin Lohan
 Version	 : 1.0
 Copyright   : (C) 2011 Florin Lohan. All rights reserved.
 Description : CModel implementation
 ============================================================================
 */

#include "Model.h"
#include "MLauncher.pan"

const TInt KSegmentSize=65536; //so we can reffer it with 2 bytes

CModel::CModel()
{
	// No implementation required
}

CModel::~CModel()
{
	iSegments.ResetAndDestroy();
}


CModel* CModel::NewL(RFs &aFs)
{
	CModel* self = new (ELeave) CModel();
	CleanupStack::PushL(self);
	self->ConstructL(aFs);
	CleanupStack::Pop(); // self;
	return self;
}

void CModel::ConstructL(RFs &aFs)
{
	iFs=&aFs;
}

TUint8* CModel::ExpandL(TInt aSize, TMLAddr &aAddr)
{
	if(!iSegments.Count() || (aSize+ *(TUint16*)iLastSegment>KSegmentSize))
	{
		//we need to allocate a new segment & size
		iLastSegment=(TUint8*)User::AllocL(KSegmentSize);
		iSegments.AppendL(iLastSegment);
		//set segment size
		*(TUint16*)iLastSegment=sizeof(TUint16);
	};
	//we should have ebough memory in the last segment
	__ASSERT_DEBUG(aSize+ *(TUint16*)iLastSegment<=KSegmentSize,Panic(ENotEnoughMemory));
	TUint8 *ptr=iLastSegment+ *(TUint16*)iLastSegment;
	aAddr=((iSegments.Count()-1)<<16) + *(TUint16*)iLastSegment;
	*(TUint16*)iLastSegment += (TUint16)aSize;
	return ptr;
}

void CModel::GetMemoryInfo(TInt &aNrSegments, TInt &aLastSegmentFill)
{
	aNrSegments=iSegments.Count();
	aLastSegmentFill= *(TUint16*)iLastSegment;
}
