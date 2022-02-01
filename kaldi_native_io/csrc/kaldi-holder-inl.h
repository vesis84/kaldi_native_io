// kaldi_native_io/csrc/kaldi-holder-inl.h
//
// This file is copied/modified from
// https://github.com/kaldi-asr/kaldi/blob/master/src/util/kaldi-holder-inl.h

// Copyright 2009-2011     Microsoft Corporation
//                2016     Johns Hopkins University (author: Daniel Povey)
//                2016     Xiaohui Zhang
//
#ifndef KALDI_NATIVE_IO_CSRC_KALDI_HOLDER_INL_H_
#define KALDI_NATIVE_IO_CSRC_KALDI_HOLDER_INL_H_

#include <iostream>
#include <string>

#include "kaldi_native_io/csrc/io-funcs.h"
#include "kaldi_native_io/csrc/kaldi-utils.h"
#include "kaldi_native_io/csrc/log.h"
#include "kaldi_native_io/csrc/text-utils.h"

namespace kaldiio {

// BasicHolder is valid for float, double, bool, and integer
// types.  There will be a compile time error otherwise, because
// we make sure that the {Write, Read}BasicType functions do not
// get instantiated for other types.

template <class BasicType>
class BasicHolder {
 public:
  typedef BasicType T;

  BasicHolder() : t_(static_cast<T>(-1)) {}

  static bool Write(std::ostream &os, bool binary, const T &t) {
    InitKaldiOutputStream(os, binary);  // Puts binary header if binary mode.
    try {
      WriteBasicType(os, binary, t);
      if (!binary) os << '\n';  // Makes output format more readable and
      // easier to manipulate.
      return os.good();
    } catch (const std::exception &e) {
      KALDIIO_WARN << "Exception caught writing Table object. " << e.what();
      return false;  // Write failure.
    }
  }

  void Clear() {}

  // Reads into the holder.
  bool Read(std::istream &is) {
    bool is_binary;
    if (!InitKaldiInputStream(is, &is_binary)) {
      KALDIIO_WARN
          << "Reading Table object [integer type], failed reading binary"
             " header\n";
      return false;
    }
    try {
      int c;
      if (!is_binary) {  // This is to catch errors, the class would work
        // without it..
        // Eat up any whitespace and make sure it's not newline.
        while (isspace((c = is.peek())) && c != static_cast<int>('\n')) {
          is.get();
        }
        if (is.peek() == '\n') {
          KALDIIO_WARN << "Found newline but expected basic type.";
          return false;  // This is just to catch a more-
          // likely-than average type of error (empty line before the token),
          // since ReadBasicType will eat it up.
        }
      }

      ReadBasicType(is, is_binary, &t_);

      if (!is_binary) {  // This is to catch errors, the class would work
        // without it..
        // make sure there is a newline.
        while (isspace((c = is.peek())) && c != static_cast<int>('\n')) {
          is.get();
        }
        if (is.peek() != '\n') {
          KALDIIO_WARN << "BasicHolder::Read, expected newline, got "
                       << CharToString(is.peek()) << ", position "
                       << is.tellg();
          return false;
        }
        is.get();  // Consume the newline.
      }
      return true;
    } catch (const std::exception &e) {
      KALDIIO_WARN << "Exception caught reading Table object. " << e.what();
      return false;
    }
  }

  // Objects read/written with the Kaldi I/O functions always have the stream
  // open in binary mode for reading.
  static bool IsReadInBinary() { return true; }

  T &Value() { return t_; }

  void Swap(BasicHolder<T> *other) { std::swap(t_, other->t_); }

  bool ExtractRange(const BasicHolder<T> &other, const std::string &range) {
    KALDIIO_ERR << "ExtractRange is not defined for this type of holder.";
    return false;
  }

  ~BasicHolder() {}

 private:
  KALDIIO_DISALLOW_COPY_AND_ASSIGN(BasicHolder);

  T t_;
};

/// A Holder for a vector of basic types, e.g.
/// std::vector<int32_t>, std::vector<float>, and so on.
/// Note: a basic type is defined as a type for which ReadBasicType
/// and WriteBasicType are implemented, i.e. integer and floating
/// types, and bool.
template <class BasicType>
class BasicVectorHolder {
 public:
  typedef std::vector<BasicType> T;

  BasicVectorHolder() {}

