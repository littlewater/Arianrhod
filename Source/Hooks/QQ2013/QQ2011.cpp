#pragma comment(linker, "/ENTRY:DllMain")
#pragma comment(linker, "/SECTION:.text,ERW /MERGE:.rdata=.text /MERGE:.data=.text")
#pragma comment(linker, "/SECTION:.Asuna,ERW /MERGE:.text=.Asuna")
#pragma comment(linker, "/EXPORT:InitCommonControlsEx=COMCTL32.InitCommonControlsEx")

#include "QQ2011.h"
#include "MyLibrary.cpp"
#include <Psapi.h>

WCHAR GlobalRegistryDb[0x20];
WCHAR GlobalHistoryDb[countof(GlobalRegistryDb)];


TYPE_OF(&SetWindowPos)              StubSetWindowPos;
TYPE_OF(&GetModuleFileNameExW)      StubGetModuleFileNameExW;
TYPE_OF(&NtQueryInformationProcess) StubNtQueryInformationProcess;
TYPE_OF(&::CreateThread)            HummerCreateThread;
TYPE_OF(&NtOpenFile)                StubNtOpenFile;
TYPE_OF(&NtCreateFile)              StubNtCreateFile;
TYPE_OF(&NtQueryAttributesFile)     StubNtQueryAttributesFile;


NTSTATUS GetRedirectFile(PUNICODE_STRING Redirected, PUNICODE_STRING Original)
{
    ULONG_PTR       Length;
    PWSTR           Buffer;
    UNICODE_STRING  Registry;

    typedef struct
    {
        UNICODE_STRING  SubPath;
        ULONG_PTR       SuffixLength;
        PCWSTR          NewSubPath;

    } DB_REDIRECT, *PDB_REDIRECT;

    PDB_REDIRECT RegistryDb;

    static DB_REDIRECT RegistryDbPath[] =
    {
        { RTL_CONSTANT_STRING(L"\\All Users\\QQ\\History.db"),  CONST_STRLEN(L"History.db") * sizeof(WCHAR), GlobalHistoryDb },
        { RTL_CONSTANT_STRING(L"\\All Users\\QQ\\Registry.db"), CONST_STRLEN(L"Registry.db") * sizeof(WCHAR), GlobalRegistryDb },
    };

    RtlInitEmptyString(Redirected);

    LOOP_ONCE
    {
        if (Original == NULL)
            continue;

        FOR_EACH(RegistryDb, RegistryDbPath, countof(RegistryDbPath))
        {
            if (Original->Length <= RegistryDb->SubPath.Length)
                continue;

            Registry = *Original;
            Registry.Buffer = PtrSub(PtrAdd(Registry.Buffer, Registry.Length), RegistryDb->SubPath.Length);
            Registry.Length = RegistryDb->SubPath.Length;

            if (!RtlEqualUnicodeString(&Registry, &RegistryDb->SubPath, TRUE))
                continue;

            break;
        }

        if (RegistryDb == RegistryDbPath + countof(RegistryDbPath))
            break;

        Length = Original->Length + RegistryDb->SubPath.Length + RegistryDb->SuffixLength + sizeof(RegistryDb->NewSubPath);
        Buffer = (PWSTR)AllocStack(Length);

        Registry.MaximumLength = Length;
        Registry.Buffer = Buffer;

        Length = StrLengthW(RegistryDb->NewSubPath) * sizeof(WCHAR);

        RtlCopyUnicodeString(&Registry, Original);
        CopyMemory(PtrSub(PtrAdd(Registry.Buffer, Registry.Length), RegistryDb->SuffixLength), RegistryDb->NewSubPath, Length + sizeof(WCHAR));
        Registry.Length = Registry.Length - RegistryDb->SuffixLength + Length;

        RtlDuplicateUnicodeString(RTL_DUPSTR_ADD_NULL, &Registry, Redirected);

        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}

NTSTATUS
NTAPI
QqNtCreateFile(
    PHANDLE             FileHandle,
    ACCESS_MASK         DesiredAccess,
    POBJECT_ATTRIBUTES  ObjectAttributes,
    PIO_STATUS_BLOCK    IoStatusBlock,
    PLARGE_INTEGER      AllocationSize,
    ULONG               FileAttributes,
    ULONG               ShareAccess,
    ULONG               CreateDisposition,
    ULONG               CreateOptions,
    PVOID               EaBuffer,
    ULONG               EaLength
)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   LocalObjectAttributes;
    UNICODE_STRING      Redirected;

    RtlInitEmptyString(&Redirected);

    LOOP_ONCE
    {
        if (ObjectAttributes == NULL)
            break;

        Status = GetRedirectFile(&Redirected, ObjectAttributes->ObjectName);
        FAIL_BREAK(Status);

        LocalObjectAttributes = *ObjectAttributes;
        LocalObjectAttributes.ObjectName = &Redirected;
        ObjectAttributes = &LocalObjectAttributes;
    }

    Status = StubNtCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);

    RtlFreeUnicodeString(&Redirected);

    return Status;
}

