/******************************************************************************

	File: Interprt.h

	Description:

	Specification of the Smalltalk Interpreter

******************************************************************************/
#pragma once

///////////////////////////////////
#include "ObjMem.h"
#include "OopQ.h"
#include <vector>
#include <fpieee.h>
#include "STExternal.h"
#include "STBehavior.h"
#include "STClassDesc.h"
#include "InterpRegisters.h"

#include "DolphinX.h"
#include "bytecdes.h"

using namespace ST;

///////////////////////////////////

#ifdef _DEBUG
	#define TRACEARG(x)	,(x)
	#define	TRACEPARM	,TRACEFLAG traceFlag
	#define TRACEDEFAULT	TRACEPARM=TraceOff
	#define CHECKREFERENCES	Interpreter::checkReferences(Interpreter::GetRegisters());
	#define CHECKREFSNOFIX \
	{\
		bool bAsyncGCDisabled = Interpreter::m_bAsyncGCDisabled;\
		Interpreter::m_bAsyncGCDisabled = true;\
		CHECKREFERENCES\
		Interpreter::m_bAsyncGCDisabled = bAsyncGCDisabled;\
	}
#else
	#define TRACEPARM
	#define TRACEARG(x)
	#define TRACEDEFAULT
	#define CHECKREFERENCES
	#define CHECKREFSNOFIX
#endif

// Entry point (from CPP) for invocation of assembler interpreter
// This is where the stack overflow handling is carried out

extern "C" void __cdecl byteCodeLoop();	// See byteasm.asm

typedef volatile LONG SHAREDLONG;

#define stackValue(offset) (*(Interpreter::m_registers.m_stackPointer - (offset)))
#define pop(number) (Interpreter::m_registers.m_stackPointer -=(number))
#define popStack()	(*Interpreter::m_registers.m_stackPointer--)
#define stackTop()	(*Interpreter::m_registers.m_stackPointer)

class Interpreter
{
	friend class ObjectMemory; 

public:
	#ifdef _DEBUG
		enum TRACEFLAG { TraceInherit, TraceOff, TraceForce };
	#endif

	static HRESULT initialize(const char* szFileName, LPVOID imageData, UINT imageSize, bool isDevSys);

	// Fire off the startup message
	static void sendStartup(LPCSTR szImagePath, DWORD dwArg);

	// Clear down current image, etc
	static void ShutDown();

	// Run the interpreter
	static void interpret();

	// Initialise
	static HRESULT initializeAfterLoad();

	// To allow the ObjectMemory to account for objects referenced from the VM we maintain an "Array"
	// to keep the ref. count on our behalf
	//
	enum { INITIALVMREFERENCES = 16 };

public:
	#if defined(_DEBUG)
		static void AddVMReference(Oop);
		static void AddVMReference(OTE*);
		static void RemoveVMReference(Oop);
		static void RemoveVMReference(OTE*);

		static void checkReferences(InterpreterRegisters&);

		// Only needed for non-Debug because of C++ walkback code
		static void WarningWithStackTrace(const char* warningCaption, StackFrame* pFrame=NULL);
		static void WarningWithStackTraceBody(const char* warningCaption, StackFrame* pFrame);
		static BOOL isCallbackFrame(Oop framePointer);
	#endif

	static void StackTraceOn(ostream& dc, StackFrame* pFrame=NULL, unsigned depth=10);
	static void DumpStack(ostream&, unsigned);
	static void DumpContext(EXCEPTION_POINTERS *pExceptionInfo, ostream& logStream);
	static void DumpContext(ostream& logStream);
	static std::string PrintString(Oop);
	
	#ifdef _DEBUG
		static void DumpOTEPoolStats();
		static void ReincrementVMReferences();
	#endif

	// Snapshotting
	//static BOOL SaveImageFile(const char* szFileName=0);

	// External interface
	static SymbolOTE* __stdcall NewSymbol(const char* name);
	
	// Private helpers
	
	static BytesOTE* __fastcall NewDWORD(DWORD dwValue, BehaviorOTE* classPointer);

	// Users of callback(), or any routine which invokes it (basically, anything which sends
	// a message into Smalltalk for evaluation), needs to be prepared to catch the SE code
	// for callback unwinds, and do the appropriate thing. If not caught at every callback
	// point, then must be caught at some major entry point from Smalltalk to prevent it
	// unwinding other callbacks (esp. window procedure entry points).
	static Oop	__stdcall callback(SymbolOTE* selector, unsigned argCount TRACEPARM) 
		/* throws  SE_VMCALLBACKUNWIND */;

