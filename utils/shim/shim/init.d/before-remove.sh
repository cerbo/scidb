#!/bin/bash
# shim
# package pre-uninstallation script
# This script is run by the package management system before package
# is uninstalled.

if test -f /etc/init.d/shimsvc; then
  /etc/init.d/shimsvc stop
fi
if test -n "$(which update-rc.d 2>/dev/null)"; then
# Ubuntu
  update-rc.d -f shimsvc remove
elif test -n "$(which chkconfig)"; then
# RHEL
  chkconfig --del shimsvc
fi
