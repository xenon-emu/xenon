#include <stdio.h>
#include <memory>
#include "fmt/format.h"

#include "PPCInterpreter.h"
#include "Base/PathUtil.h"
#include "Base/StringUtil.h"

#define BLR_OPCODE 0x4e800020

const u32 START_ADDRESS = 0x10000000;

std::filesystem::path testsPath;
std::filesystem::path testsBinPath;

typedef std::vector<std::pair<std::string, std::string>> AnnotationList;

void PPCInterpreter::SetRegFromString(PPU_STATE *ppuState, const char *regName, const char *regValue) {
  s32 value;
  if (sscanf(regName, "r%d", &value) == 1) {
    curThread.GPR[value] = Base::getFromString<u64>(regValue);
  } else if (sscanf(regName, "f%d", &value) == 1) {
    curThread.FPR[value].setValue(Base::getFromString<f64>(regValue));
  }  else if (sscanf(regName, "v%d", &value) == 1) {
    curThread.VR[value] = Base::getFromString<Base::Vector128>(regValue);
  } else if (std::strcmp(regName, "cr") == 0) {
    curThread.CR.CR_Hex = static_cast<u32>(Base::getFromString<u64>(regValue));
  } else {
    LOG_ERROR(Xenon, "[Testing] SetRegFromString: Unrecognized register name: {}", regName);
  }
}

bool PPCInterpreter::CompareRegWithString(PPU_STATE *ppuState, const char *regName, const char *regValue, std::string &outRegValue) {
  s32 value = 0;
  if (sscanf(regName, "r%d", &value) == 1) {
    u64 expected = Base::getFromString<u64>(regValue);
    if (curThread.GPR[value] != expected) {
      outRegValue = FMT("{:016X}", curThread.GPR[value]);
      return false;
    }
    return true;
  } else if (sscanf(regName, "f%d", &value) == 1) {
    if (std::strstr(regValue, "0x")) {
      // Special case: Treat float as integer.
      u64 expected = Base::getFromString<u64>(regValue, true);
      if (curThread.FPR[value].asU64() != expected) {
        outRegValue = FMT("{:016X}", curThread.FPR[value].asU64());
        return false;
      }
    } else {
      f64 expected = Base::getFromString<f64>(regValue);
      if (curThread.FPR[value].asDouble() != expected) {
        outRegValue = FMT("{:.17f}", curThread.FPR[value].asDouble());
        return false;
      }
    }
    return true;
  } else if (sscanf(regName, "v%d", &value) == 1) {
    Base::Vector128 expected = Base::getFromString<Base::Vector128>(regValue);
    if (curThread.VR[value] != expected) {
      outRegValue = FMT("[{:08X}, {:08X}, {:08X}, {:08X}]", curThread.VR[value].dsword[0],
          curThread.VR[value].dsword[1], curThread.VR[value].dsword[2], curThread.VR[value].dsword[3]);
      return false;
    }
    return true;
  } else if (std::strcmp(regName, "cr") == 0) {
    u64 actual = curThread.CR.CR_Hex;
    u64 expected = static_cast<u32>(Base::getFromString<u64>(regValue));
    if (actual != expected) {
      outRegValue = FMT("{:016X}", actual);
      return false;
    }
    return true;
  } else {
    LOG_ERROR(Xenon, "[Testing] CompareRegWithString: Unrecognized register name: {}", regName);
    return false;
  }
}

// Searches for a list of tests within a given path.
bool DiscoverTests(const std::filesystem::path &testsPath,
  std::vector<std::filesystem::path> &testFiles) {
  auto filesInPath = Base::FS::ListFilesFromPath(testsPath);
  for (auto &file : filesInPath) {
    if (file.fileName.extension() == ".s") {
      testFiles.push_back(testsPath / file.fileName);
    }
  }
  return true;
}

struct TestCase {
  TestCase(u32 execAddress, std::string &tstName)
    : executionAddress(execAddress), testName(tstName) {
  }
  u32 executionAddress;
  std::string testName;
  AnnotationList testAnnotations;
};

