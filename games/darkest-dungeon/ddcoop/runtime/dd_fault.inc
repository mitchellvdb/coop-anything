/* Best-effort first-chance diagnostics only. Never handles/resumes an exception,
 * reads game state or produces a memory dump. Cache ranges before any fault. */
static void *g_fault_handler;
static uintptr_t g_fault_bases[2];
static uint32_t g_fault_sizes[2];
static volatile LONG g_fault_inside,g_fault_count;
static const char *fault_range(uintptr_t pc,uintptr_t *rva) {
    static const char *const names[2]={"game","runtime"};
    for(unsigned i=0;i<2;i++)if(pc>=g_fault_bases[i] && pc-g_fault_bases[i]<g_fault_sizes[i]) {
        *rva=pc-g_fault_bases[i];return names[i];
    }
    *rva=0;return "external";
}
static LONG CALLBACK fault_trace(EXCEPTION_POINTERS *info) {
    if(!info || !info->ExceptionRecord)return EXCEPTION_CONTINUE_SEARCH;
    DWORD code=info->ExceptionRecord->ExceptionCode;
    if(code!=EXCEPTION_ACCESS_VIOLATION && code!=EXCEPTION_ILLEGAL_INSTRUCTION && code!=EXCEPTION_INT_DIVIDE_BY_ZERO)return EXCEPTION_CONTINUE_SEARCH;
    if(InterlockedCompareExchange(&g_fault_inside,1,0))return EXCEPTION_CONTINUE_SEARCH;
    if(InterlockedIncrement(&g_fault_count)<=4) {
        char line[640];uintptr_t rva=0;
        const char *name=fault_range((uintptr_t)info->ExceptionRecord->ExceptionAddress,&rva);
        size_t at=(size_t)snprintf(line,sizeof line,"first-chance code=%08lx thread=%lu at %s+0x%llx; frames",(unsigned long)code,(unsigned long)GetCurrentThreadId(),name,(unsigned long long)rva);
        void *frames[12];USHORT n=RtlCaptureStackBackTrace(0,12,frames,NULL);
        for(USHORT i=0;i<n && at<sizeof line-48;i++) {
            name=fault_range((uintptr_t)frames[i],&rva);
            if(rva)at+=(size_t)snprintf(line+at,sizeof line-at," %s+%llx",name,(unsigned long long)rva);
        }
        dd_log_fault(line); /* No heap or normal logger lock on this path. */
    }
    InterlockedExchange(&g_fault_inside,0);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void fault_trace_install(HMODULE self) {
    if(g_fault_handler)return;
    HMODULE modules[2]={GetModuleHandleW(NULL),self};
    for(unsigned i=0;i<2;i++)if(modules[i]) {
        const uint8_t *base=(const uint8_t *)modules[i];
        const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)base;
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x100000)continue;
        const IMAGE_NT_HEADERS64 *nt=(const IMAGE_NT_HEADERS64 *)(base+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE)continue;
        g_fault_bases[i]=(uintptr_t)base;g_fault_sizes[i]=nt->OptionalHeader.SizeOfImage;
    }
    g_fault_handler=AddVectoredExceptionHandler(0,fault_trace);
}
static void fault_trace_remove(void) {
    if(g_fault_handler){RemoveVectoredExceptionHandler(g_fault_handler);g_fault_handler=NULL;}
}