NTSTATUS
NTAPI
QqNtOpenFile(
    PHANDLE             FileHandle,
    ACCESS_MASK         DesiredAccess,
    POBJECT_ATTRIBUTES  ObjectAttributes,
    PIO_STATUS_BLOCK    IoStatusBlock,
    ULONG               ShareAccess,
    ULONG               OpenOptions
)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   LocalObjectAttributes;
    UNICODE_STRING      Redirected;

    RtlInitEmptyString(&Redirected);

    LOOP_ONCE
    {
        if (ObjectAttributes == NULL)
            break;

        Status = GetRedirectFile(&Redirected, ObjectAttributes->ObjectName);
        FAIL_BREAK(Status);

        LocalObjectAttributes = *ObjectAttributes;
        LocalObjectAttributes.ObjectName = &Redirected;
        ObjectAttributes = &LocalObjectAttributes;
    }

    Status = StubNtOpenFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);

    RtlFreeUnicodeString(&Redirected);

    return Status;
}

NTSTATUS
NTAPI
QqNtQueryAttributesFile(
    POBJECT_ATTRIBUTES      ObjectAttributes,
    PFILE_BASIC_INFORMATION FileInformation
)
{
    NTSTATUS            Status;
    OBJECT_ATTRIBUTES   LocalObjectAttributes;
    UNICODE_STRING      Redirected;

    RtlInitEmptyString(&Redirected);

    LOOP_ONCE
    {
        if (ObjectAttributes == NULL)
            break;

        Status = GetRedirectFile(&Redirected, ObjectAttributes->ObjectName);
        FAIL_BREAK(Status);

        LocalObjectAttributes = *ObjectAttributes;
        LocalObjectAttributes.ObjectName = &Redirected;
        ObjectAttributes = &LocalObjectAttributes;
    }

    Status = StubNtQueryAttributesFile(ObjectAttributes, FileInformation);

    RtlFreeUnicodeString(&Redirected);

    return Status;
}

