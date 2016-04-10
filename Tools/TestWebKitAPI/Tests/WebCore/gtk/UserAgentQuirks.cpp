/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/URL.h>
#include <WebCore/UserAgentGtk.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(WebCore, UserAgentQuirksTest)
{
    // A site with not quirks should return a null String.
    String uaString = standardUserAgentForURL(URL(ParsedURLString, "http://www.webkit.org/"));
    EXPECT_TRUE(uaString.isNull());

    // www.yahoo.com requires MAC OS platform in the UA.
    uaString = standardUserAgentForURL(URL(ParsedURLString, "http://www.yahoo.com/"));
    EXPECT_TRUE(uaString.contains("Macintosh"));
    EXPECT_TRUE(uaString.contains("Mac OS X"));
    EXPECT_FALSE(uaString.contains("Linux"));

    // For Google domains we always return the standard UA.
    uaString = standardUserAgent();
    EXPECT_FALSE(uaString.isNull());
    EXPECT_TRUE(uaString == standardUserAgentForURL(URL(ParsedURLString, "http://www.google.com/")));
    EXPECT_TRUE(uaString == standardUserAgentForURL(URL(ParsedURLString, "http://calendar.google.com/")));
    EXPECT_TRUE(uaString == standardUserAgentForURL(URL(ParsedURLString, "http://gmail.com/")));
    EXPECT_TRUE(uaString == standardUserAgentForURL(URL(ParsedURLString, "http://www.google.com.br/")));
    EXPECT_TRUE(uaString == standardUserAgentForURL(URL(ParsedURLString, "http://www.youtube.com/")));
    EXPECT_TRUE(uaString != standardUserAgentForURL(URL(ParsedURLString, "http://www.google.uk.not.com.br/")));

    // For Google Maps we remove the Version/8.0 string.
    uaString = standardUserAgentForURL(URL(ParsedURLString, "http://maps.google.com/"));
    EXPECT_TRUE(uaString == standardUserAgentForURL(URL(ParsedURLString, "http://www.google.com/maps/")));
    EXPECT_FALSE(uaString.contains("Version/8.0 Safari/"));
}

} // namespace TestWebKitAPI
