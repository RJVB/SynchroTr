#import <Foundation/Foundation.h>

#import "CritSectEx.h"
#import <stdio.h>

@implementation NSCriticalSectionScope

- (id) init
{
	if( (self = [super init]) ){
		scope = NULL;
	}
	return self;
}

- (void) dealloc
{ CritSectExScope *s = scope;
	scope = NULL;
	if( s ){
//		fprintf( stderr, "destroying lock scope %p::%p\n", [criticalSection cse], s );
		delete s;
		if( criticalSection->scopedLocks >= 1 ){
			criticalSection->scopedLocks -= 1;
		}
		[criticalSection release];
	}
	[super dealloc];
}

- (CritSectExScope*) setScopeForCS:(NSCriticalSection*)cs withTimeOut:(DWORD)timeOut
{ CritSectEx *cse = [cs cse];
	if( (scope = new CritSectExScope(cse, timeOut)) ){
		criticalSection = [cs retain];
		cs->scopedLocks += 1;
	}
	return scope;
}

- (BOOL) hasTimedOut
{
	return scope->TimedOut();
}

- (BOOL) isLocked
{
	return scope->IsLocked();
}

@end

@interface NSCriticalSectionScope (init)
+ (id) scopeForCriticalSection:(NSCriticalSection*)cse;
+ (id) scopeForCriticalSection:(NSCriticalSection*)cse withTimeOut:(DWORD)timeOut;
@end

@implementation NSCriticalSectionScope (init)

+ (id) scopeForCriticalSection:(NSCriticalSection*) cse
{
	return [self scopeForCriticalSection:cse withTimeOut:INFINITE];
}

+ (id) scopeForCriticalSection:(NSCriticalSection*)cse withTimeOut:(DWORD)timeOut
{
	if( cse ){
	  NSCriticalSectionScope *it = [[self alloc] init];
		if( it ){
			if( [it setScopeForCS:cse withTimeOut:timeOut] ){
				return it;//[it autorelease];
			}
			else{
				[it release];
				[self release];
				return nil;
			}
		}
		else{
			[self release];
		}
	}
	else{
		return nil;
	}
	return self;
}

@end


@implementation NSCriticalSection

- (id) init
{
	return [self initWithSpinMax:0];
}

- (id) initWithSpinMax:(DWORD)sm
{
	if( (self = [super init]) ){
		cse = new CritSectEx(sm);
	}
	if( cse ){
		cse->AllocateKernelSemaphore();
		spinMax = sm;
		info = [[NSString alloc] initWithUTF8String:typeid(CritSectEx).name()];
		scopedLocks = 0;
		return self;
	}
	else{
		[self release];
		return nil;
	}
}

+ (id) createWithSpinMax:(DWORD)sm
{ NSCriticalSection *it = [self alloc];
	return [[it initWithSpinMax:sm] autorelease];
}

- (void) dealloc
{ CritSectEx *c = cse;
	cse = NULL;
	[info release];
	if( !scopedLocks ){
//		fprintf( stderr, "Destroying CritSectEx %p\n", c );
		delete c;
	}
	else{
		fprintf( stderr, "Releasing NSCriticalSection %p but leaving its CritSectEx %p around!\n", self, c );
	}
	[super dealloc];
}

- (BOOL) hasTimedOut
{
	return cse->TimedOut();
}

- (BOOL) isLocked
{
	return cse->IsLocked();
}

- (NSCriticalSectionScope*) getLockScope
{
	return [NSCriticalSectionScope scopeForCriticalSection:self];
}

- (NSCriticalSectionScope*) getLockScopeWithTimeOut:(DWORD)timeOut
{
	return [NSCriticalSectionScope scopeForCriticalSection:self withTimeOut:timeOut];
}

#if __has_feature(blocks)
- (void) callLockedBlock:(LockedBlock) block
{ 
	{ CritSectExScope scope(cse);
		scopedLocks += 1;
		block();
	}
	scopedLocks -= 1;
}

- (void) callLockedBlock:(LockedBlock) block withTimeOut:(DWORD) timeOut
{
	{ CritSectExScope scope(cse, timeOut);
		scopedLocks += 1;
		block();
	}
	scopedLocks -= 1;
}
#endif

- (NSString*) description
{
	if( cse->IsLocked() ){
		return [[NSString alloc] initWithFormat:@"<%@ '%@', spinMax=%lu, locked, %lu scoped lock(s)>",
			NSStringFromClass([self class]), info, spinMax, scopedLocks ];
	}
	else{
		return [[NSString alloc] initWithFormat:@"<%@ '%@', spinMax=%lu, unlocked>",
			NSStringFromClass([self class]), info, spinMax ];
	}
}

- (void) setSpinMax:(DWORD)sm
{
	if( sm != cse->SpinMax() ){
		cse->SetSpinMax(sm);
		spinMax = sm;
	}
}

@synthesize spinMax;
@synthesize info;
@synthesize cse;
@end
