/*
 * ChildrenArray.inl
 *
 *  Created on: 7.6.2009
 *      Author: Florin
 */

#ifndef CHILDRENARRAY_INL_
#define CHILDRENARRAY_INL_
#include "MLauncher.pan"
#include "TrackInfo.h"
///////////////////////////////////////////////////////////////////////////////////////

/*
template<class T> void TChildrenArray<T>::Initialize()
{
	iNrChildren=0;
	iNext=NULL;
}

template<class T> void TChildrenArray<T>::Append(T *aChild,CFileDirPool *aPool)
{
	if(iNrChildren<KChildrenGranularity)
	{
		//we have enough place in this struct
		iChildren[iNrChildren]=aChild;
		iNrChildren++;
	}
	else
	{
		//we need to put this into next structure
		if(!iNext)
		{
			//but first, we need to allocate iNext
			iNext=(TChildrenArray<T>*)aPool->GetNewSpace(sizeof(TChildrenArray<T>));
			iNext->Initialize();
		};
		iNext->Append(aChild,aPool);
		iNrChildren++;
	};
}


template<class T> void TChildrenArray<T>::InsertIntoPosition(T *aChild,CFileDirPool *aPool,TInt aPosition)
{
	//__ASSERT_ALWAYS(aPosition<iNrChildren,Panic(EIndexTooBig3));
	if(aPosition>=KChildrenGranularity)
	{
		//insert into next children
		if(!iNext)
		{
			//but first, we need to allocate iNext
			iNext=(TChildrenArray<T>*)aPool->GetNewSpace(sizeof(TChildrenArray<T>));
			iNext->Initialize();
		};
		iNext->InsertIntoPosition(aChild,aPool,aPosition-KChildrenGranularity);
		iNrChildren++;
	}
	else
	{
		//insert here
		T *child(NULL);
		TInt i;
		TInt nrChildren2Shift;

		if(iNrChildren<KChildrenGranularity)
			nrChildren2Shift=iNrChildren;
		else 
		{
			nrChildren2Shift=KChildrenGranularity-1;
			child=iChildren[KChildrenGranularity-1];
		};

		//shift children
		for(i=nrChildren2Shift;i>aPosition;i--)
			iChildren[i]=iChildren[i-1];
		iChildren[aPosition]=aChild;

		if(child)
		{
			//we need to put this into next structure
			if(!iNext)
			{
				//but first, we need to allocate iNext
				iNext=(TChildrenArray<T>*)aPool->GetNewSpace(sizeof(TChildrenArray<T>));
				iNext->Initialize();
			};
			iNext->InsertIntoPosition(child,aPool,0);
		};
		iNrChildren++;
	}
}

template<class T> TInt TChildrenArray<T>::Delete(T *aChild)
{
	TInt i;
	TInt nrChildren(iNrChildren<KChildrenGranularity?iNrChildren:KChildrenGranularity);
	for(i=0;i<nrChildren;i++)
		if(iChildren[i]==aChild)
		{
			Delete(i);
			return ETrue;
		};
	if(iNrChildren>KChildrenGranularity)
	{
		TBool deleted=iNext->Delete(aChild);
		if(deleted)
			iNrChildren--;
		return deleted;
	}
	else return EFalse; //not deleted
}

template<class T> void TChildrenArray<T>::Delete(TInt aIndex)
{
	__ASSERT_ALWAYS(aIndex<iNrChildren,Panic(EIndexTooBig));
	if(aIndex<KChildrenGranularity)
	{	
		//the index is in this element and there are no children in the next element
		iChildren[aIndex]->iParent=NULL;
		//shift children
		TInt i;
		TInt nrChildren(iNrChildren<KChildrenGranularity?iNrChildren:KChildrenGranularity);
		for(i=aIndex;i<nrChildren-1;i++)
			iChildren[i]=iChildren[i+1];
		if(iNrChildren>KChildrenGranularity)
			iChildren[nrChildren-1]=iNext->ShiftChildren();
		iNrChildren--;
		//ATTENTION: iNext may not be null, but it may be unused
	}
	else
	{
		__ASSERT_ALWAYS(iNext,Panic(EiNExtIsNULL));
		iNext->Delete(aIndex-KChildrenGranularity);
		iNrChildren--;
	};
}

template<class T> T *TChildrenArray<T>::ShiftChildren()
{
	T *firstChild(iChildren[0]);
	TInt nrChildren(iNrChildren>KChildrenGranularity?KChildrenGranularity:iNrChildren);
	TInt i;

	for(i=0;i<nrChildren-1;i++)
		iChildren[i]=iChildren[i+1];
	if(iNrChildren>KChildrenGranularity)
		iChildren[KChildrenGranularity-1]=iNext->ShiftChildren();
	iNrChildren--;
	return firstChild;
}

template<class T> void TChildrenArray<T>::MarkAll(TBool aMark)
{
	TInt nrChildren(iNrChildren>KChildrenGranularity?KChildrenGranularity:iNrChildren);
	TInt i;

	for(i=0;i<nrChildren-1;i++)
		if(aMark)iChildren[i]->iFlags|=T::EMarked;
		else iChildren[i]->iFlags&=~T::EMarked;
	if(iNrChildren>KChildrenGranularity)
		iNext->MarkAll(aMark);
}

template<class T> T &TChildrenArray<T>::operator[](TInt anIndex)
{
	__ASSERT_ALWAYS(anIndex<iNrChildren,Panic(EIndexTooBig2));
	LOG(ELogNewSources,0,"Child %d (%d) iNext=%x",anIndex,KChildrenGranularity,iNext);
	if(anIndex<KChildrenGranularity)
	{
		//the child is in this struct
		return *(iChildren[anIndex]);
	}
	else
		return (*iNext)[anIndex-KChildrenGranularity];
}
*/

