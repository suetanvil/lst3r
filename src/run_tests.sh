#!/bin/bash

# Helper script to run tests on the newly-built VM.  lst can't exit
# with a different status, so we need to trawl through the output
# looking for error messages.

set -e

cd ../optional

results=`mktemp -t lst.testresults`

echo
echo "Running tests:"
../lst3 ../systemImage \
        -e "File new; fileIn: 'test.st'. Test new allAndQuit" | \
    tee $results
echo

if grep -q failure $results; then
    echo "Test failed!"
    rm $results
    exit 1
else
    echo "All tests passed!"
    rm $results
    exit 0
fi

