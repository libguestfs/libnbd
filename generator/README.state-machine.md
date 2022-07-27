## The state machine

Each state has some associated C code which is called when the state
is entered, or when the state is re-entered because of an external
event.  That code is in [generator/states*.c](states.c).

Each handle starts in the top level START state.

When you enter a state, the associated C code for that state runs.  If
the C code calls `SET_NEXT_STATE` and returns 0 then the connection
enters the next state without blocking.  If the C code calls
`SET_NEXT_STATE_AND_BLOCK` and returns 0 then the connection blocks,
but will resume with the code for the next state on the next external
event.  If the C code does *not* call either macro but returns 0, the
state machine is blocked and will not be re-entered until an external
event happens (see below), where the same C code will be executed
again on re-entry.  If the C code returns -1 after using
`set_error()`, then the state machine blocks and the caller should
report failure; the next external event will resume the state machine
according to whether `SET_NEXT_STATE` was used.

There are various end states such as `CLOSED` and `DEAD`.  These are
not special in relation to the above state transition rules, it's just
that they have no way to move to another state.  However, the `DEAD`
state expects that `set_error()` was used in the previous state, and
will return -1 itself after performing cleanup actions; the earlier
state that wants to transition to `DEAD` should return 0 rather than
-1, so as not to bypass this cleanup.

An external event is something like the file descriptor being ready to
read or write, or the main program calling a function such as
`nbd_aio_connect`.  Possible external events, and the next state
resulting, are listed in the states table in
[generator/state_machine.ml](state_machine.ml).

An empty string `""` for an external event’s next state means the same
state is re-entered.  The same C code for the state will be run again.

States can be grouped hierarchically.  States can be referred to by an
absolute path from the top level, such as `".DEAD"`, or by a relative
path from the current level, such as `"CONNECT"` (another state at the
same level), `"REPLY.START"` (a state in a sub-group), or
`"^FINISH_COMMAND"` (a state in the level above the current one).
When entering a group you must enter at the START state.  When leaving
a group and going to a higher level in the state tree there is no
restriction on the next state.