	static LRESULT lResultFromOop(Oop objectPointer, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK DolphinWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK VMWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK DolphinDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK CbtFilterHook(int code, WPARAM wParam, LPARAM lParam);
	static void subclassWindow(OTE* window, HWND hWnd);

	static DWORD callbackResultFromOop(Oop objectPointer);

	enum
	{
		SyncMsg = WM_USER,
		SyncCallbackMsg,
		SyncVirtualMsg
	};
	static DWORD __stdcall GenericCallbackMain(SMALLINTEGER id, BYTE* lpArgs);
	static DWORD __stdcall GenericCallback(SMALLINTEGER id, BYTE* lpArgs);

	struct COMThunk
	{
		PROC*	vtbl;
		DWORD*	argSizes;
		DWORD	id;
		DWORD	subId;
	};

	static DWORD __fastcall VirtualCallback(SMALLINTEGER id, COMThunk** thisPtr);
	static DWORD __fastcall VirtualCallbackMain(SMALLINTEGER id, COMThunk** thisPtr);

	// CompiledMethod bytecode decoding (in decode.cpp)
	#if defined(_DEBUG)
		static const char* activeMethod();
		static void decodeMethod(CompiledMethod*, ostream* pstream=NULL);
		static void decodeMethodAt(CompiledMethod*, unsigned ip, ostream&);
		static void AppendAllInstVarNames(ClassDescriptionOTE* oteClass, std::vector<std::string>& instVarNames);
#endif

	// Contexts
	static StackFrame* activeFrame();
	static void __fastcall resizeActiveProcess();

	static void IncStackRefs(Oop* const sp);
	static void DecStackRefs(Oop* const sp);

	// Stack

	static void push(Oop object);					// Most general, push SmallInteger OR object
	static void pushNewObject(Oop);					// Push newly created objects (add to Zct)
	static void pushUnknown(Oop);					// Push an object that might be new, might be old
	static void pushBool(BOOL bValue);				// Push the appropriate Smalltalk boolean object
	static void pushSmallInteger(SMALLINTEGER n);	// Push a gtd SmallInteger (no overflow check)
	static void pushUnsigned32(DWORD value);
	static void pushSigned32(SDWORD value);
	static void pushUIntPtr(UINT_PTR value);
	static void pushIntPtr(INT_PTR value);
	static void push(LPCSTR psz);					// Push ANSI string
	static void push(LPCWSTR pwsz);					// Push Wide string
	static void pushNil();
	static void pushHandle(HANDLE h);
	static void push(double d);

	// To be used when pushing a known non-SmallInteger (skips the SmallInteger check)
	static void pushObject(OTE* object)
	{
		push(reinterpret_cast<Oop>(object));
	}

	// Push newly created objects (add to Zct)
	static void pushNewObject(OTE* object)
	{
		// Note that object is pushed on stack before being added to Zct, this means
		// that on ZCT overflow the reconciliation will work correctly
		pushObject(object);
		ObjectMemory::AddToZct(object);
	}

	static Oop popAndCountUp();
	
	// N.B. pop() merely adjusts the stackPointer, remember that
	// it is necessary to ensure that no ref. counted objects
	// remain above the stackPointer (this is why there's no unPop)
	static SMALLUNSIGNED indexOfSP(Oop* sp);

	// Method lookup
	static MethodOTE* __fastcall lookupMethod(BehaviorOTE* aClass, SymbolOTE* selector);
	static MethodOTE* __fastcall messageNotUnderstood(BehaviorOTE* aClass, const unsigned argCount);
	static void __fastcall createActualMessage(const unsigned argCount);

	//Misc

	// Garbage collection and Finalization/Bereavement Queue management
	static void syncGC(DWORD gcFlags);
	static void asyncGC(DWORD gcFlags);

#ifdef PROFILING
	static void StartProfiling();
	static void StopProfiling();
#endif

public:

	enum VMInterrupts { 
						VMI_TERMINATE = ObjectMemoryIntegerObjectOf(1),
						VMI_STACKOVERFLOW = ObjectMemoryIntegerObjectOf(2), 
						VMI_BREAKPOINT = ObjectMemoryIntegerObjectOf(3),
						VMI_SINGLESTEP = ObjectMemoryIntegerObjectOf(4),
						VMI_ACCESSVIOLATION = ObjectMemoryIntegerObjectOf(5), 
						VMI_IDLEPANIC = ObjectMemoryIntegerObjectOf(6),
						VMI_GENERIC = ObjectMemoryIntegerObjectOf(7),
						VMI_STARTED = ObjectMemoryIntegerObjectOf(8),
						VMI_KILL = ObjectMemoryIntegerObjectOf(9),
						VMI_FPFAULT = ObjectMemoryIntegerObjectOf(10),
						VMI_USERINTERRUPT = ObjectMemoryIntegerObjectOf(11),
						VMI_ZERODIVIDE = ObjectMemoryIntegerObjectOf(12),
						VMI_OTOVERFLOW = ObjectMemoryIntegerObjectOf(13),
						VMI_CONSTWRITE = ObjectMemoryIntegerObjectOf(14),
						// Miscellaneous exceptions
						VMI_EXCEPTION = ObjectMemoryIntegerObjectOf(15),
						VMI_FPSTACK = ObjectMemoryIntegerObjectOf(16),
						VMI_NOMEMORY = ObjectMemoryIntegerObjectOf(17),
						VMI_HOSPICECRISIS = ObjectMemoryIntegerObjectOf(18),
						VMI_BEREAVEDCRISIS = ObjectMemoryIntegerObjectOf(19),
						VMI_CRTFAULT = ObjectMemoryIntegerObjectOf(20)
						};

#ifdef _DEBUG
	static const char* InterruptNames[static_cast<int>(VMI_CRTFAULT) + 1];
#endif

	static bool __fastcall disableInterrupts(bool bDisable);

	static bool disableAsyncGC(bool bDisable);
	static void OnCompact();
	static void MarkRoots();

	// Clear down the object caches for VM alloc'd objects
	static void freePools();

	// All of these may throw SE_VMCALLBACKUNWIND, as well as stack/OT overflow access violations, and
	// so should be called within a callbackExceptionFilter() __try/__except block
	static Oop	__stdcall perform(Oop receiver, SymbolOTE* selector TRACEDEFAULT);
	static Oop	__stdcall performWith(Oop receiver, SymbolOTE* selector, Oop arg TRACEDEFAULT);
	static Oop	__stdcall performWithWith(Oop receiver, SymbolOTE* selector, Oop arg1, Oop arg2 TRACEDEFAULT);
	static Oop	__stdcall performWithWithWith(Oop receiver, SymbolOTE* selector, Oop arg1, Oop arg2, Oop arg3 TRACEDEFAULT);
	static Oop	__stdcall performWithArguments(Oop receiver, SymbolOTE* selector, Oop argArray TRACEDEFAULT);

	// Exception filter for use with above methods - catches callback unwinds and activates handler (also
	// memory exceptions from pushing parms etc on the stack)
	static int __stdcall callbackExceptionFilter(LPEXCEPTION_POINTERS info);

	static void __fastcall activateNewMethod(CompiledMethod* methodPointer);

private:

	//////////////////////////////////////////////////////////
	// Private initialisation/termination

	static HRESULT initializeBeforeLoad();
	static HRESULT initializeImage();
	static void initializeVMReferences();

	static void GuiShutdown();

	// Timer init and shutdown
	static HRESULT initializeTimer();
	static void terminateTimer();

private:
	///////////////////////////////////////////////////////////////////////////
	// Byte Code Interpretation methods
	
	static void exitSmalltalk(int exitCode);

	static int interpreterExceptionFilter(LPEXCEPTION_POINTERS info);
	static int memoryExceptionFilter(LPEXCEPTION_POINTERS pExInfo);
	static int callbackTerminationFilter(LPEXCEPTION_POINTERS info, Process* callbackProcess, Oop prevCallbackContext);

	static void recoverFromFault(LPEXCEPTION_POINTERS pExRec);
	static void sendExceptionInterrupt(Oop oopInterrupt, LPEXCEPTION_POINTERS pExRec);
	static void saveContextAfterFault(LPEXCEPTION_POINTERS info, bool isInPrimitive);

	static void wakePendingCallbacks();
	static unsigned countPendingCallbacks();

	static void sendSelectorArgumentCount(SymbolOTE* selector, unsigned count);
	static void sendSelectorToClass(BehaviorOTE* classPointer, unsigned argCount);
	static void sendVMInterrupt(ProcessOTE* processPointer, Oop nInterrupt, Oop argPointer);
	static void __fastcall sendVMInterrupt(Oop nInterrupt, Oop argPointer);
	static MethodOTE* __fastcall findNewMethodInClass(BehaviorOTE* classPointer, const unsigned argCount);
	static MethodOTE* __stdcall findNewMethodInClassNoCache(BehaviorOTE* classPointer, const unsigned argCount);

	static BOOL __stdcall MsgSendPoll();
	static BOOL	__stdcall BytecodePoll();
	static BOOL sampleInput();
	static bool __stdcall IsUserBreakRequested();
	
	static void __fastcall executeNewMethod(MethodOTE* methodOTE, unsigned argCount);
	static void __fastcall returnValueTo(Oop resultPointer, Oop contextPointer);
	static void __fastcall returnValueToCaller(Oop resultPointer, Oop contextPointer);
	static void __fastcall nonLocalReturnValueTo(Oop resultPointer, Oop contextPointer);
	static void __fastcall invalidReturn(Oop resultPointer);

	static BlockOTE* __fastcall blockCopy(DWORD ext);

public:
	static void basicQueueForFinalization(OTE* ote);
	static void queueForFinalization(OTE* ote, int);
	static void queueForBereavementOf(OTE* ote, Oop argPointer);
	// Number of entries in the bereavement queue per object (one for the object, the other the loss count)
	enum { OopsPerBereavementQEntry = 2 };

	// Queue a process interrupt to be executed at the earliest opportunity
	static void __stdcall queueInterrupt(ProcessOTE* processPointer, Oop nInterrupt, Oop argPointer);
	static void __stdcall queueInterrupt(Oop nInterrupt, Oop argPointer);

	// Queue up a semaphore signal to be performed in sync with byte code execution
	// at the next possible opportunity. Used from interrupts and from external sources
	static void asynchronousSignal(SemaphoreOTE* aSemaphore);
	static void asynchronousSignalNoProtect(SemaphoreOTE* aSemaphore);

	static InterpreterRegisters& GetRegisters();

	static bool IsShuttingDown();
	static DWORD MainThreadId();
	static HANDLE MainThreadHandle();

private:

	// Primitive helpers
	static OTE* __fastcall dequeueForFinalization();
	static OTE* dequeueBereaved(ST::VariantObject* out);
	static void scheduleFinalization();

	#ifdef INLINEMEMFNS
		static BOOL success();
		static BOOL setSuccess(BOOL bValue);
		static BOOL primitiveFail();
	#endif

	static void CALLBACK TimeProc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);

