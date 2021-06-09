#!/bin/sh

set -e

main() {
    MAKE="${MAKE-make -j $(getconf _NPROCESSORS_ONLN)}"

    autoreconf -if

    CONFIG_ARGS="\
--enable-gcc-warnings \
--enable-fuse \
--enable-ocaml \
--enable-python \
--enable-golang \
--with-gnutls \
--with-libxml2 \
"

    if test "$GOLANG" != "skip"
    then
       CONFIG_ARGS="$CONFIG_ARGS --enable-golang"
    fi

    ./configure $CONFIGURE_OPTS $CONFIG_ARGS

    $MAKE

    if test -n "$CROSS"
    then
        echo "Possibly run tests with an emulator in the future"
        return 0
    fi

    if test "$(uname)" != "Linux"
    then
        echo "Tests are temporarily skipped on non-Linux platforms"
        return 0
    fi

    # Add a way to run all the tests, even the skipped ones, with an environment
    # variable, so that it can be set fora branch or fork in GitLab.
    if test "$SKIPPED_TESTS" != "force"
    then
        # Skip tests from ci/skipped_tests if this is the right OS version
        # The file
        os_id="$(sh -c '. /etc/os-release; echo "${NAME}-${VERSION_ID}"')"

        echo OS ID: $os_id

        # Skips comments and empty lines
        grep '^[^#]' ci/skipped_tests | while read skipper
        do
            regex="${skipper%%;*}"
            tests="${skipper#*;}"

            echo SKIPPER: "$skipper"
            echo REGEX: "$regex"
            echo TESTS: "$tests"

            # Ignore lines not meant for current $os_id
            if ! echo "$os_id" | grep -q "$regex"; then echo NOPE; continue; fi

            echo SKIPPING $tests

            for test_case in $tests
            do
                test_case_bckup="${test_case}_skipped_test"
                if ! git ls-files "$test_case" | grep -q "$test_case"
                then
                    make -C "$(dirname "$test_case")" "$(basename "$test_case")" 2>/dev/null || :
                fi
                echo Backing up "$test_case" to "${test_case_bckup}"
                cp "$test_case" "${test_case_bckup}"

                echo 'echo "Test skipped based on ci/skipped_tests file"'> "$test_case"
                echo 'exit 77'>> "$test_case"
                chmod +x "$test_case"
            done
        done
    fi

    # We do not want the check to cause complete failure as we need to clean up
    # the skipped test cases
    failed=0
    $MAKE check || failed=1

    find . -name '*_skipped_test' -print | while read test_case_bckup
    do
        test_case="${test_case_bckup%_skipped_test}"

        echo Moving "${test_case_bckup}" back to "${test_case}"
        rm -f "${test_case}"
        mv -f "${test_case_bckup}" "${test_case}"
    done

    # Now it is safe to fail
    test "$failed" -eq 0

    if test "$CHECK_VALGRIND" = "force"
    then
        $MAKE check-valgrind
    fi

    if test "$DIST" != "skip"
    then
        $MAKE dist
        $MAKE maintainer-check-extra-dist
    fi

    if test "$DISTCHECK" = "force"
    then
        $MAKE distcheck
    fi
}

main "$@"