ULONG
WINAPI
QqGetModuleFileNameExW(
    HANDLE  Process,
    PVOID   Module,
    PWSTR   Filename,
    ULONG   Size
)
{
    ULONG       Length;
    PWSTR       File;
    NTSTATUS    Status;
    PROCESS_BASIC_INFORMATION BasicInfo;

    static WCHAR QQProtect[] = L"QQProtect.exe";

    Length = StubGetModuleFileNameExW(Process, (HMODULE)Module, Filename, Size);
    if (Length == 0 || Filename == NULL || Size == 0)
        return Length;

    Status = ZwQueryInformationProcess(Process, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
    if (NT_FAILED(Status) || BasicInfo.UniqueProcessId != CurrentPid())
        return Length;

    File = findnamew(Filename);
    CopyStruct(File, QQProtect, sizeof(QQProtect));

    return File - Filename + CONST_STRLEN(QQProtect);
}

NTSTATUS
NTAPI
QqNtQueryInformationProcess(
    HANDLE              ProcessHandle,
    PROCESSINFOCLASS    ProcessInformationClass,
    PVOID               ProcessInformation,
    ULONG               ProcessInformationLength,
    PULONG              ReturnLength
)
{
    NTSTATUS Status;

    union
    {
        PVOID Information;
        PPROCESS_BASIC_INFORMATION Basic;
    };

    Status = StubNtQueryInformationProcess(ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength);
    FAIL_RETURN(Status);

    Information = ProcessInformation;

    switch (ProcessInformationClass)
    {
        case ProcessBasicInformation:
            if (Basic->UniqueProcessId == CurrentPid())
            {
                Basic->InheritedFromUniqueProcessId = Basic->UniqueProcessId;
            }
            break;
    }

    return Status;
}

HANDLE
NTAPI
QqCreateWaitQQProtectThread(
    PSECURITY_ATTRIBUTES    ThreadAttributes,
    ULONG_PTR               StackSize,
    PTHREAD_START_ROUTINE   StartAddress,
    PVOID                   Parameter,
    ULONG                   CreationFlags,
    PULONG                  ThreadId
)
{
    NTSTATUS    Status;
    PVOID       Ebp, CallCreateQQProtectExchangeWindow;
    PROCESS_BASIC_INFORMATION BasicInfo;

    LOOP_ONCE
    {
        if (PtrAnd(Parameter, 0xFFF00000) != 0)
            continue;

        Status = ZwQueryInformationProcess((HANDLE)Parameter, ProcessBasicInformation, &BasicInfo, sizeof(BasicInfo), NULL);
        FAIL_BREAK(Status);

        if (BasicInfo.UniqueProcessId != CurrentPid())
            break;

        ZwClose((HANDLE)Parameter);

        AllocStack(16);
        Ebp = *((PVOID *)_AddressOfReturnAddress() - 1);

        CallCreateQQProtectExchangeWindow = *((PVOID *)Ebp + 1);
        if (*(PBYTE)CallCreateQQProtectExchangeWindow != CALL)
            break;

        *(PULONG_PTR)((PVOID *)Ebp + 1) += GetOpCodeSize(CallCreateQQProtectExchangeWindow);

        return NULL;
    }

    return HummerCreateThread(ThreadAttributes, StackSize, StartAddress, Parameter, CreationFlags, ThreadId);
}

HRESULT
NTAPI
ShowDBClickPicture(
    PVOID   This,
    PWSTR   PicturePath,
    PRECT   ClickPosition,
    BOOL    FromHistoryIfFalse,
    PVOID
)
{
    SHELLEXECUTEINFOW ShellExecueInfo;

    ZeroMemory(&ShellExecueInfo, sizeof(ShellExecueInfo));
    ShellExecueInfo.cbSize  = sizeof(ShellExecueInfo);
    ShellExecueInfo.lpFile  = PicturePath;
    ShellExecueInfo.lpVerb  = L"open";

    ShellExecuteExW(&ShellExecueInfo);

    return S_OK;
}

BOOL
NTAPI
QqSetWindowPos(
    HWND    hWnd,
    HWND    hWndInsertAfter,
    int     X,
    int     Y,
    int     cx,
    int     cy,
    UINT    Flags
)
{
    BOOL Success;
    //LONG BuddyWidth     = 697;
    //LONG BuddyHeight    = 520;
    LONG GroupWidth     = 603;
    LONG GroupHeight    = 527;
    LONG DiscussWidth   = 556;
    LONG DiscussHeight  = 526;

#define GROUP_WIDTH     722
#define GROUP_HEIGHT    671
#define BUDDY_WIDTH     506
#define BUDDY_HEIGHT    507

#if 0

    AllocConsole();
    PrintConsoleW(L"%d, %d\n", cx, cy);

    {
        FILE *fp;

        fp = fopen("E:\\desktop\\qqlog.txt", "ab");
        fprintf(fp, "%d, %d\r\n", cx, cy);
        fclose(fp);
    }

#endif

    if (
        //(cx == BuddyWidth && cy == BuddyHeight) ||
        (cx == GroupWidth && cy == GroupHeight) ||
        (cx == DiscussWidth && cy == DiscussHeight)
       )
    {
        RECT WorkArea;

        SystemParametersInfoW(SPI_GETWORKAREA, 0, &WorkArea, 0);
/*
        if (cx == BuddyWidth)
        {
            cx = BUDDY_WIDTH;
            cy = BUDDY_HEIGHT;
        }
        else
*/
        {
            cx = GROUP_WIDTH;
            cy = GROUP_HEIGHT;
        }

        X = ((WorkArea.right - WorkArea.left) - cx) / 2;
        Y = ((WorkArea.bottom - WorkArea.top) - cy) / 2;

        CLEAR_FLAG(Flags, SWP_NOSIZE);
    }

    return StubSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, Flags);
}

