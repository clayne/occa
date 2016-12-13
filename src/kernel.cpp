/* The MIT License (MIT)
 *
 * Copyright (c) 2014-2016 David Medina and Tim Warburton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */

#include "occa/kernel.hpp"
#include "occa/device.hpp"
#include "occa/memory.hpp"
#include "occa/uva.hpp"
#include "occa/tools/io.hpp"
#include "occa/tools/sys.hpp"

namespace occa {
  //---[ KernelArg ]--------------------
  kernelArg_t::kernelArg_t() {
    dHandle = NULL;
    mHandle = NULL;

    ::memset(&data, 0, sizeof(data));
    size = 0;
    info = kArgInfo::none;
  }

  kernelArg_t::kernelArg_t(const kernelArg_t &k) {
    *this = k;
  }

  kernelArg_t& kernelArg_t::operator = (const kernelArg_t &k) {
    dHandle = k.dHandle;
    mHandle = k.mHandle;

    ::memcpy(&data, &(k.data), sizeof(data));
    size = k.size;
    info = k.info;

    return *this;
  }

  kernelArg_t::~kernelArg_t() {}

  void* kernelArg_t::ptr() const {
    return ((info & kArgInfo::usePointer) ? data.void_ : (void*) &data);
  }

  kernelArg::kernelArg() {}
  kernelArg::~kernelArg() {}

  kernelArg::kernelArg(kernelArg_t &arg_) :
    arg(arg_) {}

  kernelArg::kernelArg(const kernelArg &k) :
    arg(k.arg),
    extraArgs(k.extraArgs) {}

  kernelArg& kernelArg::operator = (const kernelArg &k) {
    arg = k.arg;
    extraArgs = k.extraArgs;
    return *this;
  }

  void kernelArg::setupFrom(void *arg_,
                            bool lookAtUva, bool argIsUva) {

    setupFrom(arg_, sizeof(void*), lookAtUva, argIsUva);
  }

  void kernelArg::setupFrom(void *arg_, size_t bytes,
                            bool lookAtUva, bool argIsUva) {

    memory_v *mHandle = NULL;
    if (argIsUva) {
      mHandle = (memory_v*) arg_;
    } else if (lookAtUva) {
      ptrRangeMap_t::iterator it = uvaMap.find(arg_);
      if (it != uvaMap.end()) {
        mHandle = it->second;
      }
    }

    arg.info = kArgInfo::usePointer;
    arg.size = bytes;

    if (mHandle) {
      arg.mHandle = mHandle;
      arg.dHandle = mHandle->dHandle;

      arg.data.void_ = mHandle->handle;
    } else {
      arg.data.void_ = arg_;
    }
  }

  template <>
  kernelArg::kernelArg(const uint8_t &arg_) {
    arg.data.uint8_ = arg_; arg.size = sizeof(uint8_t);
  }

  template <>
  kernelArg::kernelArg(const uint16_t &arg_) {
    arg.data.uint16_ = arg_; arg.size = sizeof(uint16_t);
  }

  template <>
  kernelArg::kernelArg(const uint32_t &arg_) {
    arg.data.uint32_ = arg_; arg.size = sizeof(uint32_t);
  }

  template <>
  kernelArg::kernelArg(const uint64_t &arg_) {
    arg.data.uint64_ = arg_; arg.size = sizeof(uint64_t);
  }

  template <>
  kernelArg::kernelArg(const int8_t &arg_) {
    arg.data.int8_ = arg_; arg.size = sizeof(int8_t);
  }

  template <>
  kernelArg::kernelArg(const int16_t &arg_) {
    arg.data.int16_ = arg_; arg.size = sizeof(int16_t);
  }

  template <>
  kernelArg::kernelArg(const int32_t &arg_) {
    arg.data.int32_ = arg_; arg.size = sizeof(int32_t);
  }

  template <>
  kernelArg::kernelArg(const int64_t &arg_) {
    arg.data.int64_ = arg_; arg.size = sizeof(int64_t);
  }

