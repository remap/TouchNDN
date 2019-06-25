/**
 * Copyright (C) 2019 Regents of the University of California.
 * @author: Peter Gusev <peter@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

#include <stdio.h>
#include <string>

#include <ndn-cpp/util/blob.hpp>

#include "baseDAT.hpp"
#include "contrib/apache/apr_base64.h"

using namespace std;
using namespace touch_ndn;

BaseDAT::BaseDAT(const OP_NodeInfo* info)
: BaseOp<DAT_CPlusPlusBase>(info)
{
}

BaseDAT::~BaseDAT()
{
    
}

void
BaseDAT::getGeneralInfo(DAT_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1)
{
    ginfo->cookEveryFrame = false;
    ginfo->cookEveryFrameIfAsked = true;
}

std::string
BaseDAT::toBase64(const ndn::Blob &blob)
{
    vector<char> output(::apr_base64_encode_len((int)blob.size()));
    size_t outputLength = (size_t)::apr_base64_encode_binary(&output[0], blob.buf(), (int)blob.size()) - 1;
    
    return string(output.begin(), output.begin()+outputLength);
}

// borrowed from
// https://github.com/named-data/ndn-cpp/blob/a319d02f7c7c9480202a0bcee7c56f7eb5230e58/src/encoding/base64.cpp#L58
void
BaseDAT::fromBase64(const std::string &input, std::vector<uint8_t> &output)
{
    // We are only concerned with whitespace characters which are all less than
    // the first base64 character '+'. If we find whitespace, then we'll copy
    // non-whitespace to noWhitespaceStream.
    ostringstream noWhitespaceStream;
    bool gotWhitespace = false;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] < '+') {
            if (!gotWhitespace) {
                // We need to use the noWitespaceStream. Initialize it.
                gotWhitespace = true;
                noWhitespaceStream.write(&input[0], i);
            }
        }
        else {
            if (gotWhitespace)
                noWhitespaceStream << input[i];
        }
    }
    
    string noWhitespace;
    const char* inputCString;
    if (gotWhitespace) {
        noWhitespace = noWhitespaceStream.str();
        inputCString = noWhitespace.c_str();
    }
    else
        // The input didn't have any whitespace, so use it as is.
        inputCString = input.c_str();
    
    output.resize(::apr_base64_decode_len(inputCString));
    size_t outputLength = (size_t)::apr_base64_decode_binary
    (&output[0], inputCString);
    output.resize(outputLength);
}
