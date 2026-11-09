/* Owned process only: test registration and a synthetic exception record.
 * No exception is raised; the handler must never swallow or mutate one. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned logged;
static char last_line[800];
static void dd_log_fault(const char *line) {logged++;snprintf(last_line,sizeof last_line,"%s",line);}
#include "../runtime/dd_fault.inc"
int main(void) {
    unsigned failures=0;
#define CHECK(x) do{if(!(x)){printf("FAIL fault:%d %s\n",__LINE__,#x);failures++;}}while(0)
    fault_trace_install(GetModuleHandleW(NULL));CHECK(g_fault_handler!=NULL);
    void *handler=g_fault_handler;fault_trace_install(GetModuleHandleW(NULL));CHECK(handler==g_fault_handler);
    EXCEPTION_RECORD record={0},before;CONTEXT context={0},context_before;
    EXCEPTION_POINTERS info={&record,&context};
    record.ExceptionCode=EXCEPTION_BREAKPOINT;record.ExceptionAddress=(void *)(g_fault_bases[0]+0x123);
    CHECK(fault_trace(&info)==EXCEPTION_CONTINUE_SEARCH && !logged);
    record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;before=record;context_before=context;
    CHECK(fault_trace(&info)==EXCEPTION_CONTINUE_SEARCH && logged==1);
    CHECK(strstr(last_line,"c0000005") && strstr(last_line,"game+0x123"));
    CHECK(!memcmp(&record,&before,sizeof record) && !memcmp(&context,&context_before,sizeof context));
    for(unsigned i=0;i<8;i++)CHECK(fault_trace(&info)==EXCEPTION_CONTINUE_SEARCH);
    CHECK(logged==4 && !g_fault_inside);
    CHECK(fault_trace(NULL)==EXCEPTION_CONTINUE_SEARCH);
    fault_trace_remove();CHECK(!g_fault_handler);fault_trace_remove();
    printf("Fault diagnostics: registration/cleanup, selected first-chance codes, bounded RVA-only log, unchanged context and continue-search: %s (%u failures)\n",failures?"FAIL":"PASS",failures);
    return failures!=0;
}
