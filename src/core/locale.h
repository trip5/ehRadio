#ifndef _locale_h
#define _locale_h
/*******************************************************
 * Locale / Language Selection
 * 
 * Sets L10N_INCLUDE macro to point to the appropriate
 * locale file, DSP_LOCALE for language code,
 * and L10N_CP_xxx codepage flags.
 * 
 * The actual locale file is included at the end inside
 * namespace LANG, after all other macros are defined.
 ********************************************************/

//==================================================
// #define DSP_LANGUAGE_xx_XX defines the display locale
// Other locale settings are auto-selected but can be
// changed with override defines.
//
// L10N_INCLUDE : display language file
// DSP_LOCALE : locale abbreviation of display language
// L10N_CP_LATIN or L10N_CP_CYRILLIC select font for display
// WEATHER_LANG : used by OpenWeather API
// WEBUI_LOCALE : selects .json for localized WebUI
//                default is same as DSP_LOCALE
//                (WebUI can support more languages than Display)
//==================================================

/* Guard against multiple codepages */
#if defined(L10N_CP_CYRILLIC) && defined(L10N_CP_LATIN)
  #error define error in myoptions.h: L10N_CP_CYRILLIC and L10N_CP_LATIN cannot both be defined
#endif

#if __has_include("../locale/displayL10n_custom.h")
  #define L10N_INCLUDE "../locale/displayL10n_custom.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "custom"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"
  #endif
#elif defined(DSP_LANGUAGE_be_BY)
  #define L10N_INCLUDE "../locale/displayL10n_be_BY.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "be_BY"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "be"
  #endif
#elif defined(DSP_LANGUAGE_bg_BG)
  #define L10N_INCLUDE "../locale/displayL10n_bg_BG.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "bg_BG"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "bg"
  #endif
#elif defined(DSP_LANGUAGE_bs_BA)
  #define L10N_INCLUDE "../locale/displayL10n_bs_BA.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "bs_BA"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "hr"  /* Bosnian not available, using Croatian */
  #endif
#elif defined(DSP_LANGUAGE_cs_CZ)
  #define L10N_INCLUDE "../locale/displayL10n_cs_CZ.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "cs_CZ"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "cz"
  #endif
#elif defined(DSP_LANGUAGE_da_DK)
  #define L10N_INCLUDE "../locale/displayL10n_da_DK.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "da_DK"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "da"
  #endif
#elif defined(DSP_LANGUAGE_de_DE)
  #define L10N_INCLUDE "../locale/displayL10n_de_DE.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "de_DE"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "de"
  #endif
#elif defined(DSP_LANGUAGE_el_GR)
  #define L10N_INCLUDE "../locale/displayL10n_el_GR.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "el_GR"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "el"
  #endif
#elif defined(DSP_LANGUAGE_en_US)
  #define L10N_INCLUDE "../locale/displayL10n_en_US.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "en_US"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"
  #endif
#elif defined(DSP_LANGUAGE_es_ES)
  #define L10N_INCLUDE "../locale/displayL10n_es_ES.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "es_ES"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "es"
  #endif
#elif defined(DSP_LANGUAGE_et_EE)
  #define L10N_INCLUDE "../locale/displayL10n_et_EE.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "et_EE"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"  /* Estonian not available */
  #endif
#elif defined(DSP_LANGUAGE_fi_FI)
  #define L10N_INCLUDE "../locale/displayL10n_fi_FI.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "fi_FI"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "fi"
  #endif
#elif defined(DSP_LANGUAGE_fr_FR)
  #define L10N_INCLUDE "../locale/displayL10n_fr_FR.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "fr_FR"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "fr"
  #endif
#elif defined(DSP_LANGUAGE_hr_HR)
  #define L10N_INCLUDE "../locale/displayL10n_hr_HR.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "hr_HR"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "hr"
  #endif
#elif defined(DSP_LANGUAGE_hu_HU)
  #define L10N_INCLUDE "../locale/displayL10n_hu_HU.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "hu_HU"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "hu"
  #endif
#elif defined(DSP_LANGUAGE_is_IS)
  #define L10N_INCLUDE "../locale/displayL10n_is_IS.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "is_IS"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "is"
  #endif
#elif defined(DSP_LANGUAGE_kk_KZ)
  #define L10N_INCLUDE "../locale/displayL10n_kk_KZ.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "kk_KZ"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"  /* Kazakh not available */
  #endif
#elif defined(DSP_LANGUAGE_ky_KG)
  #define L10N_INCLUDE "../locale/displayL10n_ky_KG.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "ky_KG"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"  /* Kyrgyz not available */
  #endif
#elif defined(DSP_LANGUAGE_lt_LT)
  #define L10N_INCLUDE "../locale/displayL10n_lt_LT.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "lt_LT"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "lt"
  #endif