  static bool Write(std::ostream &os, bool binary, const T &t) {
    InitKaldiOutputStream(os, binary);  // Puts binary header if binary mode.
    try {
      if (binary) {  // need to write the size, in binary mode.
        KALDIIO_ASSERT(static_cast<size_t>(static_cast<int32_t>(t.size())) ==
                       t.size());
        // Or this Write routine cannot handle such a large vector.
        // use int32_t because it's fixed size regardless of compilation.
        // change to int64 (plus in Read function) if this becomes a problem.
        WriteBasicType(os, binary, static_cast<int32_t>(t.size()));
        for (typename std::vector<BasicType>::const_iterator iter = t.begin();
             iter != t.end(); ++iter)
          WriteBasicType(os, binary, *iter);

      } else {
        for (typename std::vector<BasicType>::const_iterator iter = t.begin();
             iter != t.end(); ++iter)
          WriteBasicType(os, binary, *iter);
        os << '\n';  // Makes output format more readable and
        // easier to manipulate.  In text mode, this function writes something
        // like "1 2 3\n".
      }
      return os.good();
    } catch (const std::exception &e) {
      KALDIIO_WARN << "Exception caught writing Table object (BasicVector). "
                   << e.what();
      return false;  // Write failure.
    }
  }

  void Clear() { t_.clear(); }

  // Reads into the holder.
  bool Read(std::istream &is) {
    t_.clear();
    bool is_binary;
    if (!InitKaldiInputStream(is, &is_binary)) {
      KALDIIO_WARN
          << "Reading Table object [integer type], failed reading binary"
             " header\n";
      return false;
    }
    if (!is_binary) {
      // In text mode, we terminate with newline.
      std::string line;
      getline(is, line);  // this will discard the \n, if present.
      if (is.fail()) {
        KALDIIO_WARN << "BasicVectorHolder::Read, error reading line "
                     << (is.eof() ? "[eof]" : "");
        return false;  // probably eof.  fail in any case.
      }
      std::istringstream line_is(line);
      try {
        while (1) {
          line_is >> std::ws;  // eat up whitespace.
          if (line_is.eof()) break;
          BasicType bt;
          ReadBasicType(line_is, false, &bt);
          t_.push_back(bt);
        }
        return true;
      } catch (const std::exception &e) {
        KALDIIO_WARN << "BasicVectorHolder::Read, could not interpret line: "
                     << "'" << line << "'"
                     << "\n"
                     << e.what();
        return false;
      }
    } else {  // binary mode.
      size_t filepos = is.tellg();
      try {
        int32_t size;
        ReadBasicType(is, true, &size);
        t_.resize(size);
        for (typename std::vector<BasicType>::iterator iter = t_.begin();
             iter != t_.end(); ++iter) {
          ReadBasicType(is, true, &(*iter));
        }
        return true;
      } catch (...) {
        KALDIIO_WARN << "BasicVectorHolder::Read, read error or unexpected data"
                        " at archive entry beginning at file position "
                     << filepos;
        return false;
      }
    }
  }

  // Objects read/written with the Kaldi I/O functions always have the stream
  // open in binary mode for reading.
  static bool IsReadInBinary() { return true; }

  T &Value() { return t_; }

  void Swap(BasicVectorHolder<BasicType> *other) { t_.swap(other->t_); }

  bool ExtractRange(const BasicVectorHolder<BasicType> &other,
                    const std::string &range) {
    KALDIIO_ERR << "ExtractRange is not defined for this type of holder.";
    return false;
  }

  ~BasicVectorHolder() {}

 private:
  KALDIIO_DISALLOW_COPY_AND_ASSIGN(BasicVectorHolder);
  T t_;
};

// We define a Token as a nonempty, printable, whitespace-free std::string.
// The binary and text formats here are the same (newline-terminated)
// and as such we don't bother with the binary-mode headers.
class TokenHolder {
 public:
  typedef std::string T;

  TokenHolder() {}

  static bool Write(std::ostream &os, bool, const T &t) {  // ignore binary-mode
    KALDIIO_ASSERT(IsToken(t));
    os << t << '\n';
    return os.good();
  }

  void Clear() { t_.clear(); }

  // Reads into the holder.
  bool Read(std::istream &is) {
    is >> t_;
    if (is.fail()) return false;
    char c;
    while (isspace(c = is.peek()) && c != '\n') is.get();
    if (is.peek() != '\n') {
      KALDIIO_WARN << "TokenHolder::Read, expected newline, got char "
                   << CharToString(is.peek()) << ", at stream pos "
                   << is.tellg();
      return false;
    }
    is.get();  // get '\n'
    return true;
  }

  // Since this is fundamentally a text format, read in text mode (would work
  // fine either way, but doing it this way will exercise more of the code).
  static bool IsReadInBinary() { return false; }

  T &Value() { return t_; }

  ~TokenHolder() {}

  void Swap(TokenHolder *other) { t_.swap(other->t_); }

  bool ExtractRange(const TokenHolder &other, const std::string &range) {
    KALDIIO_ERR << "ExtractRange is not defined for this type of holder.";
    return false;
  }

 private:
  KALDIIO_DISALLOW_COPY_AND_ASSIGN(TokenHolder);
  T t_;
};

}  // namespace kaldiio

#endif  // KALDI_NATIVE_IO_CSRC_KALDI_HOLDER_INL_H_