class TestSuite {
public:
  TestSuite(const std::filesystem::path &inSourceFilePath)
    : sourceFilePath(inSourceFilePath) {
    auto name = inSourceFilePath.filename();
    name = name.replace_extension();

    name_ = Base::FS::PathToUTF8String(name);
    mapFilePath = testsBinPath / name.replace_extension(".map");
    binFilePath = testsBinPath / name.replace_extension(".bin");
  }

  bool Load() {
    if (!ReadMap()) {
      LOG_ERROR(Xenon, "[Testing]: Unable to read map for test {}",
        Base::FS::PathToUTF8String(sourceFilePath));
      return false;
    }
    if (!ReadAnnotations()) {
      LOG_ERROR(Xenon, "[Testing]: Unable to read annotations for test {}",
        Base::FS::PathToUTF8String(sourceFilePath));
      return false;
    }
    return true;
  }

  const std::string &name() const { return name_; }
  const std::filesystem::path &inSourceFilePath() const { return sourceFilePath; }
  const std::filesystem::path &getMapFilePath() const { return mapFilePath; }
  const std::filesystem::path &getBinFilePath() const { return binFilePath; }
  std::vector<TestCase> &getTestCases() { return testCases; }

private:
  std::string name_;
  std::filesystem::path sourceFilePath;
  std::filesystem::path mapFilePath;
  std::filesystem::path binFilePath;
  std::vector<TestCase> testCases;

  TestCase *FindTestCase(const std::string_view name) {
    for (auto &testCase : testCases) {
      if (testCase.testName == name) {
        return &testCase;
      }
    }
    return nullptr;
  }

  bool ReadMap() {
    FILE *f = fopen(mapFilePath.string().c_str(), "r");
    if (!f) {
      return false;
    }
    char lineBuffer[BUFSIZ];
    while (fgets(lineBuffer, sizeof(lineBuffer), f)) {
      if (!strlen(lineBuffer)) {
        continue;
      }
      // Format: 0000000000000000 t test_add1\n
      char *newline = strrchr(lineBuffer, '\n');
      if (newline) {
        *newline = 0;
      }
      char *t_test_ = strstr(lineBuffer, " t test_");
      if (!t_test_) {
        continue;
      }
      std::string address(lineBuffer, t_test_ - lineBuffer);
      std::string name(t_test_ + strlen(" t test_"));
      testCases.emplace_back(START_ADDRESS + std::stoul(address, 0, 16),
        name);
    }
    fclose(f);
    return true;
  }

  bool ReadAnnotations() {
    TestCase *currentTestCase = nullptr;
    FILE *f = fopen(sourceFilePath.string().c_str(), "r");
    if (!f) {
      return false;
    }
    char lineBuffer[BUFSIZ];
    while (fgets(lineBuffer, sizeof(lineBuffer), f)) {
      if (!strlen(lineBuffer)) {
        continue;
      }
      // Eat leading whitespace.
      char *start = lineBuffer;
      while (*start == ' ') {
        ++start;
      }
      if (strncmp(start, "test_", strlen("test_")) == 0) {
        // Global test label.
        std::string label(start + strlen("test_"), strchr(start, ':'));
        currentTestCase = FindTestCase(label);
        if (!currentTestCase) {
          LOG_ERROR(Xenon, "[Testing]: Test case {} not found in corresponding map for {}", label,
            Base::FS::PathToUTF8String(sourceFilePath));
          return false;
        }
      } else if (strlen(start) > 3 && start[0] == '#' && start[1] == '_') {
        // Annotation.
        // We don't actually verify anything here.
        char *nextSpace = strchr(start + 3, ' ');
        if (nextSpace) {
          // Looks legit.
          std::string key(start + 3, nextSpace);
          std::string value(nextSpace + 1);
          while (value.find_last_of(" \t\n") == value.size() - 1) {
            value.erase(value.end() - 1);
          }
          if (!currentTestCase) {
            LOG_ERROR(Xenon, "[Testing]: Annotation outside of test case in {}",
              Base::FS::PathToUTF8String(sourceFilePath));
            return false;
          }
          currentTestCase->testAnnotations.emplace_back(key, value);
        }
      }
    }
    fclose(f);
    return true;
  }
};

