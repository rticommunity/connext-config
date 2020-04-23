#!/bin/sh

[ $# -ne 1 ] && echo "Usage: $0 <connext-config executable>" && exit 1
CONNEXT_CONFIG="$1"

[ ! -x "$CONNEXT_CONFIG" ] && echo "Cannot find connext-config executable"

# First print the architectures
echo "---------------------------------------------------------------------"
echo "All Architectures:"
echo "---------------------------------------------------------------------"
allTargets=`$CONNEXT_CONFIG --list-all`
[ $? -ne 0 ] && echo "Command '--list-all' failed" && exit 1
echo "$allTargets"
echo ""

# Then print the result of the internal table dump
echo "---------------------------------------------------------------------"
echo "Internal Table Dump"
echo "---------------------------------------------------------------------"
$CONNEXT_CONFIG --dump-all 
[ $? -ne 0 ] && echo "Command '--dump-all' failed" && exit 1
echo ""

allCmd="--ccomp --clink --cxxcomp --cxxlink --cflags --cxxflags --ldflags --ldxxflags --ldlibs --ldxxlibs --os --platform"
for target in $allTargets; do
    echo "---------------------------------------------------------------------"
    echo "$target:"
    for cmd in $allCmd; do
        tmp="`$CONNEXT_CONFIG $cmd $target`"
        [ $? -ne 0 ] && echo "Failed command: $CONNEXT_CONFIG $cmd $target" && exit 1
        echo "\t$cmd = $tmp"

        tmp="`$CONNEXT_CONFIG --noexpand $cmd $target`"
        [ $? -ne 0 ] && echo "Failed command: $CONNEXT_CONFIG --noexpand $cmd $target" && exit 1
        echo "\t$cmd (ne) = $tmp"
    done
    tmp="`$CONNEXT_CONFIG --noexpand --debug --ldlibs $target`"
    [ $? -ne 0 ] && echo "Failed command: $CONNEXT_CONFIG --noexpand --debug --ldlibs $target" && exit 1
    echo "\t--ldlibs (debug) = $tmp"

    tmp="`$CONNEXT_CONFIG --noexpand --static --ldlibs $target`"
    [ $? -ne 0 ] && echo "Failed command: $CONNEXT_CONFIG --noexpand --static --ldlibs $target" && exit 1
    echo "\t--ldlibs (static) = $tmp"

    tmp="`$CONNEXT_CONFIG --noexpand --debug --static --ldlibs $target`"
    [ $? -ne 0 ] && echo "Failed command: $CONNEXT_CONFIG --noexpand --debug --static --ldlibs $target" && exit 1
    echo "\t--ldlibs (static,debug) = $tmp"

    echo ""
done