template<class T> T &TChildrenArray<T>::operator[](TInt anIndex)
{
	if(anIndex<iNrChildren)
	{
		__ASSERT_ALWAYS(iChildren[anIndex],Panic(EChildIsNull));
		return *(iChildren[anIndex]);
	}
	else
	{
		__ASSERT_ALWAYS(iNext,Panic(EIndexTooBig2));
		return (*iNext)[anIndex-iNrChildren];
	}
}
template<class T> void TChildrenArray<T>::Initialize(TInt aNrChildren, CFileDirPool *aPool)
{
	iNrAllocatedChildren=aNrChildren;
	iNrChildren=0;
	if(iNrAllocatedChildren>0)
	{
		iChildren=(T**)aPool->GetNewSpace(iNrAllocatedChildren*sizeof(T*));
		memset(iChildren,0,iNrAllocatedChildren*sizeof(T*));
	}
	else iChildren=NULL;
	iNext=NULL;
}

template<class T> void TChildrenArray<T>::AddMoreChildren(TInt aNrChildren, CFileDirPool *aPool)
{
	if(iNext)iNext->AddMoreChildren(aNrChildren,aPool);
	else
	{
		iNext=(TChildrenArray<T>*)aPool->GetNewSpace(sizeof(TChildrenArray<T>));
		iNext->Initialize(aNrChildren,aPool);
	}
}

template<class T> TInt TChildrenArray<T>::ChildrenCount()
{
	if(iNext)return iNrChildren+iNext->ChildrenCount();
	else return iNrChildren;
}

template<class T> void TChildrenArray<T>::MarkAsDeleted(T *aChild)
{
	//this function is not extremely efficient
	TInt i;
	TBool aFound(EFalse);
	for(i=0;i<iNrChildren;i++)
	{
		if(aChild==iChildren[i])
		{
			aChild->iFlags|=T::EDeleted;//mark the element as deleted
			aFound=ETrue;
			continue;
		}
		if(aFound)
			iChildren[i-1]=iChildren[i];
	}
	if(aFound)
	{
		iNrChildren--;
		iChildren[iNrChildren]=aChild;
	}
	else
	{
		__ASSERT_ALWAYS(iNext,Panic(EIndexTooBig3));
		iNext->MarkAsDeleted(aChild);
	}
}

template<class T> void TChildrenArray<T>::MarkAsDeleted(TInt aIndex)
{
	if(aIndex>=iNrChildren)
	{
		__ASSERT_ALWAYS(iNext,Panic(EIndexTooBig4));
		iNext->MarkAsDeleted(aIndex-iNrChildren);
		return;
	}
	
	iChildren[aIndex]->iFlags|=T::EDeleted;//mark the element as deleted
	T *child=iChildren[aIndex];
	
	
	TInt i;
	for(i=aIndex+1;i<iNrChildren;i++)
		iChildren[i-1]=iChildren[i];
	
	iNrChildren--;
	iChildren[iNrChildren]=child;
}

template<class T> void TChildrenArray<T>::MarkAllAsDeleted()
{
	TInt i;
	for(i=0;i<iNrChildren;i++)
	{
		iChildren[i]->iFlags|=T::EDeleted;
		if(iChildren[i]->iChildren)
			iChildren[i]->iChildren->MarkAllAsDeleted();
	}
	iNrChildren=0;
	if(iNext)
		iNext->MarkAllAsDeleted();
}

template<class T> void TChildrenArray<T>::UnmarkAllAsDeleted()
{
	//unmark all elements as deleted
	TInt i;
	for(i=0;i<iNrAllocatedChildren;i++)
		if(iChildren[i])
		{
			iChildren[i]->iFlags&=~T::EDeleted;
			if(iChildren[i]->iChildren)
				iChildren[i]->iChildren->UnmarkAllAsDeleted();
		}
		else break;
	if(i<iNrAllocatedChildren)iNrChildren=i;
	else iNrChildren=iNrAllocatedChildren;
	if(iNext)
		iNext->UnmarkAllAsDeleted();
}

