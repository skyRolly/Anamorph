set pagination off
set confirm off
set debuginfod enabled off
set auto-load off
set disable-randomization off
set print thread-events off
set detach-on-fork off
set schedule-multiple on
handle SIGPIPE nostop noprint pass
python
import gdb
faults = []
exits = []
def stopped(event):
    if isinstance(event, gdb.SignalEvent) and event.stop_signal in ('SIGSEGV', 'SIGBUS', 'SIGILL', 'SIGABRT'):
        faults.append(event.stop_signal)
def exited(event):
    exits.append(getattr(event, 'exit_code', None))
gdb.events.stop.connect(stopped)
gdb.events.exited.connect(exited)
gdb.execute('run')
while not faults:
    live = [inferior for inferior in gdb.inferiors() if inferior.threads()]
    if not live: break
    gdb.execute('inferior ' + str(live[0].num))
    gdb.execute('continue')
if faults:
    print('NR014_DIAGNOSTIC_FAULT', faults)
    gdb.execute('info sharedlibrary')
    gdb.execute('thread apply all bt 24')
    gdb.execute('quit 139')
else:
    print('NR014_DIAGNOSTIC_EXITS', exits)
    gdb.execute('quit ' + ('1' if any(code not in (0, None) for code in exits) else '0'))
end
