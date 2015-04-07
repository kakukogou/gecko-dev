/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/webappapis.html#images
 */

typedef (HTMLImageElement or
         HTMLVideoElement or
         HTMLCanvasElement or
         Blob or
         ImageData or
         CanvasRenderingContext2D or
         ImageBitmap) ImageBitmapSource;

[Exposed=(Window,Worker)]
interface ImageBitmap {
 [Constant]
 readonly attribute unsigned long width;
 [Constant]
 readonly attribute unsigned long height;
};

[NoInterfaceObject, Exposed=(Window,Worker)]
interface ImageBitmapFactories {
  [Throws]
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource aImage);
  [Throws]
  Promise<ImageBitmap> createImageBitmap(ImageBitmapSource aImage, long aSx, long aSy, long aSw, long aSh);
};

// Mozilla Extensions
//
// Bug 1141979 - [FoxEye] Extend ImageBitmap with interfaces to access its
// underlying image data
//
// Summary:
// This is a sub-bug of project FoxEye which's goal is to streamline the video
// processing framework on the web platform. It provides web applications a way
// to hook a script (ran in a Web Worker) to a video track (of a MediaStream)
// so that each frame will be processed accordingly.
//
// We choose ImageBitmap (instead of ImageData) as the container of video frames
// because the decoded video frame data might exist in either CPU or GPU memory.
// ImageBitmap is designed as an opaque handler so that users do not need to
// know where the real buffer is.
//
// Considering how would developers process video frames, there are two possible
// ways, via JavaScript(/asm.js) or WebGL.
// (1) If users use WebGL, then treating an ImageBitmap as an opaque container
//     and passing it into WebGL.
// (2) If users use JavaScript(/asm.js) to process the frames, then new APIs to
//     access the raw data inside an ImageBitmp are needed.
// The 2nd scenario above is the one which I would like to enable in this
// extension set.
//
//
//
// Color format:
// Image/Video data might exists in a variety of color formats. Developers need
// to know how the data is formated so that they are able to process it.
// Information that we need to know about a color format includes color space,
// channel permutation and the pixel layout. A color space is the space used to
// express colors; the space is constructed by coordinates which is called color
// channels. An image or video frame is composed by pixels and each pixel's
// color is represented by values from all channels of a given color space. The
// pixel values might be arranged in a planar way or interleaving way:
// (1) Planar layout: each channel has its pixel values stored consecutively in
//     separated buffers (/planes) and channel buffers are stored consecutively
//     in memory.
//     (Ex: RRRRRR......GGGGGG......BBBBBB......)
// (2) Interleaving layout: each pixel has its pixel values from all channels
//     stored together and interleaves all channels.
//     (Ex: RGBRGBRGBRGBRGB......)
//
// Note: color formats belong to the same color space might have different pixel
//       layouts.
//
//
//
// Components:
// (*) An enumeration 'ColorFormat' defines a list of color formats which are
// exposed to users. The extend APIs of ImageBitmap use this enumeration to
// negotiate the format while accessing the underlying data of ImageBitmap.
//
// (*) Two interfaces, 'ColorFormatPixelLayout' and 'ChannelPixelLayout', help
// together to generalize the variety of pixel layouts among color formats.
//
// The 'ColorFormatPixelLayout' represents the pixel layout of a certain color
// space and since a color format is composed by at least one channel so
// 'ColorFormatPixelLayout' contains at least one 'ChannelPixelLayout'.
//
// Although Image/Video frame is a two-dimensional structure, its data is
// usually stored in an one-dimensional array and 'ChannelPixelLayout'
// use the following properties to describe the layout of pixel values in the
// buffer.
// (1) 'offset': where is each channel's data starts from. (Relative to the
//               beginning of the video data one-dimension array.)
// (2) 'width' and 'height': how much samples are in each channel.
// (3) 'stride': the padding of each row. (Memory alignment purpose.)
// (4) 'skip': this is used to describe interleaving layout. (For planar layout,
//             this property will be zero.)
//
//
//
// Example1: RGBA image, width = 620, height = 480, stride = 2560
// chanel_r: offset = 0, width = 620, height = 480, stride = 2560, skip = 3
// chanel_g: offset = 1, width = 620, height = 480, stride = 2560, skip = 3
// chanel_b: offset = 2, width = 620, height = 480, stride = 2560, skip = 3
// chanel_a: offset = 3, width = 620, height = 480, stride = 2560, skip = 3
//
//         <---------------------------- stride ---------------------------->
//         <---------------------- width x 4 ---------------------->
// [index] 01234   8   12  16  20  24  28                           2479    2559
//         |||||---|---|---|---|---|---|----------------------------|-------|
// [data]  RGBARGBARGBARGBARGBAR___R___R...                         A%%%%%%%%
// [data]  RGBARGBARGBARGBARGBAR___R___R...                         A%%%%%%%%
// [data]  RGBARGBARGBARGBARGBAR___R___R...                         A%%%%%%%%
//              ^^^
//              r-skip
//
//
//
//
//
// Example2: YUV420P image, width = 620, height = 480, stride = 640
// chanel_y: offset = 0, width = 620, height = 480, stride = 640, skip = 0
// chanel_u: offset = 307200, width = 310, height = 240, stride = 320, skip = 0
// chanel_v: offset = 384000, width = 310, height = 240, stride = 320, skip = 0
//
//         <--------------------------- y-stride --------------------------->
//         <----------------------- y-width ----------------------->
// [index] 012345                                                  619      639
//         ||||||--------------------------------------------------|--------|
// [data]  YYYYYYYYYYYYYYYYYYYYYYYYYYYYY...                        Y%%%%%%%%%
// [data]  YYYYYYYYYYYYYYYYYYYYYYYYYYYYY...                        Y%%%%%%%%%
// [data]  YYYYYYYYYYYYYYYYYYYYYYYYYYYYY...                        Y%%%%%%%%%
// [data]  ......
//         <-------- u-stride ---------->
//         <----- u-width ----->
// [index] 307200              307509   307519
//         |-------------------|--------|
// [data]  UUUUUUUUUU...       U%%%%%%%%%
// [data]  UUUUUUUUUU...       U%%%%%%%%%
// [data]  UUUUUUUUUU...       U%%%%%%%%%
// [data]  ......
//         <-------- v-stride ---------->
//         <- --- v-width ----->
// [index] 384000              384309   384319
//         |-------------------|--------|
// [data]  VVVVVVVVVV...       V%%%%%%%%%
// [data]  VVVVVVVVVV...       V%%%%%%%%%
// [data]  VVVVVVVVVV...       V%%%%%%%%%
// [data]  ......
//
//
//
//
//
// Example3: YUV420SP_NV12 image, width = 620, height = 480, stride = 640
// chanel_y: offset = 0, width = 620, height = 480, stride = 640, skip = 0
// chanel_u: offset = 307200, width = 310, height = 240, stride = 640, skip = 1
// chanel_v: offset = 307201, width = 310, height = 240, stride = 640, skip = 1
//
//         <--------------------------- y-stride -------------------------->
//         <----------------------- y-width ---------------------->
// [index] 012345                                                 619      639
//         ||||||-------------------------------------------------|--------|
// [data]  YYYYYYYYYYYYYYYYYYYYYYYYYYYYY...                       Y%%%%%%%%%
// [data]  YYYYYYYYYYYYYYYYYYYYYYYYYYYYY...                       Y%%%%%%%%%
// [data]  YYYYYYYYYYYYYYYYYYYYYYYYYYYYY...                       Y%%%%%%%%%
// [data]  ......
//         <--------------------- u-stride / v-stride -------------------->
//         <------------------ u-width + v-width ----------------->
// [index] 307200(u-offset)                                       307819  307839
//         |------------------------------------------------------|-------|
// [index] |307201(v-offset)                                      |307820 |
//         ||-----------------------------------------------------||------|
// [data]  UVUVUVUVUVUVUVUVUVUVUVUVUVUVUV...                      UV%%%%%%%
// [data]  UVUVUVUVUVUVUVUVUVUVUVUVUVUVUV...                      UV%%%%%%%
// [data]  UVUVUVUVUVUVUVUVUVUVUVUVUVUVUV...                      UV%%%%%%%
//          ^            ^
//         u-skip        v-skip
//
//
//
//
//
// (*) Three methods are added into ImageBitmap for accessing the underlying
// data of a ImageBitmap object. See methods' comments for details.
//






