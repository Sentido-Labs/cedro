/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*-
 * vi: set et ts=2 sw=2: */
/** \file */
/** \mainpage
 * UTF-8 codec functions extracted from the Cedro C Preprocessor.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Created: 2020-11-25 22:41
 */

typedef enum UTF8Error {
  UTF8_NO_ERROR, UTF8_ERROR, UTF8_ERROR_OVERLONG,
  UTF8_ERROR_INTERRUPTED_1 = 0x40,
  UTF8_ERROR_INTERRUPTED_2 = 0x80,
  UTF8_ERROR_INTERRUPTED_3 = 0xC0
} UTF8Error, * UTF8Error_p;

/** Decode one Unicode® code point from a UTF-8 byte buffer.
 *  Assumes `end` > `cursor`.
 * @param[in] cursor current byte.
 * @param[in] end byte buffer end.
 * @param[out] codepoint decoded 21-bit code point.
 * @param[out] err_p error code variable.
 */
static inline const uint8_t*
decode_utf8(const uint8_t* cursor, const uint8_t* end, uint32_t* codepoint, UTF8Error_p err_p)
{
  uint8_t c = *cursor;
  uint32_t u;
  uint8_t len = 0;
  if      (0x00 is (c & 0x80)) { *codepoint = (uint32_t)(c); return cursor+1; }
  //if      (0x00 is (c & 0x80)) { u = (uint32_t)(c       ); len = 1; }
  else if (0xC0 is (c & 0xE0)) { u = (uint32_t)(c & 0x1F); len = 2; }
  else if (0xE0 is (c & 0xF0)) { u = (uint32_t)(c & 0x0F); len = 3; }
  else if (0xF0 is (c & 0xF8)) { u = (uint32_t)(c & 0x07); len = 4; }
  if (len is 0 or cursor + len > end) {
    *err_p = UTF8_ERROR;
    return cursor;
  }
  switch (len) {
    case 4: c = *(++cursor)^0x80; u = (u<<6)|c; if (c&0xC0) *err_p = (c&0xC0);
    case 3: c = *(++cursor)^0x80; u = (u<<6)|c; if (c&0xC0) *err_p = (c&0xC0);
    case 2: c = *(++cursor)^0x80; u = (u<<6)|c; if (c&0xC0) *err_p = (c&0xC0);
    case 1:      (++cursor);
  }
  if (len is 2 and u <    0x80 or
      len is 3 and u <  0x0800 or
      len is 4 and u < 0x10000) {
    *err_p = UTF8_ERROR_OVERLONG;
  }
  *codepoint = u;

  return cursor;
}

/** Decode one Unicode® code point from a UTF-8 byte buffer
 * without checkding for errors which makes it faster.
 *  Assumes `end` > `cursor`.
 * @param[in] cursor current byte.
 * @param[in] end byte buffer end.
 * @param[out] codepoint decoded 21-bit code point.
 */
static inline const uint8_t*
decode_utf8_unchecked(const uint8_t* cursor, const uint8_t* end, uint32_t* codepoint)
{
  uint8_t c = *cursor;
  uint32_t u;
  uint8_t len = 0;
  if      (0x00 is (c & 0x80)) { *codepoint = (uint32_t)(c); return cursor+1; }
  //if      (0x00 is (c & 0x80)) { u = (uint32_t)(c       ); len = 1; }
  else if (0xC0 is (c & 0xE0)) { u = (uint32_t)(c & 0x1F); len = 2; }
  else if (0xE0 is (c & 0xF0)) { u = (uint32_t)(c & 0x0F); len = 3; }
  else if (0xF0 is (c & 0xF8)) { u = (uint32_t)(c & 0x07); len = 4; }
  switch (len) {
    case 4: c = *(++cursor)^0x80; u = (u<<6)|c;
    case 3: c = *(++cursor)^0x80; u = (u<<6)|c;
    case 2: c = *(++cursor)^0x80; u = (u<<6)|c;
    case 1:      (++cursor);
  }
  *codepoint = u;

  return cursor;
}

/** Compute the length in Unicode® code points for a UTF-8 byte buffer.
 *  Assumes `end` > `cursor`.
 * @param[in] start byte buffer slice start.
 * @param[in] end byte buffer slice end.
 * @param[out] err_p error code variable.
 * Returns the length in code points decoded from UTF-8 bytes.
 */
static inline const size_t
len_utf8(const uint8_t * const start, const uint8_t * const end,
         UTF8Error_p err_p)
{
  size_t len = 0;
  const uint8_t* cursor = start;
  while (cursor is_not end) {
    uint8_t c = *cursor;
    if      (0x00 is (c & 0x80)) { cursor += 1; }
    else if (0xC0 is (c & 0xE0)) { cursor += 2; }
    else if (0xE0 is (c & 0xF0)) { cursor += 3; }
    else if (0xF0 is (c & 0xF8)) { cursor += 4; }
    else cursor = end + 1;
    if (cursor > end) {
      *err_p = UTF8_ERROR;
      break;
    }
    ++len;
  }
  return len;
}
