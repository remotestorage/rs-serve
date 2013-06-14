#!/bin/bash

cp authorizations.dat{,.tmp}
grep -v '^$' authorizations.dat.tmp > authorizations.dat
rm authorizations.dat.tmp
