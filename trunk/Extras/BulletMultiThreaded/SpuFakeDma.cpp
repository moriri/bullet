
#include "SpuFakeDma.h"
#include <LinearMath/btScalar.h> //for btAssert
//Disabling memcpy sometimes helps debugging DMA

#define USE_MEMCPY 1
#ifdef USE_MEMCPY

#endif


void*	cellDmaLargeGetReadOnly(void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t tid, uint32_t rid)
{

#if defined (__CELLOS_LV2__) || defined (USE_LIBSPE2)
	cellDmaLargeGet(ls,ea,size,tag,tid,rid);
	return ls;
#else
	return (void*)(uint32_t)ea;
#endif
}

void*	cellDmaSmallGetReadOnly(void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t tid, uint32_t rid)
{
#if defined (__SPU__) || defined (USE_LIBSPE2)
	mfc_get(ls,ea,size,tag,0,0);
	return ls;
#else
	return (void*)(uint32_t)ea;
#endif
}




void*	cellDmaGetReadOnly(void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t tid, uint32_t rid)
{
#if defined (__SPU__) || defined (USE_LIBSPE2)
	cellDmaGet(ls,ea,size,tag,tid,rid);
	return ls;
#else
	return (void*)(uint32_t)ea;
#endif
}


///this unalignedDma should not be frequently used, only for small data. It handles alignment and performs check on size (<16 bytes)
int stallingUnalignedDmaSmallGet(void *ls, uint64_t ea, uint32_t size)
{
	
	btAssert(size<32);
	
	ATTRIBUTE_ALIGNED16(char	tmpBuffer[32]);

	char* mainMem = (char*)ea;
	char* localStore = (char*)ls;
	uint32_t i;
	

	///make sure last 4 bits are the same, for cellDmaSmallGet
	uint32_t last4BitsOffset = ea & 0x0f;
	char* tmpTarget = tmpBuffer + last4BitsOffset;
	
#if defined (__SPU__) || defined (USE_LIBSPE2)
	
	int remainingSize = size;

	//cellDmaUnalignedGet(tmpTarget,ea,size,DMA_TAG(1),0,0);

	char* remainingTmpTarget = tmpTarget;
	uint64_t remainingEa = ea;

	while (remainingSize)
	{
		switch (remainingSize)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
			{
				mfc_get(remainingTmpTarget,remainingEa,remainingSize,DMA_TAG(1),0,0);
				remainingSize=0;
				break;
			}
		default:
			{
				//spu_printf("unaligned DMA with non-natural size:%d\n",remainingSize);
				int actualSize = 0;

				if (remainingSize > 16)
					actualSize = 16;
				else
					if (remainingSize >8)
						actualSize=8;
					else
						if (remainingSize >4)
							actualSize=4;
						else
							if (remainingSize >2)
								actualSize=2;
				mfc_get(remainingTmpTarget,remainingEa,actualSize,DMA_TAG(1),0,0);
				remainingSize-=actualSize;
				remainingTmpTarget+=actualSize;
				remainingEa += actualSize;
			}
		}
	}

#else
	//copy into final destination
#ifdef USE_MEMCPY
		memcpy(tmpTarget,mainMem,size);
#else
		for ( i=0;i<size;i++)
		{
			tmpTarget[i] = mainMem[i];
		}
#endif //USE_MEMCPY

#endif

	cellDmaWaitTagStatusAll(DMA_MASK(1));

	//this is slowish, perhaps memcpy on SPU is smarter?
	for (i=0; btLikely( i<size );i++)
	{
		localStore[i] = tmpTarget[i];
	}

	return 0;
}

#if defined (__SPU__) || defined (USE_LIBSPE2)
#else

int	cellDmaLargeGet(void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t tid, uint32_t rid)
{
	char* mainMem = (char*)ea;
	char* localStore = (char*)ls;

#ifdef USE_MEMCPY
	memcpy(localStore,mainMem,size);
#else
	for (uint32_t i=0;i<size;i++)
	{
		localStore[i] = mainMem[i];
	}
#endif
	return 0;
}

int	cellDmaGet(void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t tid, uint32_t rid)
{
	char* mainMem = (char*)ea;
	char* localStore = (char*)ls;
#ifdef USE_MEMCPY
	memcpy(localStore,mainMem,size);
#else
	for (uint32_t i=0;i<size;i++)
	{
		localStore[i] = mainMem[i];
	}	
#endif //#ifdef USE_MEMCPY
	return 0;
}

int cellDmaLargePut(const void *ls, uint64_t ea, uint32_t size, uint32_t tag, uint32_t tid, uint32_t rid)
{
	char* mainMem = (char*)ea;
	const char* localStore = (const char*)ls;
#ifdef USE_MEMCPY
	memcpy(mainMem,localStore,size);
#else
	for (uint32_t i=0;i<size;i++)
	{
		mainMem[i] = localStore[i];
	}	
#endif //#ifdef USE_MEMCPY

	return 0;
}



void	cellDmaWaitTagStatusAll(int ignore)
{

}

#endif
