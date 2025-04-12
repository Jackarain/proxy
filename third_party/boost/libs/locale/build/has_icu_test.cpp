// Copyright (c) 2010 John Maddock
// Copyright (c) 2022-2024 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <unicode/coll.h>
#include <unicode/locid.h>
#include <unicode/numfmt.h>
#include <unicode/stringpiece.h>
#include <unicode/uchar.h>
#include <unicode/utypes.h>
#include <unicode/uversion.h>

// Sanity check to avoid false negatives for version checks
#ifndef U_ICU_VERSION_MAJOR_NUM
#    error "U_ICU_VERSION_MAJOR_NUM is missing"
#endif
#ifndef U_ICU_VERSION_MINOR_NUM
#    error "U_ICU_VERSION_MINOR_NUM is missing"
#endif
#ifndef U_ICU_VERSION_PATCHLEVEL_NUM
#    error "U_ICU_VERSION_PATCHLEVEL_NUM is missing"
#endif

#define BOOST_LOCALE_MAKE_VERSION(maj, min, patch) (((maj)*100 + (min)) * 100 + patch)
#define BOOST_LOCALE_ICU_VERSION \
    BOOST_LOCALE_MAKE_VERSION(U_ICU_VERSION_MAJOR_NUM, U_ICU_VERSION_MAJOR_NUM, U_ICU_VERSION_MINOR_NUM)

#if BOOST_LOCALE_ICU_VERSION < BOOST_LOCALE_MAKE_VERSION(4, 8, 1)
// 4.8.0 fails parsing "GMT" as a full time zone if followed by unrelated text
#    error "ICU 4.8.1 or higher is required"
#endif
#if BOOST_LOCALE_ICU_VERSION == BOOST_LOCALE_MAKE_VERSION(50, 1, 0)
// https://unicode-org.atlassian.net/browse/ICU-9780
#    error "ICU 50.1.0 has a bug with integer parsing and cannot be used reliably"
#endif

int main()
{
    icu::Locale loc;
    UErrorCode err = U_ZERO_ERROR;
    UChar32 c = ::u_charFromName(U_UNICODE_CHAR_NAME, "GREEK SMALL LETTER ALPHA", &err);
    delete icu::NumberFormat::createInstance(loc, UNUM_CURRENCY_ISO, err);
    icu::StringPiece sp;
    return U_SUCCESS(err) && sp.empty() && (c != 0u);
}
