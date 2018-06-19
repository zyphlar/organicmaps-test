#pragma once

#include "ugc/types.hpp"

#include "coding/read_write_utils.hpp"
#include "coding/reader.hpp"
#include "coding/varint.hpp"
#include "coding/write_to_sink.hpp"

#include "base/exception.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ugc
{
DECLARE_EXCEPTION(SerDesException, RootException);
DECLARE_EXCEPTION(BadBlob, SerDesException);

enum class Version : uint8_t
{
  V0,
  V1,
  Latest = V1
};

template <typename Sink>
class Serializer
{
public:
  Serializer(Sink & sink) : m_sink(sink) {}

  void operator()(uint8_t const d, char const * /* name */ = nullptr) { WriteToSink(m_sink, d); }
  void operator()(uint32_t const d, char const * /* name */ = nullptr) { WriteToSink(m_sink, d); }
  void operator()(uint64_t const d, char const * /* name */ = nullptr) { WriteToSink(m_sink, d); }
  void operator()(std::string const & s, char const * /* name */ = nullptr)
  {
    rw::Write(m_sink, s);
  }

  void VisitRating(float const f, char const * /* name */ = nullptr)
  {
    CHECK_GREATER_OR_EQUAL(f, 0.0, ());
    auto const d = static_cast<uint32_t>(round(f * 10));
    VisitVarUint(d);
  }

  void VisitLang(uint8_t const index, char const * /* name */ = nullptr)
  {
    WriteToSink(m_sink, index);
  }

  void VisitLangs(std::vector<uint8_t> const & indexes, char const * /* name */ = nullptr)
  {
    (*this)(indexes);
  }

  template <typename T>
  void VisitVarUint(T const & t, char const * /* name */ = nullptr)
  {
    WriteVarUint(m_sink, t);
  }

  void operator()(TranslationKey const & key, char const * /* name */ = nullptr)
  {
    (*this)(key.m_key);
  }

  void operator()(Time const & t, char const * /* name */ = nullptr)
  {
    VisitVarUint(ToDaysSinceEpoch(t));
  }

  void operator()(Sentiment sentiment, char const * /* name */ = nullptr)
  {
    switch (sentiment)
    {
    case Sentiment::Negative: return (*this)(static_cast<uint8_t>(0));
    case Sentiment::Positive: return (*this)(static_cast<uint8_t>(1));
    }
  }

  template <typename T>
  void operator()(std::vector<T> const & vs, char const * /* name */ = nullptr)
  {
    VisitVarUint(static_cast<uint32_t>(vs.size()));
    for (auto const & v : vs)
      (*this)(v);
  }

  template <typename T, typename U>
  void operator()(std::pair<T, U> const & p, char const * /* name */ = nullptr)
  {
    (*this)(p.first);
    (*this)(p.second);
  }

  template <typename T, typename U>
  void operator()(std::unordered_map<T, U> const & m, char const * /* name */ = nullptr)
  {
    VisitVarUint(static_cast<uint32_t>(m.size()));
    for (auto const & p : m)
      (*this)(p);
  }

  template <typename R>
  void operator()(R const & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

private:
  Sink & m_sink;
};

template <typename Source>
class DeserializerV0
{
public:
  DeserializerV0(Source & source) : m_source(source) {}

  void operator()(uint8_t & d, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource(m_source, d);
  }
  void operator()(uint32_t & d, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource(m_source, d);
  }
  void operator()(uint64_t & d, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource(m_source, d);
  }
  void operator()(std::string & s, char const * /* name */ = nullptr)
  {
    rw::Read(m_source, s);
  }

  void VisitRating(float & f, char const * /* name */ = nullptr)
  {
    auto const d = DesVarUint<uint32_t>();
    f = static_cast<float>(d) / 10;
  }

  void VisitLang(uint8_t & index, char const * /* name */ = nullptr)
  {
    ReadPrimitiveFromSource<uint8_t>(m_source, index);
  }

  void VisitLangs(std::vector<uint8_t> & indexes, char const * /* name */ = nullptr)
  {
    (*this)(indexes);
  }

  template <typename T>
  void VisitVarUint(T & t, char const * /* name */ = nullptr)
  {
    t = ReadVarUint<T, Source>(m_source);
  }

  template <typename T>
  T DesVarUint()
  {
    return ReadVarUint<T, Source>(m_source);
  }

  void operator()(TranslationKey & key, char const * /* name */ = nullptr) { (*this)(key.m_key); }

  void operator()(Time & t, char const * /* name */ = nullptr)
  {
    t = FromDaysSinceEpoch(DesVarUint<uint32_t>());
  }

  void operator()(Sentiment & sentiment, char const * /* name */ = nullptr)
  {
    uint8_t s = 0;
    (*this)(s);
    switch (s)
    {
    case 0: sentiment = Sentiment::Negative; break;
    case 1: sentiment = Sentiment::Positive; break;
    default: CHECK(false, ("Can't parse sentiment from:", static_cast<int>(s))); break;
    }
  }

  template <typename T>
  void operator()(std::vector<T> & vs, char const * /* name */ = nullptr)
  {
    auto const size = DesVarUint<uint32_t>();
    vs.resize(size);
    for (auto & v : vs)
      (*this)(v);
  }

  template <typename T, typename U>
  void operator()(std::pair<T, U> & p, char const * /* name */ = nullptr)
  {
    (*this)(p.first);
    (*this)(p.second);
  }

  template <typename T, typename U>
  void operator()(std::unordered_map<T, U> & m, char const * /* name */ = nullptr)
  {
    auto const size = DesVarUint<uint32_t>();
    m.reserve(size);
    for (int i = 0; i < size; ++i)
    {
      std::pair<T, U> p;
      (*this)(p);
      m.emplace(p.first, p.second);
    }
  }

  template <typename R>
  void operator()(R & r, char const * /* name */ = nullptr)
  {
    r.Visit(*this);
  }

private:
  Source & m_source;
};

// Version must be used only for testing.
// We should use only the latest version in production.
template <typename Sink, typename UGC>
void Serialize(Sink & sink, UGC const & ugc, Version const version = Version::Latest)
{
  WriteToSink(sink, static_cast<uint8_t>(version));
  Serializer<Sink> ser(sink);
  ser(ugc);
}

template <typename Source>
void Deserialize(Source & source, UGC & ugc)
{
  uint8_t version = 0;
  ReadPrimitiveFromSource(source, version);
  if (version == static_cast<uint8_t>(Version::V0) || version == static_cast<uint8_t>(Version::V1))
  {
    DeserializerV0<Source> des(source);
    des(ugc);
    return;
  }

  MYTHROW(BadBlob, ("Unknown data version:", static_cast<int>(version)));
}

template <typename Source>
void Deserialize(Source & source, UGCUpdate & ugc)
{
  uint8_t version = 0;
  ReadPrimitiveFromSource(source, version);
  if (version == static_cast<uint8_t>(Version::V0))
  {
    DeserializerV0<Source> des(source);
    v0::UGCUpdate old;
    des(old);
    ugc.BuildFrom(old);
    return;
  }

  if (version == static_cast<uint8_t>(Version::V1))
  {
    DeserializerV0<Source> des(source);
    des(ugc);
    return;
  }

  MYTHROW(BadBlob, ("Unknown data version:", static_cast<int>(version)));
}
}  // namespace ugc
