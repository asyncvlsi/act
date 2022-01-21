#!/usr/bin/python3

import os


def run_test(folder):
    if os.system("cd {} && ./run.sh".format(folder)) != 0:
        exit(1)


run_test("act/lang/test/")
run_test("act/transform/act2v/test/")
run_test("act/transform/aflat/test/")
run_test("act/transform/ext2sp/test/")
run_test("act/transform/prs2cells/test/")
run_test("act/transform/prs2net/test/")
run_test("act/transform/prs2sim/test/")
run_test("act/transform/testing/inline/test/")
run_test("act/transform/testing/state/test/")
run_test("act/transform/v2act/test/")