  template <>
  kernelArg::kernelArg(const float &arg_) {
    arg.data.float_ = arg_; arg.size = sizeof(float);
  }

  template <>
  kernelArg::kernelArg(const double &arg_) {
    arg.data.double_ = arg_; arg.size = sizeof(double);
  }

  occa::device kernelArg::getDevice() const {
    return occa::device(arg.dHandle);
  }

  void kernelArg::setupForKernelCall(const bool isConst) const {
    occa::memory_v *mHandle = arg.mHandle;

    if (mHandle                      &&
        mHandle->isManaged()         &&
        !mHandle->leftInDevice()     &&
        mHandle->dHandle->fakesUva() &&
        mHandle->dHandle->hasUvaEnabled()) {

      if (!mHandle->inDevice()) {
        mHandle->copyFrom(mHandle->uvaPtr, mHandle->size);
        mHandle->memInfo |= uvaFlag::inDevice;
      }
      if (!isConst && !mHandle->isStale()) {
        uvaStaleMemory.push_back(mHandle);
        mHandle->memInfo |= uvaFlag::isStale;
      }
    }
  }

  int kernelArg::argumentCount(const int kArgc, const kernelArg *kArgs) {
    int argc = kArgc;
    for (int i = 0; i < kArgc; ++i) {
      argc += kArgs[i].extraArgs.size();
    }
    return argc;
  }
  //====================================


  //---[ kernelInfo ]---------------------

  //  |---[ Kernel Info ]-------------------------
  kernelInfo::kernelInfo() :
    occa::properties() {}

  kernelInfo::kernelInfo(const properties &props) :
    occa::properties(props) {}

  bool kernelInfo::isAnOccaDefine(const std::string &name) {
    return ((name == "occaInnerDim0") ||
            (name == "occaInnerDim1") ||
            (name == "occaInnerDim2") ||

            (name == "occaOuterDim0") ||
            (name == "occaOuterDim1") ||
            (name == "occaOuterDim2"));
  }

  void kernelInfo::addIncludeDefine(const std::string &filename) {
    props["headers"] += "#include \"" + filename + "\"";
  }

  void kernelInfo::addInclude(const std::string &filename) {
    props["headers"] += io::read(filename);
  }

  void kernelInfo::removeDefine(const std::string &macro) {
    if (!isAnOccaDefine(macro)) {
      props["headers"] += "#undef " + macro;
    }
  }

  void kernelInfo::addSource(const std::string &content) {
    props["headers"] += content;
  }
  //====================================


  //---[ kernel_v ]---------------------
  kernel_v::kernel_v(const occa::properties &properties_) {
    dHandle = NULL;

    properties = properties_;

    dims  = 1;
    inner = occa::dim(1,1,1);
    outer = occa::dim(1,1,1);
  }

  kernel_v::~kernel_v() {}

  void kernel_v::initFrom(const kernel_v &m) {
    dHandle = m.dHandle;

    name = m.name;
    properties = m.properties;

    metadata = m.metadata;

    dims = m.dims;
    inner = m.inner;
    outer = m.outer;

    nestedKernels = m.nestedKernels;
    arguments = m.arguments;
  }

  kernel* kernel_v::nestedKernelsPtr() {
    return &(nestedKernels[0]);
  }

  int kernel_v::nestedKernelCount() {
    return (int) nestedKernels.size();
  }

  kernelArg* kernel_v::argumentsPtr() {
    return &(arguments[0]);
  }

  int kernel_v::argumentCount() {
    return (int) arguments.size();
  }

  std::string kernel_v::binaryName(const std::string &filename) {
    return filename;
  }

  std::string kernel_v::getSourceFilename(const std::string &filename, hash_t &hash) {
    return io::hashDir(filename, hash) + kc::sourceFile;
  }

  std::string kernel_v::getBinaryFilename(const std::string &filename, hash_t &hash) {
    return io::hashDir(filename, hash) + binaryName(kc::binaryFile);
  }
  //====================================

  //---[ kernel ]-----------------------
  kernel::kernel() :
    kHandle(NULL) {}

  kernel::kernel(kernel_v *kHandle_) :
    kHandle(kHandle_) {}

