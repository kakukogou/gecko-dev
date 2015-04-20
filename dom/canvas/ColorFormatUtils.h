/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ColorFormatUtils_h
#define mozilla_dom_ColorFormatUtils_h

#include "mozilla/dom/ImageBitmapBinding.h"

namespace mozilla {
namespace dom {

template<ColorFormat colorType>
class ColorFormatTrait;

template<>
class ColorFormatTrait<ColorFormat::RGBA32>
{
public:
  static unsigned char const channels = 4;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::BGRA32>
{
public:
  static unsigned char const channels = 4;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::RGB24>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::BGR24>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::GRAY8>
{
public:
  static unsigned char const channels = 1;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::YUV444P>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::YUV422P>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::YUV420P>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::YUV420SP_NV12>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::YUV420SP_NV21>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::HSV>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::Lab>
{
public:
  static unsigned char const channels = 3;
  static unsigned char const bytesPerPixel = channels * 8;
};

template<>
class ColorFormatTrait<ColorFormat::_empty>
{
public:
  static unsigned char const channels = 0;
  static unsigned char const bytesPerPixel = 0;
};

static uint8_t
getChannelCountOfColorFormat(ColorFormat format) {
  switch (format) {
  case ColorFormat::RGBA32:
    return ColorFormatTrait<ColorFormat::RGBA32>::channels;
  case ColorFormat::BGRA32:
    return ColorFormatTrait<ColorFormat::BGRA32>::channels;
  case ColorFormat::RGB24:
    return ColorFormatTrait<ColorFormat::RGB24>::channels;
  case ColorFormat::BGR24:
    return ColorFormatTrait<ColorFormat::BGR24>::channels;
  case ColorFormat::GRAY8:
    return ColorFormatTrait<ColorFormat::GRAY8>::channels;
  case ColorFormat::YUV444P:
    return ColorFormatTrait<ColorFormat::YUV444P>::channels;
  case ColorFormat::YUV422P:
    return ColorFormatTrait<ColorFormat::YUV422P>::channels;
  case ColorFormat::YUV420P:
    return ColorFormatTrait<ColorFormat::YUV420P>::channels;
  case ColorFormat::YUV420SP_NV12:
    return ColorFormatTrait<ColorFormat::YUV420SP_NV12>::channels;
  case ColorFormat::YUV420SP_NV21:
    return ColorFormatTrait<ColorFormat::YUV420SP_NV21>::channels;
  case ColorFormat::HSV:
    return ColorFormatTrait<ColorFormat::HSV>::channels;
  case ColorFormat::Lab:
    return ColorFormatTrait<ColorFormat::Lab>::channels;
  case ColorFormat::_empty:
    return ColorFormatTrait<ColorFormat::_empty>::channels;
  default:
    return 0;
  }
}

static unsigned char
getBytesPerOixelOfColorFormat(ColorFormat format) {
  switch (format) {
  case ColorFormat::RGBA32:
    return ColorFormatTrait<ColorFormat::RGBA32>::bytesPerPixel;
  case ColorFormat::YUV444P:
    return ColorFormatTrait<ColorFormat::YUV444P>::bytesPerPixel;
  case ColorFormat::GRAY8:
    return ColorFormatTrait<ColorFormat::GRAY8>::bytesPerPixel;
  case ColorFormat::HSV:
    return ColorFormatTrait<ColorFormat::HSV>::bytesPerPixel;
  case ColorFormat::Lab:
    return ColorFormatTrait<ColorFormat::Lab>::bytesPerPixel;
  case ColorFormat::_empty:
    return ColorFormatTrait<ColorFormat::_empty>::bytesPerPixel;
  default:
    return 0;
  }
}

}
}

#endif