	// Signal a semaphore; synchronously if no interrupts are pending, else asynchronously.
	// May initiate a Process switch, but does not perform the actual context switch
	// This one is used when in sync with byte code execution
	static void signalSemaphore(SemaphoreOTE* semaphorePointer);

public:
	static void GrabAsyncProtect();
	static void RelinquishAsyncProtect();
	static void NotifyAsyncPending();
	static bool QueueAPC(PAPCFUNC pfnAPC, DWORD dwClosure);
	static void BeginAPC();
	static BOOL SetWakeupEvent();
	static void NotifyOTOverflow();

	static BOOL FastYield();
	static void sleep(ProcessOTE* aProcess);
	static int SuspendProcess(ProcessOTE* oteProc);
	static void QueueProcessOn(ProcessOTE* oteProc, LinkedListOTE* oteList);
	static BOOL __stdcall Reschedule();

	// Return the active process to a list on which it was previously
	// suspended - this may (if the list is a Semaphore) actually
	// leave the process in a runnable condition.
	static LinkedListOTE* __fastcall ResuspendActiveOn(LinkedListOTE* oteList);
	static LinkedListOTE* ResuspendProcessOn(ProcessOTE* oteProcess, LinkedListOTE* oteList);

	static ProcessOTE* schedule();
	static ProcessOTE* resume(ProcessOTE* aProcess);