/*
 * The list of supported color formats.
 * TODO: We need to elaborate this list for standardization.
 */
enum ColorFormat {
  /*
   * Channel order: R, G, B, A
   * Channel size: full rgba-chennels
   * Pixel layout: interleaving rgba-channels
   */
  "RGBA32",
  /*
   * Channel order: B, G, R, A
   * Channel size: full bgra-channels
   * Pixel layout: interleaving bgra-channels
   */
  "BGRA32",
  /*
   * Channel order: R, G, B
   * Channel size: full rgb-channels
   * Pixel layout: interleaving rgb-channels
   */
  "RGB24",
  /*
   * Channel order: B, G, R
   * Channel size: full bgr-channels
   * Pixel layout: interleaving bgr-channels
   */
  "BGR24",
  /*
   * Channel order: GRAY
   * Channel size: full gray-channel
   * Pixel layout: planar gray-channel
   */
  "GRAY8",
  /*
   * Channel order: Y, U, V
   * Channel size: full yuv-channels
   * Pixel layout: planar yuv-channels
   */
  "YUV444P",
  /*
   * Channel order: Y, U, V
   * Channel size: full y-channel, half uv-channels
   * Pixel layout: planar yuv-channels
   */
  "YUV422P",
  /*
   * Channel order: Y, U, V
   * Channel size: full yuv-channel, quarter uv-channels
   * Pixel layout: planar yuv-channels
   */
  "YUV420P",
  /*
   * Channel order: Y, U, V
   * Channel size: full yuv-channel, quarter uv-channels
   * Pixel layout: planar y-channel, interleaving uv-channels
   */
  "YUV420SP_NV12",
  /*
   * Channel order: Y, V, U
   * Channel size: full yuv-channel, quarter uv-channels
   * Pixel layout: planar y-channel, interleaving vu-channels
   */
  "YUV420SP_NV21",
  /*
   * Channel order: H, S, V
   * Channel size: full hsv-channels
   * Pixel layout: interleaving hsv-channels
   */
  "HSV",
  /*
   * Channel order: l, a, b
   * Channel size: full lab-channels
   * Pixel layout: interleaving lab-channels
   */
  "Lab",
  /*
   * empty string for error reporting
   */
  ""
};

