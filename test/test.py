import sys
import os
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.realpath(os.path.join(SCRIPT_DIR, ".."))
EXAMPLES_DIR = os.path.join(ROOT_DIR, "examples")
EXAMPLES_OUT_DIR = os.path.join(EXAMPLES_DIR, "out_test")
# TODO: Support unix builds
INTERP_PATH = os.path.join(ROOT_DIR, "build", "lisp_impl.exe")

COL_PINK = '\033[95m'
COL_OKBLUE = '\033[94m'
COL_OKGREEN = '\033[92m'
COL_WARNING = '\033[93m'
COL_FAIL = '\033[91m'
COL_ENDC = '\033[0m'

def fmt_num_word(word, num):
    if num == 1:
        return word
    return word + 's'

def main():
    print("Running examples from {}".format(EXAMPLES_DIR))
    example_files = os.listdir(EXAMPLES_DIR)
    tests_to_process = len(example_files)
    processed = 0
    succeeded = 0
    failed = 0
    for ef in example_files:
        fp = os.path.join(EXAMPLES_DIR, ef)
        if os.path.isfile(fp):
            # This is an example, so run it
            res = subprocess.run([INTERP_PATH, fp], stdout=subprocess.PIPE, text=True)
            print("[{}]: Running".format(ef), end="")
            got = res.stdout
            # Compare to the expected output
            test_output_file = os.path.join(EXAMPLES_OUT_DIR, ef + '.out')
            try:
                with open(test_output_file, "r") as tof:
                    same = True
                    expected = tof.read()
                    got_it = iter(got.splitlines())
                    expected_it = iter(expected.splitlines())
                    while True:
                        el = next(expected_it, None)
                        el = el.strip() if el is not None else None
                        gl = next(got_it, None)
                        gl = gl.strip() if gl is not None else None
                        if not el and not gl:
                            # Nothing left to compare, just exit
                            break
                        same = el == gl
                        if not same:
                            break
                    if same:
                        print("... {}Test passed{}".format(COL_OKGREEN, COL_ENDC))
                        succeeded += 1
                    else:
                        print("... {}Test failed{}".format(COL_FAIL, COL_ENDC))
                        print(COL_OKGREEN + "- Expected:")
                        print(expected + COL_ENDC)
                        print(COL_FAIL + "- Got:")
                        print(got + COL_ENDC)
                        failed += 1
                processed += 1
            except OSError as e:
                print("\n{}Couldn't read example output file at {}\n{}{}".format(COL_FAIL,
                                                                               test_output_file,
                                                                               e, COL_ENDC))
                return -1
    print("Processed {} tests".format(processed))
    print("{}{} {} succeeded{}".format(COL_OKGREEN, succeeded, fmt_num_word("test", succeeded), COL_ENDC))
    if failed != 0:
        print("{}{} {} failed{}".format(COL_FAIL, failed, fmt_num_word("test", failed), COL_ENDC))

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