	// Accessors to retrieve ProcessorScheduler singleton instance
	static ProcessorScheduler* scheduler();
	static SchedulerOTE* schedulerPointer();

	// Accessors to retrieve active Process (which may be waiting to be started)
	static ProcessOTE* activeProcessPointer();
	static Process* activeProcess();

	// Accessors to retrieve actual active Process (which may be waiting to be suspended)
	static ProcessOTE* actualActiveProcessPointer();
	static Process* actualActiveProcess();

private:
	static void ResetFP();

	static SemaphoreOTE* pendingCallbacksPointer();
	static Semaphore* pendingCallbacks();

	static StackFrame* firstFrame();

	static ProcessOTE* wakeHighestPriority();
	static ProcessOTE* resumeFirst(LinkedList* list);
	static ProcessOTE* resumeFirst(Semaphore* sem);
	static BOOL __fastcall yield();
	static BOOL	__fastcall FireAsyncEvents();
	static BOOL __fastcall CheckProcessSwitch();
	static void switchTo(ProcessOTE* processPointer, bool bInitFP=true);
	
	#ifdef _DEBUG
		static int highestWaitingPriority();
	#endif

	static bool TerminateOverlapped(ProcessOTE* oteProc);

	static BOOL isAFloat(Oop objectPointer);

private:
	static HANDLE	m_hSampleTimer;