template<class T> void TChildrenArray<T>::Append(T *aChild,CFileDirPool *aPool, TInt aAllocateIfNoSpace/*=1*/)
{
	//LOG0("Append++ (this=%x)",this);
	if(iNext)
	{
		iNext->Append(aChild,aPool,aAllocateIfNoSpace);
		//LOG0("Append-- (was appended to iNext)");
		return;
	}
	//check if any of the existing children is NULL
	if(iNrChildren<iNrAllocatedChildren)		
	{
		TInt i;
		for(i=iNrChildren;i<iNrAllocatedChildren;i++)
			if(!iChildren[i])
			{
				//found it!
				iChildren[i]=aChild;
				iNrChildren++;
				//LOG0("Append-- (to this, iNrChildren=%d)",iNrChildren);
				return;
			}
	}
	//LOG0("No space in this array");
	//if we are here, we did not find it!
	if(!iNext)
	{
		__ASSERT_ALWAYS(aAllocateIfNoSpace,Panic(EBadCode2));
		//we need to create and initialize iNext
		iNext=(TChildrenArray<T>*)aPool->GetNewSpace(sizeof(TChildrenArray<T>));
		iNext->Initialize(aAllocateIfNoSpace,aPool);
	}
	iNext->Append(aChild,aPool,0);
	//LOG0("Append-- (end, was appended to iNext)");
}

template<class T> void TChildrenArray<T>::InsertIntoPosition(T *aChild,CFileDirPool *aPool,TInt aPosition, TInt aAllocateIfNoSpace/*=1*/)
{
	TInt i;
	
	if(aPosition<iNrChildren)
	{
		//we need to shift children	
		T *child;
		for(i=aPosition+1;i<iNrChildren;i++)
		{
			child=iChildren[i];
			iChildren[i]=iChildren[i-1];
			iChildren[i-1]=aChild;
			aChild=child;
		}
		__ASSERT_ALWAYS(aChild,Panic(EBadCode3));
		aPosition=iNrChildren; //this is where we need to place the new aChild
	}
	
	if(aPosition==iNrChildren && iNrChildren<iNrAllocatedChildren)
	{
		//we could place aChild in this array, if there is still space
		for(i=iNrChildren;i<iNrAllocatedChildren;i++)
			if(!iChildren[i])break;
		if(i<iNrAllocatedChildren)
		{
			//indeed, we can place aChild here. However, we would need to shift the potential deleted elements
			T *child;
			for(i=iNrChildren;i<iNrAllocatedChildren;i++)
			{
				child=iChildren[i];
				iChildren[i]=aChild;
				aChild=child;
				if(!aChild)break;
			}
			iNrChildren++;
			return;
		}
	}
	
	//if we are here, we either do not have place, or we cannot put the child into this array
	if(!iNext)
	{
		//we need to allocate iNext
		__ASSERT_ALWAYS(aAllocateIfNoSpace,Panic(EBadCode2));
		//we need to create and initialize iNext
		iNext=(TChildrenArray<T>*)aPool->GetNewSpace(sizeof(TChildrenArray<T>));
		iNext->Initialize(aAllocateIfNoSpace,aPool);
	}
	iNext->InsertIntoPosition(aChild,aPool,aPosition-iNrChildren,aAllocateIfNoSpace);
}

template<class T> void TChildrenArray<T>::ReviewChildren()
{
	//LOG0("ReviewChildren (this=%s, iNrChildren=%d, iNrAllocatedChildren=%d)", this, iNrChildren, iNrAllocatedChildren)
	//what we need to do is simple: put children in the beginning, deleted elements in the end
	RPointerArray<T> good(iNrAllocatedChildren), deleted(iNrAllocatedChildren);
	TInt i;
	for(i=0;i<iNrAllocatedChildren;i++)
		if(iChildren[i])
		{
			if(iChildren[i]->iFlags&T::EDeleted)deleted.Append(iChildren[i]);
			else good.Append(iChildren[i]);
		}
	//now put the elements back into our array
	//LOG0("good: %d, deleted: %d",good.Count(),deleted.Count());
	iNrChildren=good.Count();
	for(i=0;i<iNrChildren;i++)
		iChildren[i]=good[i];
	for(i=0;i<deleted.Count();i++)
		iChildren[i+iNrChildren]=deleted[i];
	//close
	good.Close();
	deleted.Close();
	if(iNext)
		iNext->ReviewChildren();
}

#endif /* CHILDRENARRAY_INL_ */
