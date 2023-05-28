#!/bin/sh
failed=0
for test in $@
do
    if [ -f $test ] 
    then
        output="$($test 2>&1)"
        if [ $? -eq "0" ]
        then
            echo "\033[0;32m[ PASS ]\033[0m $test\n"
        else
            echo "\033[0;31m[ FAIL ]\033[0m $test\n"
            echo $output
            failed=$((failed+1))
        fi
    else
        echo "\033[0;33m[ NONE ]\033[0m $test\n"
    fi
done
echo ""
if [ $failed -eq "0" ]
then
    printf "\033[0;32mFailed $failed Tests\033[0m\n"
else
    printf "\033[0;31mFailed $failed Tests\033[0m\n"
fi
exit $failed
