#!/bin/sh
# Compiles and runs the learner's answer (stdin) inside checks/tpl/l1-decl.cpp. Exit 0 = HIT.
exec python3 "$(dirname "$0")/harness.py" "$(dirname "$0")/tpl/l1-decl.cpp"
