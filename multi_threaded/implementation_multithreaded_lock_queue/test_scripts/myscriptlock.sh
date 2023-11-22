#!/usr/bin/env bash

source_dir=$(dirname ${BASH_SOURCE[0]})
source "$source_dir/utils.sh"

if [[ $(check_dir) -eq 1 ]]; then
    exit 1
fi

cfile=mylock.c

cp rwlock_users/$cfile .
clang -o rwlock_test rwlock.o $cfile

./rwlock_test
rc=$?

new_files="$cfile rwlock_test"

if [[ $rc -eq 0 ]]; then
    echo "It worked!"
else
    echo "--------------------------------------------------------------------------------"
    echo "return code: $rc"
    echo "--------------------------------------------------------------------------------"
fi

cleanup $new_files
exit $rc
