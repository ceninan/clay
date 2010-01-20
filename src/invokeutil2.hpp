#ifndef __CLAY_INVOKEUTIL2_HPP
#define __CLAY_INVOKEUTIL2_HPP

int hashArgs(ArgListPtr args, const vector<bool> &isStaticFlags);
bool matchingArgs(ArgListPtr args, InvokeTableEntryPtr entry);
void initArgsInfo(InvokeTableEntryPtr entry, ArgListPtr args);

InvokeTableEntryPtr
findMatchingEntry(InvokeTablePtr table, ArgListPtr args);

InvokeTablePtr
newInvokeTable(const vector<FormalArgPtr> &formalArgs);

void
initProcedureInvokeTable(ProcedurePtr x);

InvokeTableEntryPtr
lookupProcedureInvoke(ProcedurePtr x, ArgListPtr args);

void
initProcedureEnv(ProcedurePtr x,
                 ArgListPtr args,
                 InvokeTableEntryPtr entry);

void 
nitOverloadableInvokeTables(OverloadablePtr x);

InvokeTableEntryPtr
lookupOverloadableInvoke(OverloadablePtr x, ArgListPtr args);

void
initOverloadableEnv(OverloadablePtr x,
                    ArgListPtr args,
                    InvokeTableEntryPtr entry);

#endif