	static HRESULT InitializeSampler();
	static void TerminateSampler();
	static HRESULT SetSampleTimer(SMALLINTEGER newInterval);
	static void CancelSampleTimer();
	static VOID CALLBACK SamplerProc(PVOID lpParam, BOOLEAN TimerOrWaitFired);
	static void ResetInputPollCounter();
	static void AbandonStepping();

public:
	///////////////////////////////////////////////////////////////////////////
	// Primitive Methods
	// Note: All primitives (EXCEPT THOSE INVOKED BY SPECIAL
	//		SELECTORS) are actually passed a reference to the
	//		method from which they are being invoked and the
	// 		argumentCount of that method, but only a very few of
	//		them actually need to look at these values (passed in
	//		the registers ECX and EDX respectively). In the case
	//		of primitives invoked by special selectors, then only
	//		the argumentCount can be relied upon.

	static Oop* primitiveFailure(int failureCode);
	static Oop* primitiveFailureWith(int failureCode, Oop failureOop);
	static Oop* primitiveFailureWith(int failureCode, OTE* failureObject);
	static Oop* primitiveFailureWithInt(int failureCode, SMALLINTEGER failureInt);

private:
	// Answer whether the Interpreter is currently executing a primitive method
	static bool isInPrimitive();

	// SmallInteger Arithmetic
	static Oop* __fastcall primitiveAdd();
	static Oop* __fastcall primitiveSubtract();
	static Oop* __fastcall primitiveMultiply();
	static Oop* __fastcall primitiveDivide();
	static Oop* __fastcall primitiveDiv();
	static Oop* __fastcall primitiveMod();
	static Oop* __fastcall primitiveQuo();

	// SmallInteger relational ops
	static Oop* __fastcall primitiveEqual();
	//static Oop* __fastcall primitiveNotEqual();		Removed as redundant
	static Oop* __fastcall primitiveLessThan();
	static Oop* __fastcall primitiveLessOrEqual();
	static Oop* __fastcall primitiveGreaterThan();
	static Oop* __fastcall primitiveGreaterOrEqual();

	// SmallInteger bit manipulation
	static Oop* __fastcall primitiveBitAnd();
	static Oop* __fastcall primitiveBitOr();
	static Oop* __fastcall primitiveBitXor();
	static Oop* __fastcall primitiveBitShift();

	static Oop* __fastcall primitiveSmallIntegerPrintString();

	// LargeInteger Arithmetic
	static Oop* __fastcall primitiveLargeIntegerAdd();
	static Oop* __fastcall primitiveLargeIntegerSubtract();
	static Oop* __fastcall primitiveLargeIntegerMultiply();
	static Oop* __fastcall primitiveLargeIntegerDivide();
	static Oop* __fastcall primitiveLargeIntegerDiv();
	static Oop* __fastcall primitiveLargeIntegerMod();
	static Oop* __fastcall primitiveLargeIntegerQuoAndRem();

	// LargeInteger relational ops
	static Oop* __fastcall primitiveLargeIntegerEqual();
	static Oop* __fastcall primitiveLargeIntegerLessThan();
	static Oop* __fastcall primitiveLargeIntegerLessOrEqual();
	static Oop* __fastcall primitiveLargeIntegerGreaterThan();
	static Oop* __fastcall primitiveLargeIntegerGreaterOrEqual();

	// LargeInteger bit manipulation
	static Oop* __fastcall primitiveLargeIntegerBitAnd();
	static Oop* __fastcall primitiveLargeIntegerBitOr();
	static Oop* __fastcall primitiveLargeIntegerBitXor();
	static Oop* __fastcall primitiveLargeIntegerBitShift();

	// LargeInteger miscellaneous
	static Oop* __fastcall primitiveLargeIntegerNormalize();
	static Oop* __fastcall primitiveLargeIntegerAsFloat();

