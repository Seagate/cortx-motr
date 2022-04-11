#
# Load gdb extensions for Motr
#

printf "\n"
source scripts/gdb/gdb-extensions.py
source scripts/gdb/gdb-extensions
printf "\n"

#
# Ignore signals internally used by Motr
#

# SIGSEGV -- used by m0_cookie
handle SIGSEGV pass
handle SIGSEGV noprint
# SIG34 -- used by the timer code
handle SIG34 pass
handle SIG34 noprint

#
# Don't print "new thread" messages.
#
set print thread-events off

#
# Suppress "Detaching after fork from child process..." messages.
#
set print inferior-events off

#
# Kernel debugging with kgdb
#

#set remotebaud 115200
#target remote /dev/ttyS0

set confirm off

# emacs is infinite
set height 0

#
# sockets gets valid SIGPIPE when remote side RSTs
#
handle SIGPIPE ignore nostop print
handle SIGUSR2 nostop print pass