#elif defined(DSP_LANGUAGE_lv_LV)
  #define L10N_INCLUDE "../locale/displayL10n_lv_LV.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "lv_LV"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "la"
  #endif
#elif defined(DSP_LANGUAGE_me_ME)
  #define L10N_INCLUDE "../locale/displayL10n_me_ME.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "me_ME"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "sr"  /* Montenegrin not available, using Serbian */
  #endif
#elif defined(DSP_LANGUAGE_mk_MK)
  #define L10N_INCLUDE "../locale/displayL10n_mk_MK.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "mk_MK"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "mk"
  #endif
#elif defined(DSP_LANGUAGE_mn_MN)
  #define L10N_INCLUDE "../locale/displayL10n_mn_MN.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "mn_MN"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"  /* Mongolian not available */
  #endif
#elif defined(DSP_LANGUAGE_nl_NL)
  #define L10N_INCLUDE "../locale/displayL10n_nl_NL.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "nl_NL"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "nl"
  #endif
#elif defined(DSP_LANGUAGE_no_NO)
  #define L10N_INCLUDE "../locale/displayL10n_no_NO.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "no_NO"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "no"
  #endif
#elif defined(DSP_LANGUAGE_pl_PL)
  #define L10N_INCLUDE "../locale/displayL10n_pl_PL.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "pl_PL"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "pl"
  #endif
#elif defined(DSP_LANGUAGE_pt_PT)
  #define L10N_INCLUDE "../locale/displayL10n_pt_PT.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "pt_PT"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "pt"
  #endif
#elif defined(DSP_LANGUAGE_ro_RO)
  #define L10N_INCLUDE "../locale/displayL10n_ro_RO.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "ro_RO"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "ro"
  #endif
#elif defined(DSP_LANGUAGE_ru_RU)
  #define L10N_INCLUDE "../locale/displayL10n_ru_RU.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "ru_RU"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "ru"
  #endif
#elif defined(DSP_LANGUAGE_sk_SK)
  #define L10N_INCLUDE "../locale/displayL10n_sk_SK.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "sk_SK"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "sk"
  #endif
#elif defined(DSP_LANGUAGE_sl_SI)
  #define L10N_INCLUDE "../locale/displayL10n_sl_SI.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "sl_SI"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "sl"
  #endif
#elif defined(DSP_LANGUAGE_sr_RS)
  #define L10N_INCLUDE "../locale/displayL10n_sr_RS.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "sr_RS"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "sr"
  #endif
#elif defined(DSP_LANGUAGE_sv_SE)
  #define L10N_INCLUDE "../locale/displayL10n_sv_SE.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "sv_SE"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "sv"
  #endif
#elif defined(DSP_LANGUAGE_tg_TJ)
  #define L10N_INCLUDE "../locale/displayL10n_tg_TJ.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "tg_TJ"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"  /* Tajik not available */
  #endif
#elif defined(DSP_LANGUAGE_tr_TR)
  #define L10N_INCLUDE "../locale/displayL10n_tr_TR.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "tr_TR"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "tr"
  #endif
#elif defined(DSP_LANGUAGE_uk_UA)
  #define L10N_INCLUDE "../locale/displayL10n_uk_UA.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "uk_UA"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "uk"
  #endif
#elif defined(DSP_LANGUAGE_uz_UZ)
  #define L10N_INCLUDE "../locale/displayL10n_uz_UZ.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "uz_UZ"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_CYRILLIC
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"  /* Uzbek not available */
  #endif
#else  // default to en_US
  #define L10N_INCLUDE "../locale/displayL10n_en_US.h"
  #ifndef DSP_LOCALE
    #define DSP_LOCALE "en_US"
  #endif
  #if !defined(L10N_CP_CYRILLIC) && !defined(L10N_CP_LATIN)
    #define L10N_CP_LATIN
  #endif
  #ifndef WEATHER_LANG
    #define WEATHER_LANG "en"
  #endif
#endif

/* Default is to use the same language in the WebUI as the display */
/* This can be over-ridden in myoptions.h with something like #define WEBUI_LOCALE "de_DE" */
#ifndef WEBUI_LOCALE
  #define WEBUI_LOCALE DSP_LOCALE
#endif

/* Is the hardcoded text in the HTML files not English? */
/* If yes, then you should override this */
/* Over-ride with extreme caution!! */
/* You must prepare the HTML files with hardcode_locale_to_html.py */
#ifndef HARDCODED_WEBUI_LOCALE
  #define HARDCODED_WEBUI_LOCALE "en_US"
#endif

//==================================================
// Include the selected locale file inside LANG namespace
// This happens AFTER all other macros (like UPDATEURL) are defined
//==================================================
namespace LANG{
  #include L10N_INCLUDE
}

#endif // _locale_h