class TestRunner {
public:
  TestRunner(PPU_STATE *ppuStatePtr) : ppuState(ppuStatePtr) {}

  ~TestRunner() {}

  bool Setup(TestSuite &suite) {
    // Clear the RAM area at tests load address.
    PPCInterpreter::MMUMemSet(ppuState, START_ADDRESS, 0x00000000, 0x1000);

    // Load the test binary into RAM.
    std::vector<u8> testBinData;
    std::ifstream file(suite.getBinFilePath(), std::ios_base::in | std::ios_base::binary);
    if (!file.is_open()) {
      LOG_CRITICAL(Xenon, "[Testing]: Unable to open file: {} for reading. Check your file path.", suite.getBinFilePath().string());
      return false;
    } else {
      u64 fileSize = 0;
      // fs::file_size can cause a exception if it is not a valid file
      try {
        std::error_code ec;
        fileSize = std::filesystem::file_size(suite.getBinFilePath(), ec);
        if (fileSize == -1 || !fileSize) {
          fileSize = 0;
          LOG_ERROR(Base_Filesystem, "[Testing]: Failed to retrieve the file size of {} (Error: {})", 
            suite.getBinFilePath().string(), ec.message());
        }
      } catch (const std::exception &ex) {
        LOG_ERROR(Base_Filesystem, "[Testing]: Exception trying to get file size. Reason: {}", ex.what());
        return false;
      }
      testBinData.resize(fileSize);
      file.read(reinterpret_cast<char*>(testBinData.data()), XE_SROM_SIZE);
      for (int idx = 0; idx < fileSize; ++idx) {
        PPCInterpreter::MMUWrite8(ppuState, START_ADDRESS + idx, testBinData[idx]);
      }
    }
    file.close();
    return true;
  }

  bool Run(TestCase &testCase) {
    // Setup test state from annotations.
    if (!SetupTestState(testCase)) {
      LOG_ERROR(Xenon, "[Testing]: Test setup failed");
      return false;
    }

    // Execute test.

    bool testRunning = true;
    while (testRunning) {
      PPU_THREAD_REGISTERS &thread = ppuState->ppuThread[ppuState->currentThread];
      // Update previous instruction address
      thread.PIA = thread.CIA;
      // Update current instruction address
      thread.CIA = thread.NIA;
      // Increase next instruction address
      thread.NIA += 4;
      // Fetch the instruction from memory
      thread.CI.opcode = PPCInterpreter::MMURead32(ppuState, thread.CIA, ppuState->currentThread);
      if (thread.CI.opcode == 0xFFFFFFFF || thread.CI.opcode == 0xCDCDCDCD) {
        LOG_CRITICAL(Xenon, "[Testing]: Invalid opcode found.");
        return false;
      }
      if (_ex  &PPU_EX_INSSTOR || _ex  &PPU_EX_INSTSEGM) {
        return false;
      }

      if (thread.CI.opcode == BLR_OPCODE) {
        testRunning = false;
      } else {
        PPCInterpreter::ppcExecuteSingleInstruction(ppuState);
      }
    }

    // Assert test state expectations.
    bool testResult = CheckTestResults(testCase);
    if (!testResult) {
      LOG_ERROR(Xenon, "[Testing]: Test result failed.");
    }

    return testResult;
  }