	// Float primitives
	static Oop* __fastcall primitiveAsFloat();
	static Oop* __fastcall primitiveFloatAdd();
	static Oop* __fastcall primitiveFloatSubtract();
	static Oop* __fastcall primitiveFloatLessThan();
	static Oop* __fastcall primitiveFloatGreaterThan();
	static Oop* __fastcall primitiveFloatEqual();
	static Oop* __fastcall primitiveFloatMultiply();
	static Oop* __fastcall primitiveFloatDivide();
	static Oop* __fastcall primitiveTruncated();

	static Oop* __fastcall primitiveMakePoint(CompiledMethod& , unsigned argCount);

	static Oop* __fastcall primitiveSize();
   	
	// Object Indexing Primitives
	static Oop* __fastcall primitiveAtPut();
	static Oop* __fastcall primitiveInstVarAt();
	static Oop* __fastcall primitiveInstVarAtPut();
	static Oop* __fastcall primitiveNextIndexOfFromTo();

	// Specialized primitive for storing into process stacks. Allows for Zct
	static Oop* __fastcall primitiveStackAtPut(CompiledMethod& , unsigned argCount);

	///////////////////////////////////////////////////////////////////////////
	// External Buffer access primitives

	enum { PrimitiveFailureNonInteger, PrimitiveFailureBoundsError, PrimitiveFailureBadValue, PrimitiveFailureSystemError, PrimitiveFailureWrongNumberOfArgs };

	#ifndef _M_IX86
		static Oop* __fastcall primitiveDWORDAt();
		static Oop* __fastcall primitiveDWORDAtPut();
		static Oop* __fastcall primitiveSDWORDAt();
		static Oop* __fastcall primitiveSDWORDAtPut();	// Not implemented

		static Oop* __fastcall primitiveWORDAt();
		static Oop* __fastcall primitiveWORDAtPut();
		static Oop* __fastcall primitiveSWORDAt();
		static Oop* __fastcall primitiveSWORDAtPut();
	#endif

	// Floating point number accessors
	static Oop* __fastcall primitiveSinglePrecisionFloatAt();
	static Oop* __fastcall primitiveSinglePrecisionFloatAtPut();
	static Oop* __fastcall primitiveDoublePrecisionFloatAt();
	static Oop* __fastcall primitiveDoublePrecisionFloatAtPut();
	static Oop* __fastcall primitiveLongDoubleAt();

	// Most of the External Buffer primitives handle indirect addresses
	// as well, but bytes are normally accessed via the standard primitives 
	// for #at: and #at:put:, which we'd rather not complicate and/or slow 
	// these down.
	static Oop* __fastcall primitiveByteAtAddress();
	static Oop* __fastcall primitiveByteAtAddressPut();

	// Get address of contents of a byte object
	static Oop* __fastcall primitiveAddressOf();

	///////////////////////////////////////////////////////////////////////////
	// String Class Primitives
	static Oop* __fastcall primitiveStringAt();
	static Oop* __fastcall primitiveStringAtPut();

	// Helper for memory moves
	static void memmove(BYTE* dst, const BYTE* src, size_t count);
	static Oop* __fastcall primitiveStringReplace();
	static Oop* __fastcall primitiveReplaceBytes();
	static Oop* __fastcall primitiveIndirectReplaceBytes();
	static Oop* __fastcall primitiveReplacePointers();

	static Oop* __fastcall primitiveHashBytes();
	static Oop* __fastcall primitiveStringCompare();
	static Oop* __fastcall primitiveStringLessOrEqual();
	static Oop* __fastcall primitiveStringSearch();
	static Oop* __fastcall primitiveStringNextIndexOfFromTo();

	// Stream Primitives
	static Oop* __fastcall primitiveNext();
	static Oop* __fastcall primitiveNextSDWORD();
	static Oop* __fastcall primitiveNextPut();
	static Oop* __fastcall primitiveNextPutAll();
	static Oop* __fastcall primitiveAtEnd();

	// Storage Management Primitives
	static Oop* __fastcall primitiveNew();
	static Oop* __fastcall primitiveNewWithArg();

	// Object mutation
	static Oop* __fastcall primitiveChangeBehavior();
	static Oop* __fastcall primitiveResize();
	//static Oop* __fastcall primitiveBecome();

