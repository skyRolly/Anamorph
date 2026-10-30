"""LLDB-only x86_64 Itanium ABI exception-type filter for the native observer."""
import lldb

seen_bad_call = False
exception_type = ''


def on_throw(frame, _location, _internal):
    global seen_bad_call, exception_type
    # __cxa_throw(void* exception, std::type_info* type, destructor): x86_64 ABI.
    process = frame.GetThread().GetProcess()
    type_info = frame.FindRegister('rsi').GetValueAsUnsigned()
    error = lldb.SBError()
    name_pointer = process.ReadPointerFromMemory(type_info+8, error)
    if not error.Success():
        print('AU observer: cannot read exception type_info')
        return True
    # Darwin libc++ reserves the top bit for non-unique RTTI names.
    name = process.ReadCStringFromMemory(name_pointer & ((1 << 63)-1), 256, error)
    if not error.Success():
        print('AU observer: cannot read exception type name')
        return True
    if 'bad_function_call' not in name:
        return False
    seen_bad_call = True
    exception_type = name
    print('AU observer: first bad_function_call throw: '+name)
    return True


def install(target):
    breakpoint = target.BreakpointCreateByName('__cxa_throw')
    assert breakpoint.IsValid()
    breakpoint.SetScriptCallbackFunction('au_teardown_lldb.on_throw')