[Exposed=(Window,Worker)]
interface ChannelPixelLayout
{
  /*
   * The beginning position of this channel's data (relative to the given
   * ArrayBuffer parameter of the mapDataInto() method.)
   */
  [Constant]
  readonly attribute unsigned long offset;

  /*
   * The width of this channel.
   * Channels in a color format may have different width.
   */
  [Constant]
  readonly attribute unsigned long width;

  /*
   * The height of this channel.
   * Channels in a color format may have different height.
   */
  [Constant]
  readonly attribute unsigned long height;

  /*
   * The stride of this channel.
   * This is used to describe the padding of this channel.
   */
  [Constant]
  readonly attribute unsigned long stride;

  /*
   * This is used to describe how much bytes between two adjacent pixel values
   * in this channel.
   * Possible values:
   *   - zero: for planar format.
   *   - a positive integer: for interleaving format.
   */
  [Constant]
  readonly attribute unsigned long skip;
};

[Exposed=(Window,Worker)]
interface ColorFormatPixelLayout
{
  /*
   * Channel information of this color format.
   * Each color format has at least one channel.
   */
  [Constant, Cached] // use [StoreInSlot]?
  readonly attribute sequence<ChannelPixelLayout> channels;
};

partial interface ImageBitmap {
  /*
   * Find the best color format for receiving data.
   *
   * @param possibleFormats
   *        A list of color formats that users can handler.
   *        This is optional.
   *
   * @return one of the possibleFormats or the empty string if no format in the
   *         list is supported. If the possibleFormats is not given, then
   *         returns the best color format for this ImageBitmap from all
   *         supported color formats
   */
  ColorFormat findOptimalFormat(optional sequence<ColorFormat> possibleFormats);

  /*
   * Calculate the length of mapped data wile the image is represented
   * in the given format.
   *
   * Throws if 'format' is not supported.
   *
   * @param format
   *        The format that users want.
   *
   * @return the length (in bytes) of image data the represented in the given
   *         format.
   */
  [Throws]
  long mappedDataLength(ColorFormat format);

  /*
   * Makes a copy of the underlying image data in the given format 'format' to
   * 'buffer' at offset 'offset', filling at most 'length' bytes and returns a
   * ColorFormatPixelLayout object which describes the pixel layout.
   *
   * Throws if 'format' is not supported.
   *
   * Each time this method is invoked returns a new ColorFormatPixelLayout
   * object.
   *
   * @param format
   *        The format that users want.
   * @param buffer
   *        A container for receiving the mapped image data.
   * @param offset
   *        The beginning position of the 'buffer' to place the mapped data.
   * @param length
   *        The length of space in 'buffer' that could be filled.
   *
   * @return a ColorFormatPixelLayout object which describes the pixel layout.
   */
  [Throws]
  ColorFormatPixelLayout mapDataInto(ColorFormat format, ArrayBuffer buffer, long offset, long length);

  /*
   *
   */
   [Throws]
   boolean setDataFrom(ColorFormat aFormat, ArrayBuffer aBuffer, long aOffset, long aLength, long aWidth, long aHeight, long aStride);
};