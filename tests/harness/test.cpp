/*
 * eez-framework tests: the test runner (see test.h).
 *
 *   eez-framework-tests [options] [filter...]
 *
 *   filter        run the tests whose name contains one of the filters
 *   --list        list the tests
 *   --no-fork     run the tests in this process (a crash stops the run)
 *   --verbose     print the output of every test, not only of the failed ones
 *   --timeout=S   seconds per test (default 20)
 */

#include "test.h"
#include "harness.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include <vector>

namespace test {

struct TestInfo {
    std::string name;
    std::function<void()> func;
    std::string knownBug;
    // the known bug shows only with AddressSanitizer / UndefinedBehaviorSanitizer
    // (id with the suffix "@san", see test.h)
    bool needsSanitizer;
};

#if defined(__SANITIZE_ADDRESS__)
static const bool SANITIZED = true;
#else
static const bool SANITIZED = false;
#endif

static std::vector<TestInfo> &tests() {
    static std::vector<TestInfo> g_tests;
    return g_tests;
}

static int g_failures;

void registerTest(const std::string &name, std::function<void()> func, const char *knownBug) {
    std::string bug = knownBug ? knownBug : "";
    bool needsSanitizer = false;
    auto suffix = bug.find("@san");
    if (suffix != std::string::npos) {
        bug = bug.substr(0, suffix);
        needsSanitizer = true;
    }
    tests().push_back({ name, func, bug, needsSanitizer });
}

void fail(const char *file, int line, const std::string &message) {
    g_failures++;
    const char *slash = strrchr(file, '/');
    fprintf(stderr, "    %s:%d: %s\n", slash ? slash + 1 : file, line, message.c_str());
}

std::string format(const char *fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return buffer;
}

enum Result { PASSED, FAILED, CRASHED, TIMED_OUT };

static const char *resultName(Result result) {
    return result == PASSED ? "passed" : result == FAILED ? "failed" : result == CRASHED ? "crashed" : "timed out";
}

static bool runFunc(const TestInfo &test) {
    g_failures = 0;
    try {
        test.func();
    } catch (Abort &) {
    } catch (...) {
        fail(__FILE__, __LINE__, "unexpected exception");
    }
    fflush(stdout);
    fflush(stderr);
    return g_failures == 0;
}

// a sanitizer report in the output is a failure even if the test itself passed
static bool hasSanitizerReport(const std::string &output) {
    return output.find("runtime error:") != std::string::npos ||
        output.find("ERROR: AddressSanitizer") != std::string::npos ||
        output.find("WARNING: ThreadSanitizer") != std::string::npos ||
        output.find("ERROR: LeakSanitizer") != std::string::npos;
}

static Result runForked(const TestInfo &test, unsigned timeout, std::string &output, std::string &detail) {
    int fds[2];
    if (pipe(fds) != 0) {
        perror("pipe");
        exit(2);
    }

    fflush(stdout);
    fflush(stderr);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(2);
    }

    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 1);
        dup2(fds[1], 2);
        close(fds[1]);
        alarm(timeout);
        bool passed = runFunc(test);
        _exit(passed ? 0 : 1);
    }

    close(fds[1]);
    char buffer[4096];
    ssize_t n;
    while ((n = read(fds[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, n);
    }
    close(fds[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGALRM) {
            detail = format("after %u s", timeout);
            return TIMED_OUT;
        }
        detail = format("signal %d (%s)", sig, strsignal(sig));
        return CRASHED;
    }

    if (WEXITSTATUS(status) != 0) {
        return FAILED;
    }

    if (hasSanitizerReport(output)) {
        detail = "sanitizer report";
        return FAILED;
    }

    return PASSED;
}

static void printIndented(const std::string &output) {
    size_t start = 0;
    while (start < output.size()) {
        size_t end = output.find('\n', start);
        if (end == std::string::npos) {
            end = output.size();
        }
        printf("  | %s\n", output.substr(start, end - start).c_str());
        start = end + 1;
    }
}

static int run(int argc, char **argv) {
    std::vector<std::string> filters;
    bool list = false;
    bool fork = true;
    bool verbose = false;
    unsigned timeout = 20;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--list") {
            list = true;
        } else if (arg == "--no-fork") {
            fork = false;
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg.rfind("--timeout=", 0) == 0) {
            timeout = (unsigned)atoi(arg.c_str() + 10);
        } else if (arg.rfind("--", 0) == 0) {
            fprintf(stderr, "unknown option %s\n", arg.c_str());
            return 2;
        } else {
            filters.push_back(arg);
        }
    }

    std::vector<const TestInfo *> selected;
    for (auto &test : tests()) {
        bool match = filters.empty();
        for (auto &filter : filters) {
            if (test.name.find(filter) != std::string::npos) {
                match = true;
            }
        }
        if (match) {
            selected.push_back(&test);
        }
    }

    if (list) {
        for (auto test : selected) {
            printf("%s%s\n", test->name.c_str(), test->knownBug.empty() ? "" : (" [" + test->knownBug + "]").c_str());
        }
        return 0;
    }

    harness::init();

    int numPassed = 0, numFailed = 0, numExpectedFailures = 0, numUnexpectedPasses = 0;
    std::vector<std::string> failedNames;

    for (auto test : selected) {
        std::string output;
        std::string detail;
        Result result;
        if (fork) {
            result = runForked(*test, timeout, output, detail);
        } else {
            printf("RUN   %s\n", test->name.c_str());
            fflush(stdout);
            result = runFunc(*test) ? PASSED : FAILED;
        }

        bool knownBug = !test->knownBug.empty();
        // a bug that only a sanitizer shows passes in a build without one
        bool hiddenBug = knownBug && test->needsSanitizer && !SANITIZED && result == PASSED;
        const char *label;
        bool bad;
        if (hiddenBug) {
            label = "XFAIL";
            bad = false;
            numExpectedFailures++;
        } else if (result == PASSED) {
            label = knownBug ? "XPASS" : "PASS ";
            bad = knownBug;
            if (knownBug) numUnexpectedPasses++; else numPassed++;
        } else {
            label = knownBug ? "XFAIL" : "FAIL ";
            bad = !knownBug;
            if (knownBug) numExpectedFailures++; else numFailed++;
        }

        std::string suffix;
        if (knownBug) {
            suffix += " [" + test->knownBug + "]";
        }
        if (result != PASSED) {
            suffix += std::string(" (") + resultName(result) + (detail.empty() ? "" : ", " + detail) + ")";
        }
        if (hiddenBug) {
            suffix += " (passed: the bug shows only with AddressSanitizer/UBSan)";
        } else if (result == PASSED && knownBug) {
            suffix += " (passes: is " + test->knownBug + " fixed? remove the TEST_BUG marker)";
        }
        printf("%s %s%s\n", label, test->name.c_str(), suffix.c_str());

        if (bad) {
            failedNames.push_back(test->name);
        }
        if (fork && (verbose || (bad && !output.empty()))) {
            printIndented(output);
        }
        fflush(stdout);
    }

    printf("\n%d passed, %d failed, %d known bugs (expected failures), %d known bugs passing\n",
        numPassed, numFailed, numExpectedFailures, numUnexpectedPasses);
    for (auto &name : failedNames) {
        printf("  FAILED: %s\n", name.c_str());
    }

    return failedNames.empty() ? 0 : 1;
}

} // namespace test

int main(int argc, char **argv) {
    return test::run(argc, argv);
}