	// Object Memory primitives
	static Oop* __fastcall primitiveBasicIdentityHash();
	static Oop* __fastcall primitiveIdentityHash();
	static Oop* __fastcall primitiveAllReferences(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveAllInstances();
	
	// Control Primitives
	
	static Oop* __fastcall primitiveValue(CompiledMethod* , unsigned argumentCount);
	static Oop* __fastcall primitiveValueWithArgs();
	static Oop* __fastcall primitivePerform(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitivePerformWithArgs();
	static Oop* __fastcall primitivePerformMethod(CompiledMethod&, unsigned argumentCount);

	// Process primitives
	static Oop* __fastcall primitiveSignalAtTick(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveMicrosecondClockValue();
	static Oop* __fastcall primitiveSignal(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveWait(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveResume(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveSingleStep(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveSuspend();
	static Oop* __fastcall primitiveSetSignals();
	static Oop* __fastcall primitiveFlushCache();
	static Oop* __fastcall primitiveInputSemaphore(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveSampleInterval();
	static Oop* __fastcall primitiveNewVirtual();
	static Oop* __fastcall primitiveProcessPriority();
	static Oop* __fastcall primitiveTerminateProcess();
	static Oop* __fastcall primitiveMillisecondClockValue();

	// Input/Out Primitives
	static Oop* __fastcall primitiveSnapshot(CompiledMethod& , unsigned argCount);

	// Dispatcher Primitives
	static Oop* __fastcall primitiveHookWindowCreate();

	// System Primitives
	static Oop* __fastcall primitiveEquivalent();
	static Oop* __fastcall primitiveClass();
	static Oop* __fastcall primitiveCoreLeft(CompiledMethod& , unsigned argCount);
	static void __fastcall primitiveQuit(CompiledMethod&, unsigned argumentCount);
	static Oop* __fastcall primitiveOopsLeft();
	static Oop* __fastcall primitiveInheritsFrom();
	static Oop* __fastcall primitiveShallowCopy();
	static Oop* __fastcall primitiveSetSpecialBehavior();
	static Oop* __fastcall primitiveQueueInterrupt();

	static Oop* __fastcall primitiveDeQBereavement();


	// Extension system primitives
	static Oop* __fastcall primitiveDLL32Call(CompiledMethod& method, unsigned argCount);
	static Oop* __fastcall primitiveVirtualCall(CompiledMethod& method, unsigned argCount);
	static Oop* __fastcall primitiveAsyncDLL32Call(CompiledMethod& method, unsigned argCount);

	static Oop* __fastcall primitivePerformWithArgsAt(CompiledMethod& method, unsigned argCount);
	static Oop* __fastcall primitiveValueWithArgsAt(CompiledMethod& method, unsigned argCount);

	static Oop* __fastcall primitiveUnwindInterrupt(CompiledMethod& method, unsigned argCount);
	static Oop* __fastcall primitiveVariantValue();

private:

	static BOOL __stdcall callExternalFunction(FARPROC pProc, unsigned argCount, DolphinX::CallDescriptor* argTypes, BOOL isVirtual);
	
	// Pushs object on stack instantiated from address, and returns size of object pushed
	static void pushArgsAt(CallbackDescriptor* descriptor, unsigned argCount, BYTE* lpParms);
	static unsigned pushArgsAt(const ExternalDescriptor* descriptor, BYTE* lpParms);
	
	static int __cdecl IEEEFPHandler(_FPIEEE_RECORD *pIEEEFPException);

	static void failTrace();

public:
	#ifdef _DEBUG
		// Execution trace
		static int executionTrace;
		static void __fastcall debugExecTrace(BYTE* ip, Oop* sp);
		static void __fastcall debugMethodActivated(Oop* sp);
		static void __fastcall debugReturnToMethod(Oop* sp);
		static void checkStack(Oop* sp);

		static void DumpMethodCacheStats();
		static void DumpAtCacheStats();
		static void DumpCacheStats();

	#endif

public:
	// Special Selector Table
	enum {NumSpecialSelectors = 32};

private:
	// Method cache is a hash table with overwrite on collision
	// If changing method cache size, then must also modify METHODCACHEWORDS in ISTASM.INC!
	enum { MethodCacheSize = 1024 };
	__declspec(align(16)) struct MethodCacheEntry 
	{
		// Note that the method cache must include the class, as if one tests against
		// the method's methodClass inherited methods will always result in a cache miss
		// (e.g. for #new:, which is deeply inherited by many classes such as Array)
		// The selector, on the other hand, can be tested against in the method itself.
		const SymbolOTE*	selector;
		const BehaviorOTE* 	classPointer;
		MethodOTE* 			method;
		DWORD				primAddress;
	};

	static MethodCacheEntry methodCache[MethodCacheSize];

	__declspec(align(16)) struct AtCacheEntry
	{
		OTE*	oteArray;
		MWORD	maxIndex;
		void*	pElements;
		int		type;
	};

	enum { AtCacheEntries = 16 };
	enum { AtCacheMask = (AtCacheEntries - 1)*16 };
	enum { AtCachePointers = 0, AtCacheBytes, AtCacheString };

	static AtCacheEntry AtCache[AtCacheEntries];
	static AtCacheEntry AtPutCache[AtCacheEntries];

	static void flushCaches();
	static void flushAtCaches();
	static void initializeCaches();
	static void purgeObjectFromCaches(OTE*);
	
	enum { FIXEDVMREFERENCES };
	enum { SIGNALQGROWTH=32, SIGNALQSIZE=64 };
	enum { INTERRUPTQGROWTH=8, INTERRUPTQSIZE=16 };
	enum { FINALIZEQSIZE = 128 };
	enum { FINALIZEQGROWTH = 128 };
	enum { BEREAVEMENTQSIZE = 64 };
	enum { BEREAVEMENTQGROWTH=64 };

private:
	// Critical section to protect the async queues
	static CRITICAL_SECTION m_csAsyncProtect;

public:
	// Pools
	enum { DWORDPOOL, FLOATPOOL, CONTEXTPOOL, BLOCKPOOL, NUMOTEPOOLS };
	static ObjectMemory::OTEPool m_otePools[NUMOTEPOOLS];
private:

	// Process related registers
	static BOOL newProcessWaiting();

private:
	// Dispatched message
	static HHOOK m_hHookOldCbtFilter;

	static ATOM	 m_atomVMWndClass;
	static HWND	 m_hWndVM;

	// Context related registers
	static DWORD				m_dwThreadId;	// Interpreter thread
	static HANDLE				m_hThread;
	static ProcessorScheduler*	m_pProcessor;
public:
	static InterpreterRegisters16 m_registers;

private:
	static SymbolOTE*			m_oopMessageSelector;

	// Should this be one of the registers?
	static ProcessOTE* m_oteNewProcess;

	static uint64_t m_clockFrequency;

	static OTE* m_oteUnderConstruction;				// Window currently under construction

	// Interpreter referenced objects (roots as may have no other refs)
	static OTE** m_roots[];

	// Process Switching - these are used with InterlockedExchange, so must be longs (not BYTE sized bool)
	static SHAREDLONG	m_bAsyncPending;			// Set when interrupts or async signals are pending.
	static SHAREDLONG	m_bAsyncPendingIOff;		// Ditto, used to buffer when interrupts are off
	static SHAREDLONG*	m_pbAsyncPending; 
	static SHAREDLONG	m_nAPCsPending;				// Count of APCs queued for the interpreter thread

	static BOOL		m_bStepping;					// Flag to indicate whether to break when input sampler drops to 0
	static bool		m_bInterruptsDisabled;			// Flag to indicate whether accepting interrupts or not
	static bool		m_bAsyncGCDisabled;			
	static bool		m_bShutDown;					// Is the interpreter shutting down?

	static unsigned m_nCallbacksPending;			// Number of failed callback exits pending
	static unsigned	m_nOTOverflows;

	// Circular queues to hold the semaphores and interrupts, etc
	static OopQueue<SemaphoreOTE*>	m_qAsyncSignals;
	static OopQueue<Oop>			m_qInterrupts;
	static OopQueue<OTE*>			m_qForFinalize;		// Queue of objects requiring finalization
	static OopQueue<Oop>			m_qBereavements;	// Queue of weakly referencing objects which have suffered bereavements

	// Win32 input queue management
	static SHAREDLONG	m_nInputPollCounter;			// When this goes to zero, its time to poll for input
	static LONG			m_nInputPollInterval;			// Poll counter reset to this after each message queue poll

	#if defined(_DEBUG)
		// List of VM referenced objects (generic mechanism for avoiding GC problems with
		// objects ref'd only from the VM)
		static Oop*	m_pVMRefs;
		static int	m_nFreeVMRef;
		static int	m_nMaxVMRefs;				// Current size of VM References array

		enum { VMREFSINITIAL = 16 };
		enum { VMREFSGROWTH = 16 };
	#endif
};

ostream& operator<<(ostream& stream, const CONTEXT* pCtx);

///////////////////////////////////

#include "Interprt.inl"