  kernel::kernel(const kernel &k) :
    kHandle(k.kHandle) {}

  kernel& kernel::operator = (const kernel &k) {
    kHandle = k.kHandle;
    return *this;
  }

  void kernel::checkIfInitialized() const {
    OCCA_CHECK(kHandle != NULL,
               "Kernel is not initialized");
  }

  void* kernel::getHandle(const occa::properties &props) {
    checkIfInitialized();
    return kHandle->getHandle(props);
  }

  kernel_v* kernel::getKHandle() {
    checkIfInitialized();
    return kHandle;
  }

  const std::string& kernel::mode() {
    checkIfInitialized();
    return device(kHandle->dHandle).mode();
  }

  const std::string& kernel::name() {
    checkIfInitialized();
    return kHandle->name;
  }

  const std::string& kernel::sourceFilename() {
    checkIfInitialized();
    return kHandle->sourceFilename;
  }

  const std::string& kernel::binaryFilename() {
    checkIfInitialized();
    return kHandle->binaryFilename;
  }

  occa::device kernel::getDevice() {
    checkIfInitialized();
    return occa::device(kHandle->dHandle);
  }

  void kernel::setWorkingDims(int dims, occa::dim inner, occa::dim outer) {
    checkIfInitialized();

    for (int i = 0; i < dims; ++i) {
      inner[i] += (inner[i] ? 0 : 1);
      outer[i] += (outer[i] ? 0 : 1);
    }

    for (int i = dims; i < 3; ++i)
      inner[i] = outer[i] = 1;

    if (kHandle->nestedKernelCount()) {
      for (int k = 0; k < kHandle->nestedKernelCount(); ++k) {
        kHandle->nestedKernels[k].setWorkingDims(dims, inner, outer);
      }
    } else {
      kHandle->dims  = dims;
      kHandle->inner = inner;
      kHandle->outer = outer;
    }
  }

  int kernel::maxDims() {
    checkIfInitialized();
    return kHandle->maxDims();
  }

  dim kernel::maxOuterDims() {
    checkIfInitialized();
    return kHandle->maxOuterDims();
  }

  dim kernel::maxInnerDims() {
    checkIfInitialized();
    return kHandle->maxInnerDims();
  }

  void kernel::addArgument(const int argPos, const kernelArg &arg) {
    checkIfInitialized();

    if (kHandle->argumentCount() <= argPos) {
      OCCA_CHECK(argPos < OCCA_MAX_ARGS,
                 "Kernels can only have at most [" << OCCA_MAX_ARGS << "] arguments,"
                 << " [" << argPos << "] arguments were set");

      kHandle->arguments.reserve(argPos + 1);
    }

    kHandle->arguments.insert(kHandle->arguments.begin() + argPos, arg);
  }

  void kernel::runFromArguments() {
    checkIfInitialized();

    const int argc = (int) kHandle->arguments.size();
    for (int i = 0; i < argc; ++i) {
      const bool argIsConst = kHandle->metadata.argIsConst(i);
      kHandle->arguments[i].setupForKernelCall(argIsConst);
    }

    // Add nestedKernels
    if (kHandle->nestedKernelCount()) {
      kHandle->arguments.insert(kHandle->arguments.begin(),
                                kHandle->nestedKernelsPtr());
    }

    kHandle->runFromArguments(kHandle->argumentCount(),
                              kHandle->argumentsPtr());

    // Remove nestedKernels
    if (kHandle->nestedKernelCount()) {
      kHandle->arguments.erase(kHandle->arguments.begin());
    }
  }

  void kernel::clearArgumentList() {
    checkIfInitialized();
    kHandle->arguments.clear();
  }

#include "operators/definitions.cpp"

  void kernel::free() {
    checkIfInitialized();

    if (kHandle->nestedKernelCount()) {
      for (int k = 0; k < kHandle->nestedKernelCount(); ++k)
        kHandle->nestedKernels[k].free();
    }

    kHandle->free();

    delete kHandle;
    kHandle = NULL;
  }
  //====================================
}