NTSTATUS CDECL CheckPluginList()
{
    return STATUS_SUCCESS;
}

BOOL CDECL IsTencentTrusted(PCWSTR FileName)
{
    return TRUE;
}

BOOL IsQQUINSpecified()
{
    ULONG_PTR Length;
    WCHAR ch;
    PWSTR CommandLine, QQUIN;

    CommandLine = CurrentPeb()->ProcessParameters->CommandLine.Buffer;

    QQUIN = wcsstr(CommandLine, L"QQUIN:");
    if (QQUIN == NULL)
        return FALSE;

    QQUIN += CONST_STRLEN(L"QQUIN:");

    CommandLine = QQUIN;
    while (*CommandLine != ' ' && *CommandLine != '\t' && *CommandLine != 0)
        ++CommandLine;

    Length = PtrOffset(CommandLine, QQUIN);
    if (Length >= sizeof(GlobalRegistryDb) - sizeof(L".db"))
        return FALSE;

    Length /= sizeof(WCHAR);

    swprintf(GlobalRegistryDb, L"Registry_%.*s.db", Length, QQUIN);
    swprintf(GlobalHistoryDb, L"History_%.*s.db", Length, QQUIN);

    return TRUE;
}

BOOL UnInitialize(PVOID BaseAddress)
{
    return FALSE;
}

