/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * From yuma123-2.2.5/netconf/doc/extra/COPYRIGHT:
 *
 * This package was debianized by Andy Bierman <andy@yumaworks.com>
 * Thu, 09 Aug 2012 12:20:00 -0800
 *
 * It was downloaded from <http://www.yumaworks.com>
 *
 * Upstream Author: Andy Bierman <andy@yumaworks.com>
 *
 * Copyright:
 *     Copyright (C) 2008 - 2012 by Andy Bierman <andy@yumaworks.com>
 *     Copyright (C) 2012 by YumaWorks, Inc. <support@yumaworks.com>
 *
 * License: BSD
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL Andy Bierman BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  See the file `yuma-legal-notices.pdf', installed in  the '/usr/share/doc/yuma'
 *  directory for additional license information pertaining to external packages
 *  used by some Yuma programs.
 *
 * Packaging:
 *     Copyright (C) 2012, YumaWorks, Inc. <support@yumaworks.com>
 *     released under the BSD license.
 */
/*
 * From yuma123-2.2.5/netconf/doc/extra/yuma-legal-notices.pdf:
 *
 * The base64 content transfer encoding functions in ncx/b64.c are by Bob Trower, under the MIT license.
 * MODULE NAME: b64.c
 * AUTHOR: Bob Trower 08/04/01
 * PROJECT: Crypt Data Packaging
 * COPYRIGHT: Copyright (c) Trantor Standard Systems Inc., 2001
 *
 * LICENCE:
 * Copyright (c) 2001 Bob Trower, Trantor Standard Systems Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


/** translation Table used for encoding characters */
static const char pbc_encodeCharacterTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "0123456789+/";

/** translation Table used for decoding characters */
static const char pbc_decodeCharacterTable[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

// ---------------------------------------------------------------------------|
/**
 * Determine if a character is a valid base64 value.
 *
 * \param c the character to test.
 * \return true if c is a base64 value.
 */
static bool pbc_is_base64(
  unsigned char c)
{
   return (isalnum(c) || (c == '+') || (c == '/'));
}

// ---------------------------------------------------------------------------|
/**
 * Count the number of valid base64 characters in the supplied input
 * buffer.
 * @desc this function counts the number of valid base64 characters,
 * skipping CR and LF characters. Invalid characters are treated as
 * end of buffer markers.
 *
 * \param inbuff the buffer to check.
 * \param inputlen the length of the input buffer.
 * \return the number of valid base64 characters.
 */
static uint32_t pbc_get_num_valid_characters(
  const uint8_t* inbuff,
  size_t inputlen )
{
    uint32_t validCharCount=0;
    uint32_t idx=0;

    while ( idx < inputlen )
    {
        if ( pbc_is_base64( inbuff[idx] ) )
        {
            ++validCharCount;
            ++idx;
        }
        else if ( inbuff[idx] == '\r' || inbuff[idx] == '\n' )
        {
            ++idx;
        }
        else
        {
            break;
        }
    }
    return validCharCount;
}
// ---------------------------------------------------------------------------|
/**
 * Decode the 4 byte array pf base64 values into 3 bytes.
 *
 * \param inbuff the buffer to decode
 * \param outbuff the output buffer for decoded bytes.
 */
static void pbc_decode4Bytes(
  const uint8_t inbuff[4],
  uint8_t* outbuff)
{
    outbuff[0] = (pbc_decodeCharacterTable[ inbuff[0] ] << 2) +
                 ((pbc_decodeCharacterTable[ inbuff[1] ] & 0x30) >> 4);
    outbuff[1] = ((pbc_decodeCharacterTable[ inbuff[1] ] & 0xf) << 4) +
                 ((pbc_decodeCharacterTable[ inbuff[2] ] & 0x3c) >> 2);
    outbuff[2] = ((pbc_decodeCharacterTable[ inbuff[2] ] & 0x3) << 6) +
                 pbc_decodeCharacterTable[ inbuff[3] ];
}

// ---------------------------------------------------------------------------|
/**
 * Extract up to 4 bytes of base64 data to decode.
 * this function skips CR anf LF characters and extracts up to 4 bytes
 * from the input buffer. Any invalid characters are treated as end of
 * buffer markers.
 *
 * \param iterPos the start of the data to decode. This value is updated.
 * \param endpos the end of the buffer marker.
 * \param arr4 the output buffer for extracted bytes (zero padded if
 * less than 4 bytes were extracted)
 * \param numExtracted the number of valid bytes that were extracted.
 * output buffer for extracted bytes.
 * \return true if the end of buffer was reached.
 */
static bool pbc_extract_4bytes(
  const uint8_t** iterPos,
  const uint8_t* endPos,
  uint8_t arr4[4],
  uint32_t* numExtracted)
{
    const uint8_t* iter=*iterPos;
    uint32_t validBytes = 0;
    bool endReached = false;

    while ( iter < endPos && validBytes<4 && !endReached )
    {
        if ( pbc_is_base64( *iter ) )
        {
            arr4[validBytes++] = *iter;
            ++iter;
        }
        else if ( *iter == '\r' || *iter == '\n' )
        {
            ++iter;
        }
        else
        {
            // encountered a dodgy character or an =
            if ( *iter != '=' )
            {
               //log_warn(instance,  "pbc_b64_decode() encountered invalid character(%c), "
               //          "output string truncated!", *iter );
            }

            // pad the remaining characters to decode
            size_t pad = validBytes;
            for ( ; pad<4; ++pad )
            {
                arr4[pad] = 0;
            }
            endReached = true;
        }
    }

    *numExtracted = validBytes;
    *iterPos = iter;
    return endReached ? endReached : iter >= endPos;
}

// ---------------------------------------------------------------------------|
static bool pbc_extract_3bytes(
  const uint8_t** iterPos,
  const uint8_t* endPos,
  uint8_t arr3[3],
  uint32_t* numExtracted)
{
    const uint8_t* iter=*iterPos;
    uint32_t byteCount = 0;

    while ( iter < endPos && byteCount < 3 )
    {
        arr3[ byteCount++ ] = *iter;
        ++iter;
    }

    *numExtracted = byteCount;
    while ( byteCount < 3 )
    {
        arr3[byteCount++] = 0;
    }

    *iterPos = iter;
    return iter >= endPos;
}


// ---------------------------------------------------------------------------|
static bool pbc_encode_3bytes(
  const uint8_t** iterPos,
  const uint8_t* endPos,
  uint8_t** outPos)
{
    uint8_t arr3[3];
    uint32_t numBytes;

    bool endReached = pbc_extract_3bytes( iterPos, endPos, arr3, &numBytes );

    if ( numBytes )
    {
        uint8_t* outArr = *outPos;
        outArr[0] = pbc_encodeCharacterTable[ ( arr3[0] & 0xfc) >> 2 ];
        outArr[1] = pbc_encodeCharacterTable[ ( ( arr3[0] & 0x03 ) << 4 ) +
                                          ( ( arr3[1] & 0xf0 ) >> 4 ) ];
        outArr[2] = pbc_encodeCharacterTable[ ( ( arr3[1] & 0x0f ) << 2 ) +
                                          ( ( arr3[2] & 0xc0 ) >> 6 ) ];
        outArr[3] = pbc_encodeCharacterTable[ arr3[2] & 0x3f ];

        // pad with '=' characters
        while ( numBytes < 3 )
        {
            outArr[numBytes+1] = '=';
            ++numBytes;
        }
        *outPos += 4;
    }

    return endReached;
}

/*************** E X T E R N A L    F U N C T I O N S  *************/

// ---------------------------------------------------------------------------|
uint32_t pbc_b64_get_encoded_str_len(
  uint32_t inbufflen,
  uint32_t linesize)
{
    uint32_t requiredBufLen = inbufflen%3 ? 4 * ( 1 + inbufflen/3 )
                                          : 4 * ( inbufflen/3 );
    if(linesize != 0) {
        requiredBufLen += 2 * ( inbufflen/linesize );  // allow for line breaks
        requiredBufLen += 2; // allow for final CR-LF
    }
    requiredBufLen += 1; //null terminator
    return requiredBufLen;
}

// ---------------------------------------------------------------------------|
uint32_t pbc_b64_get_decoded_str_len(
  const uint8_t* inbuff,
  size_t inputlen)
{
    uint32_t validCharCount= pbc_get_num_valid_characters( inbuff, inputlen );

    uint32_t requiredBufLen = 3*(validCharCount/4);
    uint32_t rem=validCharCount%4;
    if ( rem )
    {
        requiredBufLen += rem-1;
    }

    return requiredBufLen;
}

// ---------------------------------------------------------------------------|
protobuf_c_boolean pbc_b64_encode(
  const uint8_t* inbuff,
  uint32_t inbufflen,
  uint8_t* outbuff,
  uint32_t outbufflen,
  uint32_t linesize,
  uint32_t* retlen)
{
    assert( inbuff && "pbc_b64_decode() inbuff is NULL!" );
    assert( outbuff && "pbc_b64_decode() outbuff is NULL!" );
    assert((linesize>4) || (linesize==0));
    if ( pbc_b64_get_encoded_str_len( inbufflen, linesize ) > outbufflen )
    {
        return FALSE;
    }

    bool endReached = false;
    const uint8_t* endPos = inbuff+inbufflen;
    const uint8_t* iter = inbuff;
    uint8_t* outIter = outbuff;
    uint32_t numBlocks=0;
    int line_index=0;

    while( !endReached )
    {
        endReached = pbc_encode_3bytes( &iter, endPos, &outIter );
        ++numBlocks;
        if(linesize==0) {
            continue;
        }
        if ( (numBlocks*4) >= ((line_index+1)*linesize) )
        {
            int new_lined_bytes;
            new_lined_bytes = (numBlocks*4)% linesize;

            if(new_lined_bytes) {
            	memmove(outIter-new_lined_bytes+2, outIter-new_lined_bytes, new_lined_bytes);
            }
            *(outIter-new_lined_bytes+0)='\r';
            *(outIter-new_lined_bytes+1)='\n';
            *outIter+=2;
            line_index++;
        }
    }
    if(linesize!=0) {
        *outIter++ ='\r';
        *outIter++ ='\n';
    }

    *retlen = outIter - outbuff;
    *outIter++ ='\0';
    return TRUE;
}

// ---------------------------------------------------------------------------|
protobuf_c_boolean pbc_b64_decode(
  const uint8_t* inbuff,
  uint32_t inbufflen,
  uint8_t* outbuff,
  uint32_t outbufflen,
  uint32_t* retlen)
{
    uint8_t arr4[4];
    uint32_t numExtracted = 0;
    bool endReached = false;
    const uint8_t* endPos = inbuff+inbufflen;
    const uint8_t* iter = inbuff;

    assert( inbuff && "pbc_b64_decode() inbuff is NULL!" );
    assert( outbuff && "pbc_b64_decode() outbuff is NULL!" );

    *retlen=0;

    while ( !endReached )
    {
        endReached = pbc_extract_4bytes( &iter, endPos, arr4, &numExtracted );

        if ( numExtracted )
        {
            if ( *retlen+3 > outbufflen )
            {
                return FALSE;
            }
            pbc_decode4Bytes( arr4, outbuff+*retlen );
            *retlen += numExtracted-1;
        }
    }

    return TRUE;
}

/* END yuma_b64.c */