  bool SetupTestState(TestCase &testCase) {
    PPU_THREAD_REGISTERS &thread = ppuState->ppuThread[ppuState->currentThread];
    // Clear registers involved in tests.
    for (auto &reg : thread.GPR) { reg = 0; }
    for (auto &reg : thread.FPR) { reg.setValue(0.0); }
    for (auto &reg : thread.VR) { reg.x = 0; reg.y = 0; reg.z = 0; reg.w = 0; }
    thread.CR.CR_Hex = 0;
    thread.SPR.XER.XER_Hex = 0;

    // Set NIA for this test case.
    thread.NIA = testCase.executionAddress;

    // Set MSR to allow FPU and VXU execution.
    thread.SPR.MSR.FP = 1;
    thread.SPR.MSR.VXU = 1;

    for (auto &it : testCase.testAnnotations) {
      if (it.first == "REGISTER_IN") {
        size_t spacePosition = it.second.find(" ");
        auto regName = it.second.substr(0, spacePosition);
        auto regValue = it.second.substr(spacePosition + 1);
        PPCInterpreter::SetRegFromString(ppuState, regName.c_str(), regValue.c_str());
      } else if (it.first == "MEMORY_IN") {
        size_t spacePos = it.second.find(" ");
        auto addressStr = it.second.substr(0, spacePos);
        auto bytesStr = it.second.substr(spacePos + 1);
        u32 address = std::strtoul(addressStr.c_str(), nullptr, 16);
        auto p = PPCInterpreter::MMUGetPointerFromRAM(address);
        const char *c = bytesStr.c_str();
        while (*c) {
          while (*c == ' ') ++c;
          if (!*c) {
            break;
          }
          char ccs[3] = { c[0], c[1], 0 };
          c += 2;
          u32 b = std::strtoul(ccs, nullptr, 16);
          *p = static_cast<u8>(b);
          ++p;
        }
      }
    }
    return true;
  }

  bool CheckTestResults(TestCase &testCase) {
    bool any_failed = false;
    for (auto &it : testCase.testAnnotations) {
      if (it.first == "REGISTER_OUT") {
        size_t spacePos = it.second.find(" ");
        auto regName = it.second.substr(0, spacePos);
        auto regValue = it.second.substr(spacePos + 1);
        std::string actualValue;
        if (!PPCInterpreter::CompareRegWithString(ppuState,
          regName.c_str(), regValue.c_str(), actualValue)) {
          any_failed = true;
          LOG_ERROR(Xenon, "[Testing]: Register {} assert failed:\n", regName);
          LOG_ERROR(Xenon, "[Testing]:   Expected: {} == {}\n", regName, regValue);
          LOG_ERROR(Xenon, "[Testing]:     Actual: {} == {}\n", regName, actualValue);
        }
      } else if (it.first == "MEMORY_OUT") {
        size_t spacePos = it.second.find(" ");
        auto addressStr = it.second.substr(0, spacePos);
        auto bytesStr = it.second.substr(spacePos + 1);
        u32 address = std::strtoul(addressStr.c_str(), nullptr, 16);
        auto baseAddress = PPCInterpreter::MMUGetPointerFromRAM(address);
        auto p = baseAddress;
        const char *c = bytesStr.c_str();
        bool failed = false;
        size_t count = 0;
        std::string expecteds;
        std::string actuals;
        while (*c) {
          while (*c == ' ') ++c;
          if (!*c) {
            break;
          }
          char ccs[3] = { c[0], c[1], 0 };
          c += 2;
          count++;
          u32 current_address = address + static_cast<u32>(p - baseAddress);
          u32 expected = std::strtoul(ccs, nullptr, 16);
          u8 actual = *p;

          expecteds.append(" {:02X}", expected);
          actuals.append(" {:02X}", actual);

          if (expected != actual) {
            any_failed = true;
            failed = true;
          }
          ++p;
        }
        if (failed) {
          LOG_ERROR(Xenon, "Memory {} assert failed:\n", addressStr);
          LOG_ERROR(Xenon, "  Expected:{}\n", expecteds);
          LOG_ERROR(Xenon, "    Actual:{}\n", actuals);
        }
      }
    }
    return !any_failed;
  }

  PPU_STATE *ppuState;
};