BOOL Initialize(PVOID BaseAddress)
{
    BOOL        QQUINSpecified;
    NTSTATUS    Status;
    PVOID       module;
    ULONG_PTR   CreateThreadIAT;
    PROCESS_BASIC_INFORMATION BasicInfo;

    LdrDisableThreadCalloutsForDll(BaseAddress);


    /************************************************************************
      user32
    ************************************************************************/

    module = Ldr::LoadDll(L"USER32.dll");

    MEMORY_FUNCTION_PATCH Function_user32[] =
    {
        EAT_HOOK_JUMP_HASH(module, USER32_SetWindowPos, QqSetWindowPos, StubSetWindowPos),
    };


    /************************************************************************
      ntdll
    ************************************************************************/

    QQUINSpecified = IsQQUINSpecified();

    MEMORY_FUNCTION_PATCH Function_ntdll[] =
    {
        INLINE_HOOK_JUMP(QQUINSpecified ? NtOpenFile            : (PVOID)IMAGE_INVALID_RVA, QqNtOpenFile,               StubNtOpenFile),
        INLINE_HOOK_JUMP(QQUINSpecified ? NtCreateFile          : (PVOID)IMAGE_INVALID_RVA, QqNtCreateFile,             StubNtCreateFile),
        INLINE_HOOK_JUMP(QQUINSpecified ? NtQueryAttributesFile : (PVOID)IMAGE_INVALID_RVA, QqNtQueryAttributesFile,    StubNtQueryAttributesFile),
        INLINE_HOOK_JUMP(ZwQueryInformationProcess,                                         QqNtQueryInformationProcess,StubNtQueryInformationProcess),
    };


    /************************************************************************
      psapi
    ************************************************************************/

    module = Ldr::LoadDll(L"psapi.dll");

    MEMORY_FUNCTION_PATCH Function_psapi[] =
    {
        EAT_HOOK_JUMP_HASH(module, PSAPI_GetModuleFileNameExW, QqGetModuleFileNameExW, StubGetModuleFileNameExW),
    };


    /************************************************************************
      Common
    ************************************************************************/

    module = Ldr::LoadDll(L"Common.dll");

    MEMORY_FUNCTION_PATCH Function_Common[] =
    {
        EAT_HOOK_JUMP_NULL(module, "?IsTencentTrusted@Misc@Util@@YAHPB_W@Z", IsTencentTrusted),
    };


    /************************************************************************
      HummerEngine
    ************************************************************************/

    module = Ldr::LoadDll(L"HummerEngine.dll");

    CreateThreadIAT = IATLookupRoutineRVAByHashNoFix(module, KERNEL32_CreateThread);
    *(PVOID *)&HummerCreateThread = *(PVOID *)PtrAdd(module, CreateThreadIAT);

    MEMORY_PATCH Patch_HummerEngine[] =
    {
        PATCH_MEMORY(QqCreateWaitQQProtectThread, sizeof(PVOID), CreateThreadIAT),
    };


    /************************************************************************
      AppMisc

        L"PluginListCheck
        mov     eax, 0x80004005

        ShowPicInMultiPic begin
    ************************************************************************/

    MEMORY_FUNCTION_PATCH Function_AppMisc[] =
    {
        INLINE_HOOK_JUMP_RVA_NULL(0x122CCA, CheckPluginList),
        INLINE_HOOK_JUMP_RVA_NULL(0x1BC97B, ShowDBClickPicture),
    };


    /************************************************************************
      ChatFrameApp

        L"exit for invalid version.17
    ************************************************************************/

    MEMORY_FUNCTION_PATCH Function_ChatFrame[] =
    {
        INLINE_HOOK_JUMP_RVA_NULL(0x93C3, IsTencentTrusted),
    };


    /************************************************************************
      AppUitl

        L"PerfStand.CheckImportantModule.Begin"
        L"DllCheck"
        L"Dll Check Fail"

        check:
            QQExternal.exe
            TXPlatform.exe
            ...
    ************************************************************************/

    MEMORY_FUNCTION_PATCH Function_AppUtil[] =
    {
        INLINE_HOOK_JUMP_RVA_NULL(0xB884, CheckPluginList),
    };


    /************************************************************************
      end
    ************************************************************************/

    PATCH_ARRAY *Entry, Array[] =
    {
        { L"HummerEngine.dll",  Patch_HummerEngine, countof(Patch_HummerEngine) },
        { NULL,                 NULL,               0,                          Function_Common,    countof(Function_Common) },
        { L"AppUtil.dll",       NULL,               0,                          Function_AppUtil,   countof(Function_AppUtil) },
        { L"AppMisc.dll",       NULL,               0,                          Function_AppMisc,   countof(Function_AppMisc) },
        { L"ChatFrameApp.dll",  NULL,               0,                          Function_ChatFrame, countof(Function_ChatFrame) },

        { NULL,                 NULL,               0,                          Function_ntdll,     countof(Function_ntdll) },
        { NULL,                 NULL,               0,                          Function_psapi,     countof(Function_psapi) },
        { NULL,                 NULL,               0,                          Function_user32,    countof(Function_user32) },
    };


    FOR_EACH(Entry, Array, countof(Array))
    {
        PVOID Base;

        Base = Entry->DllName == NULL ? NULL : Ldr::LoadDll(Entry->DllName);

        Nt_PatchMemory(Entry->Patch, Entry->PatchCount, Entry->FunctionPatch, Entry->FunctionCount, Base);
    }

    return TRUE;
}

BOOL WINAPI DllMain(PVOID BaseAddress, ULONG Reason, PVOID Reserved)
{
    switch (Reason)
    {
        case DLL_PROCESS_ATTACH:
            return Initialize(BaseAddress) || UnInitialize(BaseAddress);

        case DLL_PROCESS_DETACH:
            UnInitialize(BaseAddress);
            break;
    }

    return TRUE;
}
