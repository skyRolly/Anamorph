"""LLDB-only x86_64 Itanium ABI exception-type filter for the native observer."""
import lldb

seen_bad_call = False
exception_type = ''
enqueue_observations = 0
framework_enqueues = 0
origin_module = ''
origin_symbol = ''


def on_cf_enqueue(frame, _location, _internal):
    global enqueue_observations, framework_enqueues
    caller = frame.GetThread().GetFrameAtIndex(1)
    if caller.GetModule().GetFileSpec().GetFilename() == 'AudioToolboxCore':
        framework_enqueues += 1
    if enqueue_observations < 16:
        print('AU enqueue trace: '+str(caller.GetModule().GetFileSpec().GetFilename())
              +' / '+str(caller.GetFunctionName()), flush=True)
    enqueue_observations += 1
    return False


def on_throw(frame, _location, _internal):
    global seen_bad_call, exception_type, origin_module, origin_symbol
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
    caller = frame.GetThread().GetFrameAtIndex(2)
    origin_module = caller.GetModule().GetFileSpec().GetFilename() or ''
    origin_symbol = caller.GetFunctionName() or ''
    print('AU observer: first bad_function_call throw: '+name)
    return True


def install(target):
    breakpoint = target.BreakpointCreateByName('__cxa_throw')
    assert breakpoint.IsValid()
    breakpoint.SetScriptCallbackFunction('au_teardown_lldb.on_throw')
    if target.GetExecutable().GetFilename() == 'apple-au-pending-control':
        enqueue_breakpoint = target.BreakpointCreateByName('CFRunLoopPerformBlock')
        assert enqueue_breakpoint.IsValid()
        enqueue_breakpoint.SetScriptCallbackFunction('au_teardown_lldb.on_cf_enqueue')