#ifdef _WIN32
int filter(unsigned int code) {
  if (code == EXCEPTION_ILLEGAL_INSTRUCTION) {
    return EXCEPTION_EXECUTE_HANDLER;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif // _WIN32


void ProtectedRunTest(TestSuite &testSuite, TestRunner &runner,
  TestCase &testCase, s32 &failedCount,
  s32 &passedCount) {
#ifdef _WIN32
  __try {
#endif // XE_COMPILER_MSVC

    if (!runner.Setup(testSuite)) {
      LOG_ERROR(Xenon, "[Testing]:     TEST FAILED SETUP");
      ++failedCount;
    }

    if (runner.Run(testCase)) {
      ++passedCount;
    } else {
      LOG_ERROR(Xenon, "[Testing]:     TEST FAILED");
      ++failedCount;
    }

#ifdef _WIN32
  } __except (filter(GetExceptionCode())) {
    LOG_ERROR(Xenon, "[Testing]:     TEST FAILED (UNSUPPORTED INSTRUCTION)");
    ++failedCount;
  }
#endif
}

bool PPCInterpreter::RunTests(PPU_STATE *ppuState) {
  s32 result = 1, failedTestsCount = 0, passedTestsCount = 0;

  // Setup paths.
  testsPath = Config::filepaths.instrTestsPath;
  testsBinPath = Config::filepaths.instrTestsBinPath;

  auto testsPathDir = testsPath;
  std::vector<std::filesystem::path> testFilesList;
  if (!DiscoverTests(testsPathDir, testFilesList)) {
    return false;
  }
  if (!testFilesList.size()) {
    LOG_ERROR(Xenon, "[Testing]: No tests were discovered. Check your path or correct files.");
    return false;
  }
  LOG_INFO(Xenon, "[Testing]: {} tests have been discovered.", testFilesList.size());
  LOG_INFO(Xenon, "");

  std::vector<TestSuite> testSuites;

  bool loadFailed = false;
  for (auto &testPath : testFilesList) {
    TestSuite testSuite(testPath);
    if (!testSuite.Load()) {
      LOG_ERROR(Xenon, "[Testing]: Test suite {} failed to load.", Base::FS::PathToUTF8String(testPath));
      loadFailed = true;
      continue;
    }
    testSuites.push_back(std::move(testSuite));
  }
  if (loadFailed) {
    LOG_ERROR(Xenon, "[Testing]: One or more test suites failed to load.");
  }

  LOG_INFO(Xenon, "[Testing]: {} tests loaded.", testSuites.size());
  TestRunner runner(ppuState);
  for (auto &testSuite : testSuites) {
    LOG_INFO(Xenon, "[Testing]: {}.s:", testSuite.name());
    for (auto &testCase : testSuite.getTestCases()) {
      LOG_INFO(Xenon, "[Testing]:   - {}", testCase.testName);
      ProtectedRunTest(testSuite, runner, testCase, failedTestsCount, passedTestsCount);
    }
    LOG_INFO(Xenon, "");
  }

  LOG_INFO(Xenon, "");
  LOG_INFO(Xenon, "[Testing]: Total tests executed: {}", failedTestsCount + passedTestsCount);
  LOG_INFO(Xenon, "[Testing]: Passed: {}", passedTestsCount);
  LOG_INFO(Xenon, "[Testing]: Failed: {}", failedTestsCount);

  // Reset the state:
  PPU_THREAD_REGISTERS &thread = ppuState->ppuThread[ppuState->currentThread];
  // Clear registers involved in tests.
  for (auto& reg : thread.GPR) { reg = 0; }
  for (auto& reg : thread.FPR) { reg.setValue(0.0); }
  for (auto& reg : thread.VR) { reg.x = 0; reg.y = 0; reg.z = 0; reg.w = 0; }
  thread.CR.CR_Hex = 0;
  thread.SPR.XER.XER_Hex = 0;
  thread.SPR.MSR.FP = 0;
  thread.SPR.MSR.VXU = 0;
  // Set NIA back to default.
  thread.NIA = 0x100;

  return failedTestsCount ? false : true;
}